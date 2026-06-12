/*
 * dcc_state.c - definitions of the shared mutable state for the dcc compiler.
 *
 * MODULE: its own translation unit. This file DEFINES the compiler's
 * file-scope globals; every other module references them through the matching
 * `extern` declarations in the umbrella header dcc.h.
 *
 * Why dcc keeps so much shared state: it is a single-pass, syntax-directed
 * compiler with no AST. Parser and code generator share a large amount of
 * "current position" state (the source buffer, the lookahead token, the
 * symbol tables, per-function codegen flags, ...). Keeping all of it in one
 * place, declared once in dcc.h, lets the modules cooperate without threading
 * the state through every call.
 *
 * Intentionally NOT here (kept private/static to one module for locality):
 *   - pp_expr_p / pp_expr_depth        -> dcc_preproc.c (#if expression cursor)
 *   - include_dirs / num_include_dirs  -> dcc.c (include search path)
 *
 * Source provenance: monolith src/ddc.c lines 199-203, 347-348, 378-489.
 */

#include "dcc.h"

/* ---- assembler symbol-name table + command-line options ---------------- */
struct AsmName asm_names[MAX_ASM_NAMES];
int nasm_names;
int opt_floatio;
int opt_module;      /* -c/-module: emit linkable helper module, not final app TU */
int opt_stack_size;  /* bytes reserved above heap for C stack */

/* ---- typedef table ----------------------------------------------------- */
struct TypeDef typedefs[MAX_TYPEDEFS];
int ntypedefs;

/* ---- struct/union + field tables --------------------------------------- */
struct StructDef struct_defs[MAX_STRUCTS];
int nstruct_defs;
struct FieldDef field_defs[MAX_FIELDS];
int nfield_defs;

/* ---- in-progress field metadata (filled while parsing one declarator) -- */
int current_field_array_elem_size;
int current_field_array_dim_count;
int current_field_array_dims[4];
int current_field_bit_width;
int current_field_bit_shift;
unsigned int current_field_bit_mask;

/* ---- source buffer + lexer position + lookahead token ------------------ */
char *src;
long src_len;
long posi;
long tok_start_pos;
int line_no;
int tok_line;
struct Token tok;
FILE *outf;
const char *input_name;
const char *output_name;
char current_file_name[256];
char predefined_date_text[16];
char predefined_time_text[16];

/* ---- symbol tables ----------------------------------------------------- */
struct Sym globals[MAX_SYMS];
int nglobals;
struct Sym locals[MAX_LOCALS];
int nlocals;
int local_size;
int param_offset;

/* ---- preprocessor macro table ------------------------------------------ */
struct Def defs[MAX_DEFINES];
int ndefs;

/* ---- #if/#ifdef conditional-inclusion stack ---------------------------- */
int if_parent_active[MAX_IFSTACK];
int if_this_active[MAX_IFSTACK];
int if_seen_else[MAX_IFSTACK];
int if_branch_taken[MAX_IFSTACK];
int if_sp;
int pp_active = 1;

/* ---- string-literal pool ----------------------------------------------- */
char *strings[MAX_STRINGS];
int string_wide[MAX_STRINGS];
int nstrings;

/* ---- deferred EXTRN emission list --------------------------------------- */
struct Sym *used_extrns[MAX_USED_EXTRNS];
int nused_extrns;

/* ---- per-function code-generation state -------------------------------- */
int label_id;
int current_return_label;
int current_return_type;
int parse_function_return_type;
int current_local_bytes;
int max_function_local_bytes;
int current_omit_ix_frame;
int current_function_has_call;

/* ---- loop break/continue target stack + parser flags ------------------- */
int break_stack[MAX_FLOW];
int cont_stack[MAX_FLOW];
int nflow;
int errors;
int scan_mode;
int decl_is_extern;
int decl_is_static;
int decl_is_const;      /* current declaration used const qualifier */
int decl_is_register;   /* current decl used 'register' keyword */
int expr_result_dead;
int g_expr_type;
int g_tok_long_suffix; /* set by lexer when L/l suffix seen on integer literal */
int g_tok_unsigned_suffix; /* set for U/u suffix or non-decimal unsigned-int literal */

/* User-defined goto labels (function-scoped) */
char ulabel_names[MAX_USER_LABELS][64];
int  ulabel_ids[MAX_USER_LABELS];
int  ulabel_defined[MAX_USER_LABELS];
int  ulabel_referenced[MAX_USER_LABELS];
int  nulabels;

/* Enum constants (file-scoped) */
char enum_const_names[MAX_ENUM_CONSTS][64];
int  enum_const_values[MAX_ENUM_CONSTS];
int  nenum_consts;

/* Communicates array length from array-typedef through parse_base_type to declarators */
int g_typedef_array_len;
int g_typedef_is_func;

/* Counter for naming anonymous structs/unions uniquely */
int g_anon_struct_counter;

/* Most recently parsed function prototype/parameter-list metadata. */
int g_proto_has;
int g_proto_nargs;
int g_proto_variadic;
int g_proto_types[MAX_PROTO_PARAMS];
int g_funcptr_decl_array_len;
int g_ptr_array_dim_count;
int g_ptr_array_dims[8];
int g_ptr_array_elem_size;
int g_last_array_dim_count;
int g_last_array_dims[8];
