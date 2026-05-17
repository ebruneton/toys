fn panic_result() -> u64 [
  72,139,52,36,   /*mov rsi,[rsp]*/
  72,137,231,     /*mov rdi,rsp*/
  65,184,0,0,0,0, /*mov r8d,0*/
  195             /*ret*/
]

fn panic(error: u64) [
  72,137,252,   /*mov rsp,rdi*/
  72,137,52,36, /*mov [rsp],rsi*/
  195           /*ret*/
]

const TC_INTEGER: u64 = 2;
const TC_IDENTIFIER: u64 = 3;
const TC_ADD: u64 = 4;
const TC_SUB: u64 = 5;
const TC_MUL: u64 = 6;
const TC_DIV: u64 = 7;
const TC_BIT_AND: u64 = 8;
const TC_BIT_OR: u64 = 9;
const TC_SHIFT_LEFT: u64 = 10;
const TC_SHIFT_RIGHT: u64 = 11;
const TC_LT: u64 = 12;
const TC_GE: u64 = 17;
const TC_AND: u64 = 18;
const TC_OR: u64 = 19;
const TC_ARROW: u64 = 20;
const TC_AS: u64 = 138;
const TC_BREAK: u64 = 128;
const TC_CONST: u64 = 129;
const TC_ELSE: u64 = 130;
const TC_FN: u64 = 131;
const TC_IF: u64 = 132;
const TC_LET: u64 = 133;
const TC_LOOP: u64 = 134;
const TC_NULL: u64 = 139;
const TC_RETURN: u64 = 135;
const TC_SIZEOF: u64 = 140;
const TC_STATIC: u64 = 136;
const TC_STRUCT: u64 = 141;
const TC_U32: u64 = 142;
const TC_WHILE: u64 = 137;

const SYM_FN: u64 = 0;
const SYM_FORWARD_FN: u64 = 1;
const SYM_VARIABLE: u64 = 2;
const SYM_CONST: u64 = 3;
const SYM_STATIC: u64 = 4;
const SYM_STRUCT: u64 = 5;
const SYM_FIELD: u64 = 6;
const SYM_VOID: u64 = 7;

struct Symbol {
  name: &u64,
  length: u64,
  kind: u64,
  value: u64,
  type: &Symbol,
  dim: u64,
  next: &Symbol
}

struct Compiler {
  src: &u64,
  src_end: &u64,
  next_char: u64,
  next_char_type: u64,
  next_token: u64,
  next_token_data: u64,
  next_token_length: u64,
  dst: &u64,
  dst_mark: &u64,
  dst_limit: &u64,
  heap: &u64,
  heap_limit: &u64,
  symbols: &Symbol,
  fn_return_type: &Symbol,
  next_register: u64,
  frame_size: u64
}
