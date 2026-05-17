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

#include <stddef.h>

#include "base.h"
#include "backend.h"
#include "scanner.h"

size_t nearest_multiple_of(size_t s, size_t a) {
  return s % a == 0 ? s : ((s / a) + 1) * a;
}

#define alignedsizeof(T) nearest_multiple_of(sizeof(T), sizeof(max_align_t))

Symbol* sym_lookup(Symbol* symbol, u8* name, u64 length) {
  while (symbol != null) {
    if (symbol->length == length && mem_compare(symbol->name, name, length) == 0) {
      return symbol;
    }
    symbol = symbol->next;
  }
  return null;
}

Symbol* tc_new_symbol(Compiler* self, u8* name, u64 length, u64 kind,
    uintptr_t value, Symbol* type, u64 dim, Symbol* next) {
  Symbol* symbol = (Symbol*) mem_allocate(alignedsizeof(Symbol), &self->heap, self->heap_limit);
  if (sym_lookup(next, name, length) != null) { panic(30); }
  symbol->name = name;
  symbol->length = length;
  symbol->kind = kind;
  symbol->value = value;
  symbol->type = type;
  symbol->dim = dim;
  symbol->next = next;
  return symbol;
}

Symbol* tc_add_symbol(Compiler* self, u8* name, u64 length, u64 kind,
    uintptr_t value, Symbol* type, u64 dim) {
  self->symbols = tc_new_symbol(
      self, name, length, kind, value, type, dim, self->symbols);
  return self->symbols;
}

Symbol* tc_add_or_resolve_fn_symbol(Compiler* self, u8* name, u64 length, uintptr_t value) {
  Symbol* symbol = sym_lookup(self->symbols, name, length);
  if (symbol == null) {
    return tc_add_symbol(self, name, length, SYM_FN, value, null, 0);
  }
  if (symbol->kind != SYM_FORWARD_FN) { panic(31); }
  tc_fill_placeholders((u8*) symbol->value, (u8*) value);
  symbol->kind = SYM_FN;
  symbol->value = value;
  return symbol;
}

void tc_parse_token(Compiler* self, u64 token) {
  if (self->next_token != token) { panic(20); }
  tc_read_token(self);
}

u64 tc_parse_integer(Compiler* self) {
  if (self->next_token != TC_INTEGER) { panic(21); }
  u64 value = (u64) self->next_token_data;
  tc_read_token(self);
  return value;
}

u8* tc_parse_identifier(Compiler* self, u64* length_p) {
  if (self->next_token != TC_IDENTIFIER) { panic(22); }
  u8* name = (u8*) self->next_token_data;
  *length_p = self->next_token_length;
  tc_read_token(self);
  return name;
}

Symbol* tc_parse_symbol(Compiler* self, Symbol* symbol) {
  u64 length = 0;
  u8* name = tc_parse_identifier(self, &length);
  symbol = sym_lookup(symbol, name, length);
  if (symbol == null) { panic(33); }
  return symbol;
}

Symbol* tc_parse_type(Compiler* self, u64* dim) {
  *dim = 0;
  while (self->next_token == TC_BIT_AND || self->next_token == TC_AND) {
    *dim = *dim + 1;
    if (self->next_token == TC_AND) { *dim = *dim + 1; }
    tc_read_token(self);
  }
  if (self->next_token == TC_U32) {
    tc_read_token(self);
    return null;
  }
  Symbol* symbol = tc_parse_symbol(self, self->symbols);
  if (*dim == 0 || symbol->kind != SYM_STRUCT) { panic(42); }
  return symbol;
}

void tc_parse_const(Compiler* self) {
  tc_parse_token(self, TC_CONST);
  u64 length = 0;
  u8* name = tc_parse_identifier(self, &length);
  tc_parse_token(self, ':');
  u64 dim = 0;
  Symbol* type = tc_parse_type(self, &dim);
  tc_parse_token(self, '=');
  tc_add_symbol(self, name, length, SYM_CONST, tc_parse_integer(self), type, dim);
  tc_parse_token(self, ';');
}

void tc_parse_static(Compiler* self) {
  tc_parse_token(self, TC_STATIC);
  u64 length = 0;
  u8* name = tc_parse_identifier(self, &length);
  tc_add_symbol(self, name, length, SYM_STATIC, (uintptr_t) self->dst, null, 1);
  tc_parse_token(self, '=');
  tc_parse_token(self, '[');
  tc_write8(self, tc_parse_integer(self));
  while (self->next_token == ',') {
    tc_read_token(self);
    tc_write8(self, tc_parse_integer(self));
  }
  tc_parse_token(self, ']');
  tc_parse_token(self, ';');
}

void tc_parse_struct(Compiler* self) {
  tc_parse_token(self, TC_STRUCT);
  u64 length = 0;
  u8* name = tc_parse_identifier(self, &length);
  Symbol* symbol = tc_add_symbol(self, name, length, SYM_STRUCT, 0, null, 0);
  tc_parse_token(self, '{');
  u64 dim = 0;
  Symbol* type = null;
  Symbol* fields = null;
  u64 value = 0;
  while (self->next_token != '}') {
    if (value > 0) { tc_parse_token(self, ','); }
    name = tc_parse_identifier(self, &length);
    tc_parse_token(self, ':');
    type = tc_parse_type(self, &dim);
    fields = tc_new_symbol(self, name, length, SYM_FIELD, value, type, dim, fields);
    value = value + 1;
  }
  tc_read_token(self);
  symbol->value = value;
  symbol->type = fields;
}

const u64 FROM_ADDRESS = 0;
const u64 FROM_VARIABLE = 1;
const u64 FROM_NULL = 2;
const u64 FROM_VOID = 3;
const u64 FROM_OTHER = 255;

typedef struct Value {
  u64 origin;
  u64 slot;
  Symbol* type;
  u64 dim;
} Value;

void value_type_check(Value* self, Symbol* type, u64 dim) {
  if (self->origin == FROM_NULL && dim != 0) { return; }
  if (self->type != type || self->dim != dim) { panic(43); }
}

void value_check(Value* self, Symbol* symbol) {
  value_type_check(self, symbol->type, symbol->dim);
}

Value* tc_push_value(Compiler* self, u64 origin, u64 slot, Symbol* type, u64 dim) {
  Value* value = (Value*) mem_allocate(alignedsizeof(Value), &self->heap, self->heap_limit);
  if (dim == 0 && type != null) { panic(44); }
  value->origin = origin;
  value->slot = slot;
  value->type = type;
  value->dim = dim;
  return value;
}

Value* tc_push_symbol_value(Compiler* self, Symbol* symbol) {
  u64 origin = FROM_OTHER;
  if (symbol->kind == SYM_FIELD) { origin = FROM_ADDRESS; }
  else if (symbol->kind == SYM_VARIABLE) { origin = FROM_VARIABLE; }
  else if (symbol->kind == SYM_VOID) { origin = FROM_VOID; }
  if (symbol->kind == SYM_FN) {
    return tc_push_value(self, origin, symbol->value, null, 0);
  }
  return tc_push_value(self, origin, symbol->value, symbol->type, symbol->dim);
}

Value* tc_top_value(Compiler* self) {
  return (Value*) (self->heap - alignedsizeof(Value));
}

Value* tc_pop_value_or_void(Compiler* self) {
  self->heap = self->heap - alignedsizeof(Value);
  return (Value*) self->heap;
}

Value* tc_pop_value(Compiler* self) {
  Value* value = tc_pop_value_or_void(self);
  if (value->origin == FROM_VOID) { panic(45); }
  return value;
}

void tc_parse_expr(Compiler* self);

void tc_check_fn_arguments(Compiler* self, Symbol* function, u64 argument_count) {
  Symbol* parameter = function->type->next;
  while (parameter != null && argument_count != 0) {
    value_check(tc_pop_value(self), parameter);
    parameter = parameter->next;
    argument_count = argument_count - 1;
  }
  if (parameter != null || argument_count != 0) { panic(46); }
  tc_push_symbol_value(self, function->type);
}

void tc_parse_fn_arguments(Compiler* self, Symbol* function) {
  if (function->kind != SYM_FN && function->kind != SYM_FORWARD_FN) {
    panic(34);
  }
  tc_parse_token(self, '(');
  u64 next_register = self->next_register;
  if (next_register != 0) {
    tc_save_registers(self, next_register);
  }
  u64 argument_count = 0;
  while (self->next_token != ')') {
    if (argument_count > 0) { tc_parse_token(self, ','); }
    tc_parse_expr(self);
    argument_count = argument_count + 1;
  }
  tc_read_token(self);
  tc_check_fn_arguments(self, function, argument_count);
  tc_write_call_insn(self, function, next_register);
}

void tc_parse_sizeof_expr(Compiler* self) {
  tc_parse_token(self, TC_SIZEOF);
  tc_parse_token(self, '(');
  Symbol* symbol = tc_parse_symbol(self, self->symbols);
  if (symbol->kind != SYM_STRUCT) { panic(47); }
  tc_push_value(self, FROM_OTHER, 0, null, 0);
  tc_write_cst_insn(self, symbol->value * 8);
  tc_parse_token(self, ')');
}

void tc_parse_primitive_expr(Compiler* self) {
  Symbol* symbol = null;
  if (self->next_token == TC_INTEGER) {
    tc_push_value(self, FROM_OTHER, 0, null, 0);
    tc_write_cst_insn(self, tc_parse_integer(self));
  } else if (self->next_token == TC_IDENTIFIER) {
    symbol = tc_parse_symbol(self, self->symbols);
    if (self->next_token == '(') {
      tc_parse_fn_arguments(self, symbol);
    } else {
      tc_push_symbol_value(self, symbol);
      if (symbol->kind == SYM_VARIABLE) {
        tc_write_get_insn(self, symbol->value);
      } else if (symbol->kind == SYM_CONST) {
        tc_write_cst_insn(self, symbol->value);
      } else if (symbol->kind == SYM_STATIC || symbol->kind == SYM_FN) {
        tc_write_static_insn(self, (u8*) symbol->value);
      } else {
        panic(35);
      }
    }
  } else if (self->next_token == TC_SIZEOF) {
    tc_parse_sizeof_expr(self);
  } else if (self->next_token == TC_NULL) {
    tc_read_token(self);
    tc_push_value(self, FROM_NULL, 0, null, 0);
    tc_write_cst_insn(self, 0);
  } else {
    tc_parse_token(self, '(');
    tc_parse_expr(self);
    tc_parse_token(self, ')');
  }
}

void tc_parse_path_expr(Compiler* self) {
  Value* value = null;
  Symbol* field = null;
  tc_parse_primitive_expr(self);
  while (self->next_token == '.') {
    tc_read_token(self);
    value = tc_pop_value(self);
    if (value->type == null || value->dim != 1) { panic(48); }
    field = tc_parse_symbol(self, value->type->type);
    tc_push_symbol_value(self, field);
    tc_write_load_insn(self, field->value);
  }
}

void tc_parse_pointer_expr(Compiler* self) {
  Value* value = null;
  if (self->next_token == TC_MUL) {
    tc_read_token(self);
    tc_parse_pointer_expr(self);
    value = tc_pop_value(self);
    if (value->dim == 0) { panic(49); }
    tc_push_value(self, FROM_ADDRESS, 0, value->type, value->dim - 1);
    tc_write_load_insn(self, 0);
  } else if (self->next_token == TC_BIT_AND) {
    tc_read_token(self);
    tc_parse_path_expr(self);
    value = tc_pop_value(self);
    if (value->origin == FROM_ADDRESS) {
      tc_erase_load_insn(self);
      tc_write_address_of_insn(self, value->slot);
    } else if (value->origin == FROM_VARIABLE) {
      tc_erase_get_insn(self);
      tc_write_ptr_insn(self, value->slot);
    } else {
      panic(36);
    }
    tc_push_value(self, FROM_OTHER, 0, value->type, value->dim + 1);
  } else {
    tc_parse_path_expr(self);
  }
}

void tc_parse_cast_expr(Compiler* self) {
  tc_parse_pointer_expr(self);
  if (self->next_token != TC_AS) { return; }
  tc_read_token(self);
  Value* value = tc_pop_value(self);
  if (value->origin == FROM_NULL) { value->origin = FROM_OTHER; }
  u64 dim = 0;
  Symbol* type = tc_parse_type(self, &dim);
  tc_push_value(self, value->origin, 0, type, dim);
}

void tc_check_integer_expr(Compiler* self) {
  Value* right_value = tc_pop_value(self);
  Value* left_value = tc_pop_value(self);
  if (left_value->type != null || left_value->dim != 0) { panic(50); }
  if (right_value->type != null || right_value->dim != 0) { panic(51); }
  tc_push_value(self, FROM_OTHER, 0, null, 0);
}

void tc_check_add_or_sub_expr(Compiler* self, u64 token) {
  Value* right_value = tc_pop_value(self);
  Value* left_value = tc_pop_value(self);
  if (right_value->dim == 0) {
    tc_push_value(self, FROM_OTHER, 0, left_value->type, left_value->dim);
  } else if (token == TC_SUB && left_value->dim != 0) {
    if (left_value->type != right_value->type) { panic(52); }
    if (left_value->dim != right_value->dim) { panic(52); }
    tc_push_value(self, FROM_OTHER, 0, null, 0);
  } else {
    panic(53);
  }
}

void tc_parse_mult_expr(Compiler* self) {
  tc_parse_cast_expr(self);
  u64 next_token = self->next_token;
  while (next_token == TC_MUL || next_token == TC_DIV) {
    tc_read_token(self);
    tc_parse_cast_expr(self);
    tc_check_integer_expr(self);
    tc_write_binary_insn(self, next_token);
    next_token = self->next_token;
  }
}

void tc_parse_add_expr(Compiler* self) {
  tc_parse_mult_expr(self);
  u64 next_token = self->next_token;
  while (next_token == TC_ADD || next_token == TC_SUB) {
    tc_read_token(self);
    tc_parse_mult_expr(self);
    tc_check_add_or_sub_expr(self, next_token);
    tc_write_binary_insn(self, next_token);
    next_token = self->next_token;
  }
}

void tc_parse_shift_expr(Compiler* self) {
  tc_parse_add_expr(self);
  u64 next_token = self->next_token;
  if (next_token == TC_SHIFT_LEFT || next_token == TC_SHIFT_RIGHT) {
    tc_read_token(self);
    tc_parse_add_expr(self);
    tc_check_integer_expr(self);
    tc_write_binary_insn(self, next_token);
  }
}

void tc_parse_bit_and_expr(Compiler* self) {
  tc_parse_shift_expr(self);
  while (self->next_token == TC_BIT_AND) {
    tc_read_token(self);
    tc_parse_shift_expr(self);
    tc_check_integer_expr(self);
    tc_write_binary_insn(self, TC_BIT_AND);
  }
}

void tc_parse_expr(Compiler* self) {
  tc_parse_bit_and_expr(self);
  while (self->next_token == TC_BIT_OR) {
    tc_read_token(self);
    tc_parse_bit_and_expr(self);
    tc_check_integer_expr(self);
    tc_write_binary_insn(self, TC_BIT_OR);
  }
}

void tc_check_comparison_expr(Compiler* self) {
  Value* right_value = tc_pop_value(self);
  Value* left_value = tc_pop_value(self);
  if (left_value->dim != 0 && right_value->origin == FROM_NULL) { return; }
  if (left_value->type != right_value->type ||
      left_value->dim != right_value->dim) {
    panic(54);
  }
}

u64 tc_parse_comparison_expr(Compiler* self) {
  tc_parse_expr(self);
  u64 token = self->next_token;
  if (token < TC_LT || token > TC_GE) { panic(25); }
  tc_read_token(self);
  tc_parse_expr(self);
  tc_check_comparison_expr(self);
  return token;
}

u64 tc_parse_and_expr(Compiler* self, u8** else_refs_p) {
  u64 token = tc_parse_comparison_expr(self);
  while (self->next_token == TC_AND) {
    tc_read_token(self);
    tc_write_jump_insn(self, TC_LT + TC_GE - token, else_refs_p);
    token = tc_parse_comparison_expr(self);
  }
  return token;
}

u8* tc_parse_boolean_expr(Compiler* self, u8** then_refs_p) {
  u8* else_refs = null;
  u64 token = tc_parse_and_expr(self, &else_refs);
  while (self->next_token == TC_OR) {
    tc_read_token(self);
    tc_write_jump_insn(self, token, then_refs_p);
    tc_fill_placeholders(else_refs, self->dst);
    else_refs = null;
    token = tc_parse_and_expr(self, &else_refs);
  }
  tc_write_jump_insn(self, TC_LT + TC_GE - token, &else_refs);
  return else_refs;
}

const u64 END_UNREACHABLE = 0;
const u64 END_REACHABLE = 1;

u64 tc_parse_stmt(Compiler* self, u8** break_refs_p);

u64 tc_parse_block_stmt(Compiler* self, u8** break_refs_p) {
  u64 state = END_REACHABLE;
  tc_parse_token(self, '{');
  while (self->next_token != '}') {
    if (state == END_UNREACHABLE) { panic(37); }
    state = tc_parse_stmt(self, break_refs_p);
  }
  tc_read_token(self);
  return state;
}

u64 tc_parse_assignment(Compiler* self) {
  Value* value = tc_top_value(self);
  u64 origin = value->origin;
  u64 slot = value->slot;
  if (origin == FROM_ADDRESS) {
    tc_erase_load_insn(self);
  } else if (origin == FROM_VARIABLE) {
    tc_erase_get_insn(self);
  } else {
    panic(38);
  }
  tc_parse_token(self, '=');
  tc_parse_expr(self);
  tc_check_comparison_expr(self);
  if (origin == FROM_ADDRESS) {
    tc_write_store_insn(self, slot);
  } else {
    tc_write_set_insn(self, slot);
  }
  return END_REACHABLE;
}

u64 tc_parse_expr_or_assign_stmt(Compiler* self) {
  tc_parse_expr(self);
  if (self->next_token == '=') {
    tc_parse_assignment(self);
  } else if (tc_pop_value_or_void(self)->origin != FROM_VOID) {
    tc_write_pop_insn(self);
  }
  tc_parse_token(self, ';');
  return END_REACHABLE;
}

u64 tc_parse_return_stmt(Compiler* self) {
  tc_parse_token(self, TC_RETURN);
  if (self->next_token == ';') {
    if (self->fn_return_type->kind != SYM_VOID) { panic(55); }
  } else {
    if (self->fn_return_type->kind == SYM_VOID) { panic(56); }
    tc_parse_expr(self);
    value_check(tc_pop_value(self), self->fn_return_type);
  }
  tc_parse_token(self, ';');
  tc_write_return_insn(self);
  return END_UNREACHABLE;
}

u64 tc_parse_break_stmt(Compiler* self, u8** break_refs_p) {
  tc_parse_token(self, TC_BREAK);
  tc_parse_token(self, ';');
  tc_write_goto_insn(self, break_refs_p);
  return END_UNREACHABLE;
}

u64 tc_parse_while_or_loop_stmt(Compiler* self) {
  u8* loop_dst = self->dst;
  u8* body_refs = null;
  u8* end_refs = null;
  u64 token = self->next_token;
  tc_read_token(self);
  if (token == TC_WHILE) {
    end_refs = tc_parse_boolean_expr(self, &body_refs);
  }
  tc_fill_placeholders(body_refs, self->dst);
  tc_parse_block_stmt(self, &end_refs);
  tc_write_loop_insn(self, loop_dst);
  tc_fill_placeholders(end_refs, self->dst);
  if (token == TC_LOOP && end_refs == null) { return END_UNREACHABLE; }
  return END_REACHABLE;
}

u64 tc_parse_if_stmt(Compiler* self, u8** break_refs_p) {
  tc_parse_token(self, TC_IF);
  u8* then_refs = null;
  u8* else_refs = tc_parse_boolean_expr(self, &then_refs);
  tc_fill_placeholders(then_refs, self->dst);
  u64 state = tc_parse_block_stmt(self, break_refs_p);
  u8* end_if_refs = null;
  if (self->next_token == TC_ELSE) {
    tc_read_token(self);
    if (state == END_REACHABLE) {
      tc_write_goto_insn(self, &end_if_refs);
    }
    tc_fill_placeholders(else_refs, self->dst);
    if (self->next_token == '{') {
      state = state | tc_parse_block_stmt(self, break_refs_p);
    } else {
      state = state | tc_parse_if_stmt(self, break_refs_p);
    }
    tc_fill_placeholders(end_if_refs, self->dst);
  } else {
    tc_fill_placeholders(else_refs, self->dst);
    state = END_REACHABLE;
  }
  return state;
}

u64 tc_parse_stmt(Compiler* self, u8** break_refs_p) {
  if (self->next_token == TC_IF) {
    return tc_parse_if_stmt(self, break_refs_p);
  } else if (self->next_token == TC_WHILE || self->next_token == TC_LOOP) {
    return tc_parse_while_or_loop_stmt(self);
  } else if (self->next_token == TC_BREAK) {
    if (break_refs_p == null) { panic(39); }
    return tc_parse_break_stmt(self, break_refs_p);
  } else if (self->next_token == TC_RETURN) {
    return tc_parse_return_stmt(self);
  }
  return tc_parse_expr_or_assign_stmt(self);
}

u64 tc_parse_let_stmt(Compiler* self, u64 variable) {
  tc_parse_token(self, TC_LET);
  u64 length = 0;
  u8* name = tc_parse_identifier(self, &length);
  u64 separator = self->next_token;
  Symbol* type = null;
  u64 dim = 0;
  if (separator == ':') {
    tc_read_token(self);
    type = tc_parse_type(self, &dim);
  }
  tc_parse_token(self, '=');
  tc_parse_expr(self);
  tc_parse_token(self, ';');
  Value* value = tc_pop_value(self);
  if (separator == ':') {
    value_type_check(value, type, dim);
  } else {
    if (value->origin == FROM_NULL) { panic(57); }
    type = value->type;
    dim = value->dim;
  }
  tc_add_symbol(self, name, length, SYM_VARIABLE, variable, type, dim);
  tc_save_registers(self, 1);
  return variable + 1;
}

Symbol* tc_parse_fn_name(Compiler* self) {
  u64 length = 0;
  u8* name = tc_parse_identifier(self, &length);
  return tc_add_or_resolve_fn_symbol(self, name, length, (uintptr_t) self->dst);
}

void tc_check_fn_parameters(Symbol* forward_parameters, Symbol* parameters) {
  if (forward_parameters == null) { return; }
  while (forward_parameters != null && parameters != null) {
    if (forward_parameters->kind != parameters->kind ||
        forward_parameters->type != parameters->type ||
        forward_parameters->dim != parameters->dim) {
      panic(58);
    }
    forward_parameters = forward_parameters->next;
    parameters = parameters->next;
  }
  if (forward_parameters != null || parameters != null) { panic(59); }
}

u64 tc_parse_fn_parameters(Compiler* self, Symbol* function) {
  u64 i = 0;
  u8* name = null;
  u64 length = 0;
  Symbol* type = null;
  u64 dim = 0;
  Symbol* symbols = null;
  tc_parse_token(self, '(');
  while (self->next_token != ')') {
    if (i > 0) { tc_parse_token(self, ','); }
    name = tc_parse_identifier(self, &length);
    tc_parse_token(self, ':');
    type = tc_parse_type(self, &dim);
    symbols = tc_new_symbol(self, name, length, SYM_VARIABLE, i, type, dim, symbols);
    i = i + 1;
  }
  tc_read_token(self);
  if (self->next_token == TC_ARROW) {
    tc_read_token(self);
    type = tc_parse_type(self, &dim);
    symbols = tc_new_symbol(self, null, 0, SYM_VARIABLE, 0, type, dim, symbols);
  } else {
    symbols = tc_new_symbol(self, null, 0, SYM_VOID, 0, null, 0, symbols);
  }
  tc_check_fn_parameters(function->type, symbols);
  function->type = symbols;
  self->fn_return_type = symbols;
  return i;
}

void tc_parse_fn_asm_body(Compiler* self) {
  u64 i = 0;
  tc_parse_token(self, '[');
  while (self->next_token != ']') {
    if (i > 0) tc_parse_token(self, ',');
    tc_write8(self, tc_parse_integer(self));
    i = i + 1;
  }
  tc_read_token(self);
}

void tc_parse_fn_body(Compiler* self, Symbol* function, u64 arity) {
  if (self->next_token == ';') {
    tc_read_token(self);
    function->kind = SYM_FORWARD_FN;
    function->value = 0;
    return;
  }
  if (self->next_token == '[') {
    tc_parse_fn_asm_body(self);
    return;
  }
  Symbol* parameter = function->type->next;
  while (parameter != null) {
    tc_add_symbol(self, parameter->name, parameter->length, SYM_VARIABLE,
        arity - parameter->value, parameter->type, parameter->dim);
    parameter = parameter->next;
  }
  tc_parse_token(self, '{');
  tc_write_fn_insn(self, arity);
  u64 next_variable = arity + 1;
  u64 state = END_REACHABLE;
  while (self->next_token != '}') {
    if (state == END_UNREACHABLE) { panic(40); }
    if (self->next_token == TC_CONST) {
      tc_parse_const(self);
    } else if (self->next_token == TC_LET) {
      next_variable = tc_parse_let_stmt(self, next_variable);
    } else {
      state = tc_parse_stmt(self, null);
    }
  }
  if (state == END_REACHABLE) {
    if (self->fn_return_type->kind != SYM_VOID) { panic(41); }
    tc_write_return_insn(self);
  }
  tc_read_token(self);
}

void tc_check_symbols(Symbol* symbol, Symbol* end_symbol) {
  while (symbol != end_symbol) {
    if (symbol->kind == SYM_FORWARD_FN) { panic(32); }
    symbol = symbol->next;
  }
}

void tc_parse_fn(Compiler* self) {
  tc_parse_token(self, TC_FN);
  Symbol* function = tc_parse_fn_name(self);
  u64 arity = tc_parse_fn_parameters(self, function);
  u8* heap = self->heap;
  Symbol* symbols = self->symbols;
  tc_parse_fn_body(self, function, arity);
  self->symbols = symbols;
  self->heap = heap;
}

void tc_parse_program(Compiler* self) {
  while (true) {
    if (self->next_token == TC_FN) {
      tc_parse_fn(self);
    } else if (self->next_token == TC_STRUCT) {
      tc_parse_struct(self);
    } else if (self->next_token == TC_STATIC) {
      tc_parse_static(self);
    } else if (self->next_token == TC_CONST) {
      tc_parse_const(self);
    } else {
      if (self->next_token != 0) { panic(23); }
      return;
    }
  }
}

