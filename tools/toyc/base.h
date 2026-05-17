/**
 * Copyright (c) 2026 Eric Bruneton
 * All rights reserved.
 *
 * This file is part of Toys (https://github.com/ebruneton/toys).
 *
 * Toys is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * Toys is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Toys. If not, see <https://www.gnu.org/licenses/>
 */

#ifndef _BASE_H_
#define _BASE_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define null NULL

typedef uint8_t u8;
typedef uint64_t u64;

inline u64 load8(u8 *ptr) { return (u64)*ptr; }
inline u64 load16(u8 *ptr) { return (load8(ptr + 1) << 8) + load8(ptr); }
inline u64 load32(u8 *ptr) { return (load16(ptr + 2) << 16) + load16(ptr); }

inline void store8(u8 *ptr, u64 value) { *ptr = (u8)value; }

inline void store16(u8 *ptr, u64 value) {
  *ptr = value;
  *(ptr + 1) = value >> 8;
}

inline void store32(u8 *ptr, u64 value) {
  *ptr = value;
  *(ptr + 1) = value >> 8;
  *(ptr + 2) = value >> 16;
  *(ptr + 3) = value >> 24;
}

void panic(u64 error_code);

#define TC_INTEGER 2
#define TC_IDENTIFIER 3
#define TC_ADD 4
#define TC_SUB 5
#define TC_MUL 6
#define TC_DIV 7
#define TC_BIT_AND 8
#define TC_BIT_OR 9
#define TC_SHIFT_LEFT 10
#define TC_SHIFT_RIGHT 11
#define TC_LT 12
#define TC_GE 17
#define TC_AND 18
#define TC_OR 19
#define TC_ARROW 20
#define TC_AS 138
#define TC_BREAK 128
#define TC_CONST 129
#define TC_ELSE 130
#define TC_FN 131
#define TC_IF 132
#define TC_LET 133
#define TC_LOOP 134
#define TC_NULL 139
#define TC_RETURN 135
#define TC_SIZEOF 140
#define TC_STATIC 136
#define TC_STRUCT 141
#define TC_U32 142
#define TC_WHILE 137

#define SYM_FN 0
#define SYM_FORWARD_FN 1
#define SYM_VARIABLE 2
#define SYM_CONST 3
#define SYM_STATIC 4
#define SYM_STRUCT 5
#define SYM_FIELD 6
#define SYM_VOID 7

typedef struct Symbol {
  u8* name;
  u64 length;
  u64 kind;
  uintptr_t value;
  struct Symbol* type;
  u64 dim;
  struct Symbol* next;
} Symbol;

typedef struct Compiler {
  u8* src;
  u8* src_end;
  u8 next_char;
  u64 next_char_type;
  u64 next_token;
  uintptr_t next_token_data;
  u64 next_token_length;
  u8* dst;
  u8* dst_mark;
  u8* dst_limit;
  u8* heap;
  u8* heap_limit;
  Symbol* symbols;
  Symbol* fn_return_type;
  u64 next_register;
  u64 frame_size;
} Compiler;

#endif
