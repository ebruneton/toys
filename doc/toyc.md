# Toy compiler

## Toy language

As the name implies, Toy is a **toy language**, with a very small set of
features. Semantically, it is approximatively a very small subset of C.
Syntactically, it is close to Rust. The language is presented in full details in
[Part 3](https://ebruneton.github.io/toypc/toypc.pdf#page=189) of [[1]](#toypc).
This section gives a short overview of the language, via some comments on its
grammar definition.

<a name="toypc">[1]</a>
[Programming a toy computer from scratch](https://ebruneton.github.io/toypc/toypc.pdf),
Eric Bruneton, 2024

### Types

- type: `&`\* (`u64` | IDENTIFIER) `;`
- struct: `struct` IDENTIFIER
`{` (IDENTIFIER `:` type (`,` IDENTIFIER `:` type)\*)? `}`

Toy has a very limited set of types: `u64` for unsigned 64 bits integers, struct
types, and pointer types. Nothing else. **Struct types cannot be nested, and
cannot be used for function parameters or local variables** (but struct
*pointer* types are allowed).

**Note**: *in Toy `&` denotes a pointer, not a reference*.

### Expressions

- expr: bit_and_expr (`|` bit_and_expr)\*
- bit_and_expr: shift_expr (`&` shift_expr)\*
- shift_expr: add_expr ((`<<` | `>>`) add_expr)\*
- add_expr: mult_expr ((`+` | `-`) mult_expr)\*
- mult_expr: cast_expr ((`*` | `/`) cast_expr)\*
- cast_expr: pointer_expr (`as` type)?
- pointer_expr: `*` pointer_expr | `&` path_expr | path_expr
- path_expr: primitive_expr (`.` IDENTIFIER)\*
- primitive_expr:
  - INTEGER
  - | IDENTIFIER `(` (expr (`,` expr)\*)? `)`?
  - | `sizeof` `(` IDENTIFIER `)`
  - | `null`
  - | `(` expr `)`

Toy supports basic logic (`|`, `&`, `<<`, and `>>`) and arithmetic (`+`, `-`,
`*`, `/`) binary expressions (but not unary ones). Adding (resp. subtracting) an
integer value *x* from a pointer gives a pointer of the same type, whose address
is incremented (resp. decremented) by *x* *bytes* (and not by the size of the
pointed value in bytes). Subtracting two pointers of the same type gives an
integer value (the difference of their addresses in *bytes*).

Toy also supports explicit casts (`as`), pointer dereference (`*`), "pointer to"
(`&`), field access (`.`), and method call expressions. A function name can also
be used alone (it evaluates to the function's address).

Literals are restricted to non-negative decimal literals, character literals
(such as `` `a` ``), and `null`
(**no binary or hexadecimal literals, no string literals**, etc).

### Statements

- const: `const` IDENTIFIER `:` type `=` INTEGER `;`
- let_stmt: `let` IDENTIFIER (`:` type)? `=` expr `;`
- stmt:
  - if_stmt
  - | while_or_loop_stmt
  - | break_stmt
  - | return_stmt
  - | expr_or_assign_stmt
- if_stmt: `if` boolean_expr block_stmt (`else` (block_stmt | if_stmt))?
- while_or_loop_stmt: (`while` boolean_expr | `loop`) block_stmt
- boolean_expr: and_expr (`||` and_expr)\*
- and_expr: comparison_expr (`&&` comparison_expr)\*
- comparison_expr: expr (`<` | `==` | `>` | `<=` | `!=` | `>=`) expr
- break_stmt: `break` `;`
- return_stmt: `return` expr? `;`
- expr_or_assign_stmt: expression (`=` expr)? `;`
- block_stmt: `{` stmt\* `}`

Toy supports constant (`const`) and local variable (`let`) definitions, but only
at the "root level" (i.e., not nested in conditional or loop statements). The
type of a local variable comes after its name, and can be omitted if the
initializer expression is not `null`. Control structures are restricted to `if`
(without parentheses around the condition), `while` (same comment), and `loop`
(infinite loop), with `break` and `return` to interrupt the normal flow
(**no `for` loop, no `continue`**).

### Functions

- fn: `fn` IDENTIFIER fn_parameters fn_body
- fn_parameters: `(` (IDENTIFIER `:` type (`,` IDENTIFIER `:` type)\*)? `)`
  (`->` type)?
- fn_body: `{` (const | let_stmt | stmt)\* `}` | fn_asm_body | `;`
- fn_asm_body: `[` (INTEGER (`,` INTEGER)\* )? `]`

Functions are declared with the `fn` keyword. Parameters are declared with a
name followed by a type. The (optional) return type is preceded by `->`. A
function must be declared before it can be called, but forward declarations
(functions without a body) are allowed. The function's body can be given as raw
*machine code* (not assembly code), as a list of byte values between square
brackets. This can be used to access special registers, for system programming.

### Programs

- program: (fn | struct | static | const)\* END
- static: `static` IDENTIFIER `=` `[` INTEGER (`,` INTEGER)\* `]` `;`

A program is simply a list of function, struct, and constant definitions
(**no modules, imports, includes, etc**). Static data can also be defined, as a
list of byte values between square brackets. This data is mutable (although
`static` blocks are generally used for immuable data, such as "strings"). It is
stored "where it is declared", between the compiled code of the surrounding
functions. Used in an expression, the name of a static block evaluates to an
`u64` pointer to its first byte.

## Compiler architecture

The Toy compiler uses a
[recursive descent parser](https://en.wikipedia.org/wiki/Recursive_descent_parser)
to parse the source code, type check it, and emit the corresponding compiled
code, all in [one pass](https://en.wikipedia.org/wiki/One-pass_compiler). **It
does not use any [preprocessor](https://en.wikipedia.org/wiki/C_preprocessor),
[assembler](https://en.wikipedia.org/wiki/Assembly_language#Assembler), [object
file format](https://en.wikipedia.org/wiki/Object_file), or
[linker](https://en.wikipedia.org/wiki/Linker_(computing))**. Instead, it
directly produces raw executable, **position-independent** machine code, without
any metadata. Its construction in several steps, starting from a basic bytecode
assembler, is presented in full details in
[Part 3](https://ebruneton.github.io/toypc/toypc.pdf#page=189) of [[1]](#toypc).
The next sections present the [differences](https://github.com/ebruneton/toys/commit/158f540b1a4edab61b81e27ceca4321e48860ae7) between the x86_64 version and the
original ARM-v7M version, which are essentially limited to the backend
implementation.

## Registers and calling convention

ARM provides 16 registers, named R0 to R15, but the machine code to access the
first 8 is different from the machine code to access the others. Also registers
R13, R14 and R15 are used for specific purposes: stack pointer, link register
(return address), and program counter (instruction pointer). For these reasons,
and to simplify the implementation, the original Toy compiler uses only the
*first* 8 registers, R0 to R7, to store values and pass arguments to functions.

Similarly, the x86_64 architecture has 16 64 bits registers, but the machine
code to access the first 8 is different from the machine code to access the
others. Also, some of the first 8 registers are used for specific purposes (such
as the stack pointer). For these reasons, the x86_64 Toy compiler only uses the
*last* 8 registers, named R8 to R15, to store values and pass arguments to
functions.

**Note**: internally, the compiler uses register indices from 0 to 7 to
designate registers (as in the original version). It is only when generating
machine code, in the backend, that these indices are converted to registers R8
to R15.

These registers are used as a "stack" to store intermediate values in
expressions (see
[section 20.2.1](https://ebruneton.github.io/toypc/toypc.pdf#page=366) of
[[1]](#toypc)). They are also used to pass arguments to functions (which must
have at most 8 parameters): *i*-th parameter in register R*i+8*. They must be
saved by the caller if it wants to use their value after the call returns (i.e.,
they are *caller-saved* registers; there are no *callee-saved* registers).

Function call stack frames are organized as in the original version. The
function call arguments are pushed on the stack by the callee, in increasing
address order, between the return address and the function's local variables.
The figure below illustrates this for a function with 3 arguments (arg0, arg1,
and arg2) and 2 local variables (x0 and x1; rip is the return address -- see
also [Figure 20.2](https://ebruneton.github.io/toypc/toypc.pdf#page=368) in
[[1]](#toypc)).

![Function stack frame](figures/stack-frame.svg)

## Backend

The backend API is almost the same as in the original version, but its
implementation is different in this x86_64 port. This section only presents some
of these implementation changes. Refer to
[Part 3](https://ebruneton.github.io/toypc/toypc.pdf#page=189) of [[1]](#toypc)
for a presentation of the API.

###  Placeholders

The original version uses 16 bits placeholders for forward jumps inside
functions, and 32 bits ones for forward function calls. The x86_64 version uses
32 bits placeholders for both cases. Consequently, the API and the
implementation used to manage placeholders is a bit simpler in the new version.

### Saving and restoring registers

On ARM-v7M it is possible to push (resp. pop) several registers on the stack
with a single PUSH (resp. POP) instruction. This is not possible with x86_64.
Instead, each register must be pushed with a specific instruction:

```rust
fn tc_save_registers(self: &Compiler, n: u64) {
  self.frame_size = self.frame_size + n;
  self.next_register = 0;
  while n > 0 {
    n = n - 1;
    tc_write16(self, /*push r{n+8}*/ 20545 | (n << 8));
  }
}
```

**Note**: the x86_64 instructions and their machine code encodings are described
in [[2]](#intelx86manualvol2). A summary is also available in
[HTML format](https://www.felixcloutier.com/x86/). But the simplest method to
find the encoding of an instruction (and the one used to write this backend) is
to use an [online x86 assembler](https://defuse.ca/online-x86-assembler.htm). At
a higher level, finding which instruction(s) to use to compile a piece of code
can be done with online tools such as [godbolt.org](https://godbolt.org/).

<a name="intelx86manualvol2">[2]</a>
[Intel 64 and IA-32 Architectures Software Developer’s Manual, Volume 2](https://cdrdv2.intel.com/v1/dl/getContent/671110),
Intel, 2023

### Pushing constant values

As in the original version, the `tc_write_cst_insn` function generates
"optimized" code to push a constant value on the stack. It tries to use smaller
instructions for smaller values. For this it uses, in particular, the
[xor r*x*,r*x* trick](https://xania.org/202512/01-xor-eax-eax) to set r*x* to 0:

```rust
fn tc_write_cst_insn(self: &Compiler, value: u64) {
  let z = tc_new_register(self);
  if value == 0 {
    tc_write24(self, /*xor r{z+8},r{z+8}*/ 12621, 192 | z << 3 | z);
  } else if value <= 4294967295 {
    tc_write16(self, /*mov r{z+8},value32*/ 47169 | (z << 8));
    tc_write32(self, value);
  } else {
    tc_write16(self, /*movabs r{z+8},value64*/ 47177 | (z << 8));
    *mem_allocate(8, &self.dst, self.dst_limit) = value;
  }
}
```

### Compiling binary expressions

Finding how to generate the machine code for a binary expression is mostly
straightforward, with the online tools mentionned above. Most expressions can be
compiled into a single x86_64 instruction. But a shift expression requires 2
instructions, and a division requires 4. This is because the `shl`, `shr` and
`div` instructions use some hardcoded registers as argument and/or result value,
and because of the "stack-based" register allocation strategy. It is thus
necessary to move some values between registers before and/or after performing
the actual operation.

The division is the most complex case. The `div` instruction divides the 128
bits value in rdx:rax by a given register, and stores the result in rdx:rax.
Dividing R*z+8* by R*y+8* and storing the result in R*z+8* thus requires to:

- set rdx to 0
- move the dividend from r*z+8* to rax
- divide it by the divisor in r*y+8*
- move the result from rax to r*z+8*

The first step can be done by setting edx to 0 (setting the lower 32 bits of a
64 bits register clears the higher 32 bits). This leads to the following code:

```rust
    tc_write16(self, /*xor edx,edx*/ 53809);
    tc_write24(self, /*mov rax,r{z+8}*/ 35148, 192 | z << 3);
    tc_write24(self, /*div r{y+8}*/ 63305, 240 | y);
    tc_write24(self, /*mov r{z+8},rax*/ 35145, 192 | z);
```

### Jump instructions

The `tc_write_jump_insn` function generates the code to jump to a not yet known
offset if registers R8 and R9 satisfy the comparison operator given by `token`:

```rust
static JUMP_INSTRUCTIONS = [130, 132, 135, 134, 133, 131];

fn tc_write_jump_insn(self: &Compiler, token: u64, placeholder_p: &&u64) {
  tc_write32(self, /*cmp r8,r9; j{xx} offset32*/ 264780109);
  tc_write8(self, load8(JUMP_INSTRUCTIONS + token - TC_LT));
  tc_write_placeholder(self, placeholder_p);
  self.next_register = 0;
}
```

For this it generates a `cmp` instruction to compare these registers, followed
by the start of a jump instruction corresponding to `token`: namely `jb`, `je`,
`ja`, `jbe`, `jne`, or `jae`, for `<`, `=,` `>`, `<=`, `!=`, and `>=`,
respectively. It then writes a 32 bits placeholder for the not yet known offset,
which corresponds to the end of the jump instruction.

### Load and store instructions

The `tc_write_load_insn` and `tc_write_store_insn` generate code to load a value
from memory in a register, and to store the value of a register in memory (as in
the original version). In x86_64 this can be done with a `mov` instruction
(unlike ARM, where `mov` is restricted to transfer between registers). However,
the encoding of these `mov` instructions is more complex than for a move between
registers. To simplify these functions and a few others, the following auxiliary
function is introduced:

```rust
fn tc_write_mem_insn(self: &Compiler, insn: u64, modrm: u64, sib: u64, slot: u64) {
  tc_write16(self, insn);
  if slot != 0 { modrm = modrm + 64; }
  if slot >= 16 { modrm = modrm + 64; }
  tc_write8(self, modrm);
  if sib != 0 { tc_write8(self, sib); }
  if slot >= 16 {
    tc_write32(self, slot * 8);
  } else if slot != 0 {
    tc_write8(self, slot * 8);
  }
}
```

This function generates instructions which are made of a 2 bytes prefix and
opcode (`insn`) followed by a ModR/M addressing-form specifier (`modrm`), an
optional Scale-Index-Base byte (`sib`, if not 0), and an optional displacement
(8 times `slot`, if not 0). The displacement is encoded in 1 byte if possible,
or 4 bytes otherwise (and the ModR/M specifier is adjusted accordingly). See
Section 2.2 of [[2]](#intelx86manualvol2) for more details.

With this, `tc_write_store_insn`, which stores the value of R9 at address R8 + 8
\* `field`, can be implemented very easily:

```rust
fn tc_write_store_insn(self: &Compiler, field: u64) {
  tc_write_mem_insn(self, /*mov [r8+offset],r9*/ 35149, 8, 0, field);
  self.next_register = 0;
}
```

The `tc_write_load_insn` function, which stores the value at address R*z+8* + 8
\* `field` in R*z+8*, is a bit more complex. This is because, probably due to
some historical reason, there is a special case for R12 (*z*=4). In this case a
Scale-Index-Base byte must follow the ModR/M (see note 1 below Table 2.2 in
[[2]](#intelx86manualvol2):

```rust
fn tc_write_load_insn(self: &Compiler, field: u64) {
  self.dst_mark = self.dst;
  let z = self.next_register - 1;
  let sib = 0;
  if z == 4 { sib = 36; }
  tc_write_mem_insn(self, /*mov r{z+8},[r{z+8}+offset]*/ 35661, z << 3 | z, sib, field);
}
```

Note also that this function saves the generated code pointer `dst` in
`dst_mark`, a new field added in the `Compiler` struct in this new version. This
is to make it easier to erase this instruction when needed, in
`tc_erase_load_insn` (the original version did not need this because
`tc_write_load_insn` was always generating a 2 bytes instruction):

```rust
fn tc_erase_load_insn(self: &Compiler) {
  self.dst = self.dst_mark;
}
```

The auxiliary `tc_write_mem_insn` function is also used in `tc_write_ptr_insn`,
`tc_write_get_insn`, and `tc_write_set_insn` to generate code related to local
variables (in the first case with a `lea` instruction instead of a `mov`). And
`dst_mark` is also used in `tc_write_get_insn`, in order to simplify the
implementation of `tc_erase_get_insn`.

### Return instruction

The `tc_write_return_insn` function generates code to return from a function.
This requires incrementing the stack pointer to make it point to the saved
returned address (see the figure in
[Registers and calling convention](#registers-and-calling-convention)), before
issuing a `ret` instruction. For this the following auxiliary function is
introduced:

```rust
fn tc_write_increment_insn(self: &Compiler, insn: u64, modrm: u64, slot: u64) {
  if slot >= 16 {
    tc_write24(self, insn - 512, modrm);
    tc_write32(self, slot * 8);
  } else if slot != 0 {
    tc_write24(self, insn, modrm);
    tc_write8(self, slot * 8);
  }
}
```

This function generates instructions which are made of a 2 bytes prefix and
opcode (`insn`) followed by a ModR/M addressing-form specifier (`modrm`), and an
optional displacement (8 times `slot`, if not 0). The displacement is encoded in
1 byte if possible, or 4 bytes otherwise (and the instruction prefix is adjusted
accordingly). Thanks to this helper function, the stack pointer can be
incremented by 8 times the frame size (a number of 64 bits slots) in
`tc_write_return_insn` as follows:

```rust
fn tc_write_return_insn(self: &Compiler) {
  tc_write_increment_insn(self, /*add rsp,offset*/ 33608, 196, self.frame_size);
  tc_write8(self, /*ret*/ 195);
  self.next_register = 0;
}
```

This helper function also makes it easier to implement
`tc_write_address_of_insn`, which needs to increment the value of R*z+8* with 8
times `field`.
