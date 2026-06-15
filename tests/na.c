/*
 * na.c Named after TWICE's Nayeon and definitely not GNU's nano text editor.
 * Small VT-100 text editor for CP/M 2.2, converted from the uploaded
 * Linux/C++ single-file editor.
 *
 * Strict C89, fixed 80x24 screen, small in-memory files, CP/M-ish keys:
 *   ^E Up    ^X Down   ^S Left  ^D Right
 *   ^R PgUp  ^C PgDn
 *   ^F Find
 *   ^W Save  ^A Save As
 *   ^T Cut line   ^Y Copy line   ^V Paste below current line
 *   ^Z Exit
 *
 * Arrow keys and VT-100 PgUp/PgDn/Home/End are also recognized if the
 * console sends escape sequences.  A lone ESC is not a command.
 *
 * This version intentionally avoids termios, ioctl, Unix fd I/O, fsync,
 * rename-safe writes, C++, dynamic vectors, and time-based status expiry.
 *
 * FIXED AUTOWRAP VERSION 3: absolute VT100 positioning for every row; no CR/LF scrolling during redraw.
 * Console input uses getch() (no echo, blocking) and kbhit() (non-blocking
 * poll) from stdio.h, which map to CP/M BDOS 6/11 respectively.
 *
 * Drawing deliberately leaves column 80 blank.  Some terminals/emulators
 * autowrap as soon as column 80 is written, before the following ESC[K or
 * CR/LF is processed.  Restricting visible writes to columns 1..79 avoids
 * bottom-line corruption and cursor displacement.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#define SCREEN_ROWS 24
#define SCREEN_COLS 80
#define DRAW_COLS   (SCREEN_COLS - 1)
#define TEXT_ROWS   (SCREEN_ROWS - 2)

#define LINES_INIT  16
#define MAX_LINE    254
#define STATUS_LEN  96
#define NAME_LEN    16
#define TAB_STOP    8

#define KEY_NULL       0
#define KEY_CTRL_A     1
#define KEY_CTRL_C     3
#define KEY_CTRL_D     4
#define KEY_CTRL_E     5
#define KEY_CTRL_F     6
#define KEY_CTRL_H     8
#define KEY_TAB        9
#define KEY_ENTER     13
#define KEY_CTRL_R    18
#define KEY_CTRL_S    19
#define KEY_CTRL_T    20
#define KEY_CTRL_V    22
#define KEY_CTRL_W    23
#define KEY_CTRL_X    24
#define KEY_CTRL_Y    25
#define KEY_CTRL_Z    26
#define KEY_ESC       27
#define KEY_BACKSPACE 127

#define KEY_DEL       1000
#define KEY_HOME      1001
#define KEY_END       1002
#define KEY_PGUP      1003
#define KEY_PGDN      1004
#define KEY_UP        1005
#define KEY_DOWN      1006
#define KEY_LEFT      1007
#define KEY_RIGHT     1008
#define KEY_CTRL_PGUP 1009
#define KEY_CTRL_PGDN 1010

struct editor {
    char filename[NAME_LEN];
    char **line;
    int nlines;
    int line_cap;
    int cx;
    int cy;
    int rowoff;
    int coloff;
    int dirty;
    char status[STATUS_LEN];
    char **cut;
    int ncut;
    int cut_cap;
    int last_was_cut;
    int crlf;
    int redraw;     /* 0=cursor+status, 1=current row, 2=full */
};

static struct editor E;

static int clampi(v, lo, hi)
int v;
int lo;
int hi;
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void outstr(s)
const char *s;
{
    fputs(s, stdout);
}

static void vt_clear()
{
    outstr("\033[0m");
    outstr("\033[2J");
    outstr("\033[1;1H");
    fflush(stdout);
}

static void die(s)
const char *s;
{
    vt_clear();
    puts(s);
    exit(1);
}

static void *xmalloc(n)
unsigned n;
{
    void *p;
    p = malloc(n ? n : 1);
    if (p == NULL) die("out of memory");
    return p;
}

static char *xstrdup(s)
const char *s;
{
    char *p;
    unsigned n;
    n = (unsigned)strlen(s) + 1;
    p = (char *)xmalloc(n);
    memcpy(p, s, n);
    return p;
}

static void free_line(p)
char *p;
{
    if (p != NULL) free(p);
}

static void grow_lines(need)
int need;
{
    char **nw;
    int new_cap;
    int i;
    new_cap = E.line_cap ? E.line_cap * 2 : LINES_INIT;
    if (new_cap < need) new_cap = need;
    nw = (char **)xmalloc((unsigned)new_cap * sizeof(char *));
    for (i = 0; i < E.nlines; i++) nw[i] = E.line[i];
    free(E.line);
    E.line = nw;
    E.line_cap = new_cap;
}

static void grow_cut(need)
int need;
{
    char **nw;
    int new_cap;
    int i;
    new_cap = E.cut_cap ? E.cut_cap * 2 : 8;
    if (new_cap < need) new_cap = need;
    nw = (char **)xmalloc((unsigned)new_cap * sizeof(char *));
    for (i = 0; i < E.ncut; i++) nw[i] = E.cut[i];
    free(E.cut);
    E.cut = nw;
    E.cut_cap = new_cap;
}

static char *make_line(s, len)
const char *s;
int len;
{
    char *p;
    if (len < 0) len = 0;
    if (len > MAX_LINE) len = MAX_LINE;
    p = (char *)xmalloc((unsigned)len + 1);
    if (len > 0) memcpy(p, s, (unsigned)len);
    p[len] = '\0';
    return p;
}

static void set_status(fmt, a, b)
const char *fmt;
const char *a;
int b;
{
    /* Small substitute for snprintf.  Existing dcc has printf-family work,
       but avoid relying on vsnprintf. */
    char tmp[STATUS_LEN + 40];
    if (a == NULL) a = "";
    sprintf(tmp, fmt, a, b);
    strncpy(E.status, tmp, STATUS_LEN - 1);
    E.status[STATUS_LEN - 1] = '\0';
}

static void set_status0(s)
const char *s;
{
    strncpy(E.status, s, STATUS_LEN - 1);
    E.status[STATUS_LEN - 1] = '\0';
}

static void init_editor()
{
    memset(&E, 0, sizeof(E));
    E.crlf = 1;               /* CP/M text files default to CRLF. */
    E.line_cap = LINES_INIT;
    E.line = (char **)xmalloc((unsigned)LINES_INIT * sizeof(char *));
    E.line[0] = xstrdup("");
    E.nlines = 1;
    E.redraw = 2;
}


static void clear_lines()
{
    int i;
    for (i = 0; i < E.nlines; i++) {
        free_line(E.line[i]);
        E.line[i] = NULL;
    }
    E.nlines = 0;
    E.cx = 0;
    E.cy = 0;
    E.rowoff = 0;
    E.coloff = 0;
}

static void free_cutbuf()
{
    int i;
    for (i = 0; i < E.ncut; i++) {
        free_line(E.cut[i]);
        E.cut[i] = NULL;
    }
    free(E.cut);
    E.cut = NULL;
    E.ncut = 0;
    E.cut_cap = 0;
}

static void ensure_cursor()
{
    int len;
    if (E.nlines <= 0) {
        if (E.line_cap < 1) grow_lines(1);
        E.line[0] = xstrdup("");
        E.nlines = 1;
    }
    E.cy = clampi(E.cy, 0, E.nlines - 1);
    len = (int)strlen(E.line[E.cy]);
    E.cx = clampi(E.cx, 0, len);
}

static int visual_col(y, x)
int y;
int x;
{
    int i;
    int v;
    char *s;
    if (y < 0 || y >= E.nlines) return 0;
    s = E.line[y];
    x = clampi(x, 0, (int)strlen(s));
    v = 0;
    for (i = 0; i < x; i++) {
        if (s[i] == '\t') v += TAB_STOP - (v % TAB_STOP);
        else v++;
    }
    return v;
}

static int read_key()
{
    int c;
    int a;
    int b;
    int term;

    c = getch();
    if (c != KEY_ESC) return c;

    /* Use kbhit() to distinguish a lone ESC from the start of a sequence.
       Escape sequence bytes arrive together so kbhit() is reliable here. */
    if (!kbhit()) return KEY_ESC;
    c = getch();
    if (c == '[') {
        c = getch();
        if (c >= '0' && c <= '9') {
            a = 0;
            b = 0;
            while (c >= '0' && c <= '9') {
                a = a * 10 + c - '0';
                c = getch();
            }
            if (c == ';') {
                c = getch();
                while (c >= '0' && c <= '9') {
                    b = b * 10 + c - '0';
                    c = getch();
                }
            }
            term = c;
            if (term == '~' || term == '^') {
                if (b == 5 && a == 5) return KEY_CTRL_PGUP;
                if (b == 5 && a == 6) return KEY_CTRL_PGDN;
                if (a == 1 || a == 7) return KEY_HOME;
                if (a == 3) return KEY_DEL;
                if (a == 4 || a == 8) return KEY_END;
                if (a == 5) return KEY_PGUP;
                if (a == 6) return KEY_PGDN;
            }
            return KEY_ESC;
        }
        if (c == 'A') return KEY_UP;
        if (c == 'B') return KEY_DOWN;
        if (c == 'C') return KEY_RIGHT;
        if (c == 'D') return KEY_LEFT;
        if (c == 'H') return KEY_HOME;
        if (c == 'F') return KEY_END;
    } else if (c == 'O') {
        c = getch();
        if (c == 'H') return KEY_HOME;
        if (c == 'F') return KEY_END;
    }
    return KEY_ESC;
}

static void insert_line_at(pos, s)
int pos;
char *s;
{
    int i;
    if (E.nlines >= E.line_cap) grow_lines(E.nlines + 1);
    pos = clampi(pos, 0, E.nlines);
    for (i = E.nlines; i > pos; i--) E.line[i] = E.line[i - 1];
    E.line[pos] = s;
    E.nlines++;
}

static void delete_line_at(pos)
int pos;
{
    int i;
    if (pos < 0 || pos >= E.nlines) return;
    free_line(E.line[pos]);
    for (i = pos; i < E.nlines - 1; i++) E.line[i] = E.line[i + 1];
    E.nlines--;
    if (E.nlines == 0) {
        E.line[0] = xstrdup("");
        E.nlines = 1;
    }
}

static void replace_line(y, s)
int y;
char *s;
{
    if (y < 0 || y >= E.nlines) {
        free_line(s);
        return;
    }
    free_line(E.line[y]);
    E.line[y] = s;
}

static void load_file(name)
const char *name;
{
    FILE *fp;
    char buf[MAX_LINE + 4];
    int len;
    int ch;
    int prev_cr;

    strncpy(E.filename, name, NAME_LEN - 1);
    E.filename[NAME_LEN - 1] = '\0';

    clear_lines();
    E.crlf = 0;

    fp = fopen(name, "r");
    if (fp == NULL) {
        E.line[0] = xstrdup("");
        E.nlines = 1;
        E.crlf = 1;
        E.dirty = 0;
        set_status0("new file");
        return;
    }

    len = 0;
    prev_cr = 0;
    while ((ch = fgetc(fp)) != EOF) {
        ch &= 0xff;
        if (ch == 0x1a) break;  /* CP/M Ctrl-Z = end of text file */
        if (ch == '\r') {
            E.crlf = 1;
            prev_cr = 1;
            continue;
        }
        if (ch == '\n') {
            insert_line_at(E.nlines, make_line(buf, len));
            len = 0;
            prev_cr = 0;
            continue;
        }
        if (prev_cr) {
            insert_line_at(E.nlines, make_line(buf, len));
            len = 0;
            prev_cr = 0;
        }
        if (len < MAX_LINE) buf[len++] = (char)ch;
    }
    if (prev_cr || len > 0 || E.nlines == 0) {
        insert_line_at(E.nlines, make_line(buf, len));
    }
    fclose(fp);
    ensure_cursor();
    E.dirty = 0;
    E.redraw = 2;
    set_status("opened %s %d lines", E.filename, E.nlines);
}

static int save_file(name)
const char *name;
{
    FILE *fp;
    int i;
    fp = fopen(name, "w");
    if (fp == NULL) {
        set_status0("save failed: open");
        return 0;
    }
    for (i = 0; i < E.nlines; i++) {
        fputs(E.line[i], fp);
        if (i + 1 < E.nlines) {
            if (E.crlf) fputs("\r\n", fp);
            else fputc('\n', fp);
        }
    }
    if (fclose(fp) != 0) {
        set_status0("save failed: close");
        return 0;
    }
    strncpy(E.filename, name, NAME_LEN - 1);
    E.filename[NAME_LEN - 1] = '\0';
    E.dirty = 0;
    set_status("saved %s", E.filename, 0);
    return 1;
}

static void scroll_editor()
{
    ensure_cursor();
    if (E.cy < E.rowoff) E.rowoff = E.cy;
    if (E.cy >= E.rowoff + TEXT_ROWS) E.rowoff = E.cy - TEXT_ROWS + 1;
    if (E.cx < E.coloff) E.coloff = E.cx;
    if (E.cx >= E.coloff + DRAW_COLS) E.coloff = E.cx - DRAW_COLS + 1;
}

static void vt_goto(row, col)
int row;
int col;
{
    char pos[24];
    if (row < 1) row = 1;
    if (row > SCREEN_ROWS) row = SCREEN_ROWS;
    if (col < 1) col = 1;
    if (col > SCREEN_COLS) col = SCREEN_COLS;
    sprintf(pos, "\033[%d;%dH", row, col);
    outstr(pos);
}

static void clear_draw_cols()
{
    int i;
    for (i = 0; i < DRAW_COLS; i++) putchar(' ');
}

static void draw_row(y)
int y;
{
    int filerow;
    int col;
    int i;
    int len;
    int drawn;
    char *s;
    char ch;

    vt_goto(y + 1, 1);
    filerow = y + E.rowoff;
    drawn = 0;
    if (filerow >= E.nlines) {
        putchar('~');
        drawn = 1;
    } else {
        s = E.line[filerow];
        len = (int)strlen(s);
        col = 0;
        for (i = 0; i < len && drawn < DRAW_COLS; i++) {
            ch = s[i];
            if (ch == '\t') {
                int spaces;
                spaces = TAB_STOP - (col % TAB_STOP);
                while (spaces-- > 0) {
                    if (col >= E.coloff && drawn < DRAW_COLS) {
                        putchar(' ');
                        drawn++;
                    }
                    col++;
                }
            } else {
                if (col >= E.coloff && drawn < DRAW_COLS) {
                    if (ch >= 32 && ch <= 126) putchar(ch);
                    else putchar('?');
                    drawn++;
                }
                col++;
            }
        }
    }
    while (drawn < DRAW_COLS) {
        putchar(' ');
        drawn++;
    }
}

static void draw_rows()
{
    int y;
    for (y = 0; y < TEXT_ROWS; y++) draw_row(y);
}

static void draw_status()
{
    char left[STATUS_LEN];
    char msg[STATUS_LEN];
    char right[STATUS_LEN];
    int l;
    int m;
    int r;
    int n;
    int i;
    const char *name;

    name = E.filename[0] ? E.filename : "[No Name]";
    sprintf(left, " %.38s%s - %d lines ", name, E.dirty ? " *" : "", E.nlines);
    sprintf(right, " Ln %d Col %d ", E.cy + 1, visual_col(E.cy, E.cx) + 1);

    strncpy(msg, E.status, STATUS_LEN - 1);
    msg[STATUS_LEN - 1] = '\0';

    l = (int)strlen(left);
    m = (int)strlen(msg);
    r = (int)strlen(right);
    if (l > DRAW_COLS) l = DRAW_COLS;

    vt_goto(SCREEN_ROWS - 1, 1);
    outstr("\033[7m");

    n = 0;
    for (i = 0; i < l && n < DRAW_COLS; i++, n++) putchar(left[i]);

    if (m > 0 && n + 1 < DRAW_COLS) {
        putchar(' ');
        n++;
        for (i = 0; i < m && n < DRAW_COLS - r - 1; i++, n++) putchar(msg[i]);
    }

    while (n < DRAW_COLS) {
        if (DRAW_COLS - n == r) {
            for (i = 0; i < r && n < DRAW_COLS; i++, n++) putchar(right[i]);
            break;
        }
        putchar(' ');
        n++;
    }
    outstr("\033[0m");
}

static void draw_help()
{
    static char help[] =
        " ^W Save  ^A SaveAs  ^Z Exit  ^F Find  ^E/^X/^S/^D Move ";
    int i;

    vt_goto(SCREEN_ROWS, 1);
    outstr("\033[7m");
    for (i = 0; i < DRAW_COLS; i++) {
        if (help[i] == '\0') putchar(' ');
        else putchar(help[i]);
    }
    outstr("\033[0m");
}

static void refresh_screen()
{
    int rx;
    int ry;
    int prev_rowoff;
    int prev_coloff;

    prev_rowoff = E.rowoff;
    prev_coloff = E.coloff;
    scroll_editor();
    if (E.rowoff != prev_rowoff || E.coloff != prev_coloff) E.redraw = 2;

    outstr("\033[0m");

    if (E.redraw >= 2) {
        vt_goto(1, 1);
        draw_rows();
        draw_status();
        draw_help();
    } else if (E.redraw == 1) {
        ry = E.cy - E.rowoff;
        ry = clampi(ry, 0, TEXT_ROWS - 1);
        draw_row(ry);
        draw_status();
    } else {
        draw_status();
    }

    rx = E.cx - E.coloff;
    ry = E.cy - E.rowoff;
    rx = clampi(rx, 0, DRAW_COLS - 1);
    ry = clampi(ry, 0, TEXT_ROWS - 1);
    vt_goto(ry + 1, rx + 1);
    fflush(stdout);
    E.redraw = 0;
}

static int prompt_text(prompt, out, outsz)
const char *prompt;
char *out;
int outsz;
{
    int k;
    int len;
    out[0] = '\0';
    len = 0;
    for (;;) {
        strncpy(E.status, prompt, STATUS_LEN - 1);
        E.status[STATUS_LEN - 1] = '\0';
        strncat(E.status, out, STATUS_LEN - 1 - strlen(E.status));
        refresh_screen();
        k = read_key();
        if (k == KEY_CTRL_C || k == KEY_ESC) {
            set_status0("cancelled");
            return 0;
        }
        if (k == KEY_ENTER || k == '\n') {
            if (len > 0) return 1;
        } else if (k == KEY_BACKSPACE || k == KEY_CTRL_H || k == KEY_DEL) {
            if (len > 0) out[--len] = '\0';
        } else if (k >= 32 && k <= 126) {
            if (len + 1 < outsz) {
                out[len++] = (char)k;
                out[len] = '\0';
            }
        }
    }
}

static void mark_non_cut()
{
    E.last_was_cut = 0;
}

static void insert_char(c)
int c;
{
    char *old;
    char *nw;
    int len;
    mark_non_cut();
    ensure_cursor();
    old = E.line[E.cy];
    len = (int)strlen(old);
    if (len >= MAX_LINE) {
        set_status0("line too long");
        return;
    }
    nw = (char *)xmalloc((unsigned)len + 2);
    memcpy(nw, old, (unsigned)E.cx);
    nw[E.cx] = (char)c;
    strcpy(nw + E.cx + 1, old + E.cx);
    replace_line(E.cy, nw);
    E.cx++;
    E.dirty = 1;
    E.redraw = 1;
}

static void insert_soft_tab()
{
    int spaces;
    spaces = TAB_STOP - (visual_col(E.cy, E.cx) % TAB_STOP);
    while (spaces-- > 0) insert_char(' ');
}

static void insert_newline()
{
    char *old;
    char *left;
    char *right;
    int len;
    mark_non_cut();
    ensure_cursor();
    old = E.line[E.cy];
    len = (int)strlen(old);
    left = make_line(old, E.cx);
    right = make_line(old + E.cx, len - E.cx);
    replace_line(E.cy, left);
    insert_line_at(E.cy + 1, right);
    E.cy++;
    E.cx = 0;
    E.dirty = 1;
    E.redraw = 2;
}

static void del_backspace()
{
    char *old;
    char *nw;
    int len;
    int plen;
    mark_non_cut();
    ensure_cursor();
    if (E.cy == 0 && E.cx == 0) return;
    if (E.cx > 0) {
        old = E.line[E.cy];
        len = (int)strlen(old);
        nw = (char *)xmalloc((unsigned)len);
        memcpy(nw, old, (unsigned)(E.cx - 1));
        strcpy(nw + E.cx - 1, old + E.cx);
        replace_line(E.cy, nw);
        E.cx--;
        E.redraw = 1;
    } else {
        plen = (int)strlen(E.line[E.cy - 1]);
        len = plen + (int)strlen(E.line[E.cy]);
        if (len > MAX_LINE) {
            set_status0("joined line too long");
            return;
        }
        nw = (char *)xmalloc((unsigned)len + 1);
        strcpy(nw, E.line[E.cy - 1]);
        strcat(nw, E.line[E.cy]);
        replace_line(E.cy - 1, nw);
        delete_line_at(E.cy);
        E.cy--;
        E.cx = plen;
        E.redraw = 2;
    }
    E.dirty = 1;
}

static void del_at_cursor()
{
    char *old;
    char *nw;
    int len;
    int nlen;
    mark_non_cut();
    ensure_cursor();
    old = E.line[E.cy];
    len = (int)strlen(old);
    if (E.cx < len) {
        nw = (char *)xmalloc((unsigned)len);
        memcpy(nw, old, (unsigned)E.cx);
        strcpy(nw + E.cx, old + E.cx + 1);
        replace_line(E.cy, nw);
        E.dirty = 1;
        E.redraw = 1;
        return;
    }
    if (E.cy + 1 < E.nlines) {
        nlen = len + (int)strlen(E.line[E.cy + 1]);
        if (nlen > MAX_LINE) {
            set_status0("joined line too long");
            return;
        }
        nw = (char *)xmalloc((unsigned)nlen + 1);
        strcpy(nw, old);
        strcat(nw, E.line[E.cy + 1]);
        replace_line(E.cy, nw);
        delete_line_at(E.cy + 1);
        E.dirty = 1;
        E.redraw = 2;
    }
}

static void copy_line()
{
    mark_non_cut();
    ensure_cursor();
    free_cutbuf();
    if (E.cut_cap < 1) grow_cut(1);
    E.cut[0] = xstrdup(E.line[E.cy]);
    E.ncut = 1;
    set_status0("copied line");
}

static void cut_line()
{
    ensure_cursor();
    if (!E.last_was_cut) free_cutbuf();
    if (E.ncut >= E.cut_cap) grow_cut(E.ncut + 1);
    E.cut[E.ncut++] = xstrdup(E.line[E.cy]);
    delete_line_at(E.cy);
    if (E.cy >= E.nlines) E.cy = E.nlines - 1;
    E.cx = clampi(E.cx, 0, (int)strlen(E.line[E.cy]));
    E.last_was_cut = 1;
    E.dirty = 1;
    E.redraw = 2;
    set_status("cut %s%d lines", "", E.ncut);
}

static void paste_below()
{
    int i;
    int pos;
    mark_non_cut();
    ensure_cursor();
    if (E.ncut == 0) {
        set_status0("cutbuffer empty");
        return;
    }
    pos = E.cy + 1;
    for (i = 0; i < E.ncut; i++) {
        insert_line_at(pos + i, xstrdup(E.cut[i]));
    }
    E.cy = pos;
    E.cx = 0;
    E.dirty = 1;
    E.redraw = 2;
    set_status("pasted %s%d lines", "", i);
}

static void move_cursor(key)
int key;
{
    int len;
    mark_non_cut();
    ensure_cursor();
    if (key == KEY_LEFT) {
        if (E.cx > 0) E.cx--;
        else if (E.cy > 0) {
            E.cy--;
            E.cx = (int)strlen(E.line[E.cy]);
        }
    } else if (key == KEY_RIGHT) {
        len = (int)strlen(E.line[E.cy]);
        if (E.cx < len) E.cx++;
        else if (E.cy + 1 < E.nlines) {
            E.cy++;
            E.cx = 0;
        }
    } else if (key == KEY_UP) {
        if (E.cy > 0) E.cy--;
    } else if (key == KEY_DOWN) {
        if (E.cy + 1 < E.nlines) E.cy++;
    } else if (key == KEY_HOME) {
        E.cx = 0;
    } else if (key == KEY_END) {
        E.cx = (int)strlen(E.line[E.cy]);
    }
    ensure_cursor();
}

static void page_move(down)
int down;
{
    mark_non_cut();
    ensure_cursor();
    if (down) E.cy = clampi(E.cy + TEXT_ROWS, 0, E.nlines - 1);
    else E.cy = clampi(E.cy - TEXT_ROWS, 0, E.nlines - 1);
    ensure_cursor();
}

static void goto_start()
{
    mark_non_cut();
    E.cy = 0;
    E.cx = 0;
    set_status0("top of file");
}

static void goto_end()
{
    mark_non_cut();
    E.cy = E.nlines - 1;
    E.cx = 0;
    set_status0("end of file");
}

static void do_search()
{
    char q[64];
    int y;
    char *p;
    int start;
    mark_non_cut();
    if (!prompt_text("Find: ", q, sizeof(q))) return;
    for (start = 0; start < 2; start++) {
        for (y = (start == 0 ? E.cy : 0); y < E.nlines; y++) {
            p = strstr(E.line[y], q);
            if (p != NULL) {
                E.cy = y;
                E.cx = (int)(p - E.line[y]);
                set_status0("found");
                return;
            }
        }
    }
    set_status0("not found");
}

static void save_as()
{
    char name[NAME_LEN];
    mark_non_cut();
    if (!prompt_text("Save As: ", name, sizeof(name))) return;
    save_file(name);
}

static void quick_save()
{
    mark_non_cut();
    if (E.filename[0] == '\0') save_as();
    else save_file(E.filename);
}

static int confirm(question)
const char *question;
{
    int k;
    set_status0(question);
    refresh_screen();
    for (;;) {
        k = read_key();
        if (k == 'y' || k == 'Y' || k == KEY_CTRL_Z) return 1;
        if (k == 'n' || k == 'N' || k == KEY_CTRL_C || k == KEY_ESC) return 0;
    }
}

static int process_key(k)
int k;
{
    switch (k) {
    case KEY_CTRL_Z:
        mark_non_cut();
        if (E.dirty && !confirm("Unsaved changes. Quit anyway? y/n")) {
            set_status0("quit cancelled");
            return 1;
        }
        return 0;
    case KEY_CTRL_E: move_cursor(KEY_UP); return 1;
    case KEY_CTRL_X: move_cursor(KEY_DOWN); return 1;
    case KEY_CTRL_S: move_cursor(KEY_LEFT); return 1;
    case KEY_CTRL_D: move_cursor(KEY_RIGHT); return 1;
    case KEY_CTRL_R: page_move(0); return 1;
    case KEY_CTRL_C: page_move(1); return 1;
    case KEY_CTRL_F: do_search(); return 1;
    case KEY_CTRL_W: quick_save(); return 1;
    case KEY_CTRL_A: save_as(); return 1;
    case KEY_CTRL_T: cut_line(); return 1;
    case KEY_CTRL_Y: copy_line(); return 1;
    case KEY_CTRL_V: paste_below(); return 1;
    case KEY_PGUP: page_move(0); return 1;
    case KEY_PGDN: page_move(1); return 1;
    case KEY_HOME:
    case KEY_END:
    case KEY_UP:
    case KEY_DOWN:
    case KEY_LEFT:
    case KEY_RIGHT:
        move_cursor(k); return 1;
    case KEY_CTRL_PGUP: goto_start(); return 1;
    case KEY_CTRL_PGDN: goto_end(); return 1;
    case KEY_DEL: del_at_cursor(); return 1;
    case KEY_BACKSPACE:
    case KEY_CTRL_H: del_backspace(); return 1;
    case KEY_ENTER:
    case '\n': insert_newline(); return 1;
    case KEY_TAB: insert_soft_tab(); return 1;
    default:
        if (k >= 32 && k <= 126) insert_char(k);
        else mark_non_cut();
        return 1;
    }
}

int main(argc, argv)
int argc;
char **argv;
{
    int k;
    init_editor();
    if (argc >= 2) load_file(argv[1]);
    else set_status0("ready");
    vt_clear();

    for (;;) {
        refresh_screen();
        k = read_key();
        if (!process_key(k)) break;
    }
    vt_clear();
    free_cutbuf();
    clear_lines();
    free(E.line);
    return 0;
}
