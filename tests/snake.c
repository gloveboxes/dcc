/*
 * snake.c - small VT-100 Snake for CP/M 2.2 and dcc C89.
 * Version 5: fixed status/help string overread and growth redraw.
 *
 * Target assumptions:
 *   - 80x24 VT-100-ish console.
 *   - getch() is blocking, no echo.
 *   - kbhit() polls for pending input.
 *   - ^C or ^Q quits at any time.
 *
 * Controls:
 *   ^E / arrow up / W      up
 *   ^X / arrow down / X/S  down
 *   ^S / arrow left / A    left
 *   ^D / arrow right / D   right
 *   Space                  pause
 *   ^C or ^Q               quit
 *
 * The code avoids host clocks.  At startup it asks the user to press a key
 * roughly once per second three times and uses the amount of busy looping
 * between presses as its timing base.  This makes the game playable both on
 * a slow Z80 and on very fast emulators.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int getch();
extern int kbhit();

#define SCREEN_ROWS 24
#define SCREEN_COLS 80
#define DRAW_COLS   79

#define FIELD_X1    2
#define FIELD_Y1    3
#define FIELD_W     60
#define FIELD_H     18
#define FIELD_X2    (FIELD_X1 + FIELD_W - 1)
#define FIELD_Y2    (FIELD_Y1 + FIELD_H - 1)
#define MAX_SNAKE   (FIELD_W * FIELD_H)

#define KEY_CTRL_C   3
#define KEY_CTRL_D   4
#define KEY_CTRL_E   5
#define KEY_CTRL_Q   17
#define KEY_CTRL_S   19
#define KEY_CTRL_X   24
#define KEY_ESC      27

#define KEY_UP       1005
#define KEY_DOWN     1006
#define KEY_LEFT     1007
#define KEY_RIGHT    1008

#define DIR_UP       0
#define DIR_RIGHT    1
#define DIR_DOWN     2
#define DIR_LEFT     3

static unsigned char sx[MAX_SNAKE];
static unsigned char sy[MAX_SNAKE];
static int slen;
static int dir;
static int pending_dir;
static int food_x;
static int food_y;
static int score;
static int game_over;
static unsigned long rng_state;
static unsigned long tick_delay;

static void outstr(s)
const char *s;
{
    fputs(s, stdout);
}

static void vt_goto(row, col)
int row;
int col;
{
    char buf[24];
    if (row < 1) row = 1;
    if (row > SCREEN_ROWS) row = SCREEN_ROWS;
    if (col < 1) col = 1;
    if (col > SCREEN_COLS) col = SCREEN_COLS;
    sprintf(buf, "\033[%d;%dH", row, col);
    outstr(buf);
}

static void vt_clear()
{
    outstr("\033[0m");
    outstr("\033[2J");
    outstr("\033[1;1H");
    fflush(stdout);
}

static void clear_draw_cols()
{
    int i;
    for (i = 0; i < DRAW_COLS; i++) putchar(' ');
}

static void clear_line(row)
int row;
{
    vt_goto(row, 1);
    clear_draw_cols();
}

static int read_key()
{
    int c;

    c = getch();
    if (c != KEY_ESC) return c;

    if (!kbhit()) return KEY_ESC;
    c = getch();
    if (c == '[') {
        c = getch();
        if (c == 'A') return KEY_UP;
        if (c == 'B') return KEY_DOWN;
        if (c == 'C') return KEY_RIGHT;
        if (c == 'D') return KEY_LEFT;
    }
    return KEY_ESC;
}

static int next_poll_key()
{
    if (!kbhit()) return 0;
    return read_key();
}

static int is_quit_key(k)
int k;
{
    return k == KEY_CTRL_C || k == KEY_CTRL_Q || k == 'q' || k == 'Q';
}

static void update_status(msg)
const char *msg;
{
    int i;
    int done;
    char buf[90];

    if (msg == NULL) msg = "";
    sprintf(buf, " Score:%d  Len:%d  %s", score, slen, msg);
    buf[DRAW_COLS] = '\0';

    vt_goto(1, 1);
    outstr("\033[7m");
    done = 0;
    for (i = 0; i < DRAW_COLS; i++) {
        if (!done && buf[i] != '\0') {
            putchar(buf[i]);
        } else {
            putchar(' ');
            done = 1;
        }
    }
    outstr("\033[0m");
}

static void draw_help()
{
    static char help[] =
        " ^E/^X/^S/^D or arrows move   Space pause   ^C/^Q quit ";
    int i;
    int done;

    vt_goto(24, 1);
    outstr("\033[7m");
    done = 0;
    for (i = 0; i < DRAW_COLS; i++) {
        if (!done && help[i] != '\0') {
            putchar(help[i]);
        } else {
            putchar(' ');
            done = 1;
        }
    }
    outstr("\033[0m");
}

static void draw_border()
{
    int x;
    int y;

    for (x = FIELD_X1 - 1; x <= FIELD_X2 + 1; x++) {
        vt_goto(FIELD_Y1 - 1, x);
        putchar('#');
        vt_goto(FIELD_Y2 + 1, x);
        putchar('#');
    }
    for (y = FIELD_Y1; y <= FIELD_Y2; y++) {
        vt_goto(y, FIELD_X1 - 1);
        putchar('#');
        vt_goto(y, FIELD_X2 + 1);
        putchar('#');
    }
}

static void put_cell(x, y, ch)
int x;
int y;
int ch;
{
    vt_goto(y, x);
    putchar(ch);
}

static unsigned next_rand()
{
    rng_state = rng_state * 1103515245UL + 12345UL;
    return (unsigned)((rng_state >> 8) & 0x7fffUL);
}

static int snake_at(x, y)
int x;
int y;
{
    int i;
    for (i = 0; i < slen; i++) {
        if ((int)sx[i] == x && (int)sy[i] == y) return 1;
    }
    return 0;
}

static void place_food()
{
    int tries;
    int x;
    int y;

    for (tries = 0; tries < 2000; tries++) {
        x = FIELD_X1 + (int)(next_rand() % FIELD_W);
        y = FIELD_Y1 + (int)(next_rand() % FIELD_H);
        if (!snake_at(x, y)) {
            food_x = x;
            food_y = y;
            put_cell(food_x, food_y, '*');
            return;
        }
    }

    for (y = FIELD_Y1; y <= FIELD_Y2; y++) {
        for (x = FIELD_X1; x <= FIELD_X2; x++) {
            if (!snake_at(x, y)) {
                food_x = x;
                food_y = y;
                put_cell(food_x, food_y, '*');
                return;
            }
        }
    }
}

static void init_game()
{
    int midx;
    int midy;

    midx = FIELD_X1 + FIELD_W / 2;
    midy = FIELD_Y1 + FIELD_H / 2;
    slen = 5;
    sx[0] = (unsigned char)midx;
    sy[0] = (unsigned char)midy;
    sx[1] = (unsigned char)(midx - 1);
    sy[1] = (unsigned char)midy;
    sx[2] = (unsigned char)(midx - 2);
    sy[2] = (unsigned char)midy;
    sx[3] = (unsigned char)(midx - 3);
    sy[3] = (unsigned char)midy;
    sx[4] = (unsigned char)(midx - 4);
    sy[4] = (unsigned char)midy;
    dir = DIR_RIGHT;
    pending_dir = DIR_RIGHT;
    score = 0;
    game_over = 0;
}

static void draw_snake_full()
{
    int i;

    for (i = slen - 1; i >= 0; i--) {
        if (i == 0) put_cell((int)sx[i], (int)sy[i], '@');
        else put_cell((int)sx[i], (int)sy[i], 'o');
    }
}

static void clear_field()
{
    int x;
    int y;

    for (y = FIELD_Y1; y <= FIELD_Y2; y++) {
        vt_goto(y, FIELD_X1);
        for (x = FIELD_X1; x <= FIELD_X2; x++) putchar(' ');
    }
}

static void full_redraw(msg)
const char *msg;
{
    vt_clear();
    update_status(msg);
    draw_border();
    clear_field();
    draw_snake_full();
    put_cell(food_x, food_y, '*');
    draw_help();
    fflush(stdout);
}

static void set_direction(d)
int d;
{
    if (d == DIR_UP && dir != DIR_DOWN) pending_dir = d;
    if (d == DIR_DOWN && dir != DIR_UP) pending_dir = d;
    if (d == DIR_LEFT && dir != DIR_RIGHT) pending_dir = d;
    if (d == DIR_RIGHT && dir != DIR_LEFT) pending_dir = d;
}

static int handle_key(k)
int k;
{
    if (k == 0) return 1;
    if (is_quit_key(k)) return 0;

    if (k == KEY_UP || k == KEY_CTRL_E || k == 'w' || k == 'W') {
        set_direction(DIR_UP);
    } else if (k == KEY_DOWN || k == KEY_CTRL_X || k == 'x' || k == 'X' ||
               k == 's' || k == 'S') {
        set_direction(DIR_DOWN);
    } else if (k == KEY_LEFT || k == KEY_CTRL_S || k == 'a' || k == 'A') {
        set_direction(DIR_LEFT);
    } else if (k == KEY_RIGHT || k == KEY_CTRL_D || k == 'd' || k == 'D') {
        set_direction(DIR_RIGHT);
    } else if (k == ' ') {
        update_status("paused - any key resumes");
        fflush(stdout);
        k = read_key();
        if (is_quit_key(k)) return 0;
        update_status("playing");
    }
    return 1;
}

static int delay_or_quit()
{
    unsigned long i;
    int k;

    for (i = 0; i < tick_delay; i++) {
        if ((i & 31UL) == 0UL) {
            k = next_poll_key();
            if (k != 0) {
                if (!handle_key(k)) return 0;
            }
        }
    }
    return 1;
}

static void move_snake()
{
    int nx;
    int ny;
    int eat;
    int i;
    int old_tail_x;
    int old_tail_y;

    dir = pending_dir;
    nx = (int)sx[0];
    ny = (int)sy[0];
    if (dir == DIR_UP) ny--;
    else if (dir == DIR_DOWN) ny++;
    else if (dir == DIR_LEFT) nx--;
    else nx++;

    if (nx < FIELD_X1 || nx > FIELD_X2 || ny < FIELD_Y1 || ny > FIELD_Y2) {
        game_over = 1;
        return;
    }

    eat = (nx == food_x && ny == food_y);
    old_tail_x = (int)sx[slen - 1];
    old_tail_y = (int)sy[slen - 1];

    for (i = 0; i < slen - (eat ? 0 : 1); i++) {
        if ((int)sx[i] == nx && (int)sy[i] == ny) {
            game_over = 1;
            return;
        }
    }

    if (eat && slen < MAX_SNAKE) slen++;
    for (i = slen - 1; i > 0; i--) {
        sx[i] = sx[i - 1];
        sy[i] = sy[i - 1];
    }
    sx[0] = (unsigned char)nx;
    sy[0] = (unsigned char)ny;

    put_cell((int)sx[1], (int)sy[1], 'o');
    put_cell((int)sx[0], (int)sy[0], '@');

    if (eat) {
        score += 10;
        update_status("playing");
        if (slen >= MAX_SNAKE) {
            game_over = 2;
            return;
        }
        place_food();

        /*
         * Growth leaves the old tail in place by design, but redraw the whole
         * snake after eating.  This avoids depending on stale terminal state
         * and makes the newly added segment explicit.
         */
        draw_snake_full();
    } else {
        put_cell(old_tail_x, old_tail_y, ' ');
    }
    fflush(stdout);
}

static unsigned long calibrate_one(prompt)
const char *prompt;
{
    unsigned long n;
    int k;

    clear_line(7);
    vt_goto(7, 1);
    outstr(prompt);
    fflush(stdout);

    while (kbhit()) getch();
    n = 0UL;
    for (;;) {
        n++;
        if (n == 0xffffffffUL) return n;
        if (kbhit()) {
            k = getch();
            if (is_quit_key(k)) return 0UL;
            return n;
        }
    }
}

static int calibrate_delay()
{
    unsigned long a;
    unsigned long b;
    unsigned long c;
    unsigned long avg;

    vt_clear();
    outstr("Snake timing calibration\r\n\r\n");
    outstr("Press a key, wait about one second, press again.\r\n");
    outstr("Repeat for three one-second intervals.  ^C or ^Q quits.\r\n\r\n");
    outstr("Press any key to start... ");
    fflush(stdout);

    if (is_quit_key(getch())) return 0;

    a = calibrate_one("1/3: wait about one second, then press a key");
    if (a == 0UL) return 0;
    rng_state ^= a;
    b = calibrate_one("2/3: wait about one second, then press a key");
    if (b == 0UL) return 0;
    rng_state ^= b << 3;
    c = calibrate_one("3/3: wait about one second, then press a key");
    if (c == 0UL) return 0;
    rng_state ^= c << 7;

    avg = a / 3UL + b / 3UL + c / 3UL;
    tick_delay = avg / 5UL;      /* About five snake moves per second. */
    if (tick_delay < 200UL) tick_delay = 200UL;
    return 1;
}

static void game_loop()
{
    int k;

    for (;;) {
        if (!delay_or_quit()) return;
        while (kbhit()) {
            k = read_key();
            if (!handle_key(k)) return;
        }
        move_snake();
        if (game_over) return;
    }
}

int main(argc, argv)
int argc;
char **argv;
{
    (void)argc;
    (void)argv;

    rng_state = 0x1234abcdUL;
    tick_delay = 5000UL;

    if (!calibrate_delay()) {
        vt_clear();
        return 0;
    }

    init_game();
    place_food();
    full_redraw("playing");
    game_loop();

    if (game_over == 2) update_status("you filled the board - winner!");
    else if (game_over) update_status("game over - press any key");
    else update_status("quit - press any key");
    fflush(stdout);
    getch();
    vt_clear();
    return 0;
}
