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

#include "base.h"

u8* mem_allocate(uintptr_t size, u8** ptr_p, u8* ptr_limit) {
  u8* ptr = *ptr_p;
  if (size > (uintptr_t) ptr_limit || ptr > ptr_limit - size) { panic(1); }
  *ptr_p = ptr + size;
  return ptr;
}

void tc_write8(Compiler* self, u64 value) {
  store8(mem_allocate(1, &self->dst, self->dst_limit), value);
}

void tc_write16(Compiler* self, u64 value) {
  store16(mem_allocate(2, &self->dst, self->dst_limit), value);
}

void tc_write24(Compiler* self, u64 opcode, u64 args) {
  tc_write16(self, opcode);
  tc_write8(self, args);
}

void tc_write32(Compiler* self, u64 value) {
  store32(mem_allocate(4, &self->dst, self->dst_limit), value);
}

void tc_write_placeholder(Compiler* self, u8** placeholder_p) {
  u8* new_placeholder = self->dst;
  u8* last_placeholder = *placeholder_p;
  *placeholder_p = new_placeholder;
  if (last_placeholder == null) { last_placeholder = new_placeholder; }
  tc_write32(self, new_placeholder - last_placeholder);
}

void tc_fill_placeholders(u8* placeholder, u8* value) {
  u64 offset = 0;
  while (placeholder != null) {
    offset = load32(placeholder);
    store32(placeholder, value - (placeholder + 4));
    if (offset == 0) { break; }
    placeholder = placeholder - offset;
  }
}

const u64 MAX_REGISTERS = 8;

u64 tc_new_register(Compiler* self) {
  u64 reg = self->next_register;
  if (reg >= MAX_REGISTERS) { panic(105); }
  self->next_register = reg + 1;
  return reg;
}

void tc_save_registers(Compiler* self, u64 n) {
  self->frame_size = self->frame_size + n;
  self->next_register = 0;
  while (n > 0) {
    n = n - 1;
    tc_write16(self, /*push r{n+8}*/ 0x5041 | (n << 8));
  }
}

void tc_write_cst_insn(Compiler* self, u64 value) {
  u64 z = tc_new_register(self);
  if (value == 0) {
    tc_write24(self, /*xor r{z+8},r{z+8}*/ 0x314d, 0xc0 | z << 3 | z);
  } else if (value <= 0xFFFFFFFF) {
    tc_write16(self, /*mov r{z+8},value32*/ 0xB841 | (z << 8));
    tc_write32(self, value);
  } else {
    tc_write16(self, /*movabs r{z+8},value64*/ 0xB849 | (z << 8));
    *((u64*) mem_allocate(8, &self->dst, self->dst_limit)) = value;
  }
}

void tc_write_static_insn(Compiler* self, u8* dst) {
  u64 z = tc_new_register(self);
  tc_write24(self, /*lea r{z+8},[rip+offset32]*/ 0x8d4c, 0x05 | z << 3);
  tc_write32(self, dst - (self->dst + 4));
}

void tc_write_binary_insn(Compiler* self, u64 token) {
  u64 y = self->next_register - 1;
  u64 z = y - 1;
  if (token == TC_ADD) {
    tc_write24(self, /*add r{z+8},r{y+8}*/ 0x034d, 0xc0 | z << 3 | y);
  } else if (token == TC_SUB) {
    tc_write24(self, /*sub r{z+8},r{y+8}*/ 0x2b4d, 0xc0 | z << 3 | y);
  } else if (token == TC_MUL) {
    tc_write32(self, /*imul r{z+8},r{y+8}*/ 0xc0af0f4d | z << 27 | y << 24);
  } else if (token == TC_DIV) {
    tc_write16(self, /*xor edx,edx*/ 0xd231);
    tc_write24(self, /*mov rax,r{z+8}*/ 0x894c, 0xc0 | z << 3);
    tc_write24(self, /*div r{y+8}*/ 0xf749, 0xf0 | y);
    tc_write24(self, /*mov r{z+8},rax*/ 0x8949, 0xc0 | z);
  } else if (token == TC_BIT_AND) {
    tc_write24(self, /*and r{z+8},r{y+8}*/ 0x234d, 0xc0 | z << 3 | y);
  } else if (token == TC_BIT_OR) {
    tc_write24(self, /*or r{z+8},r{y+8}*/ 0x0b4d, 0xc0 | z << 3 | y);
  } else if (token == TC_SHIFT_LEFT) {
    tc_write24(self, /*mov ecx,r{y+8}*/ 0x8944, 0xc1 | y << 3);
    tc_write24(self, /*shl r{z+8},cl*/ 0xd349, 0xe0 | z);
  } else {
    tc_write24(self, /*mov ecx,r{y+8}*/ 0x8944, 0xc1 | y << 3);
    tc_write24(self, /*shr r{z+8},cl*/ 0xd349, 0xe8 | z);
  }
  self->next_register = y;
}

/* jb, je, ja, jbe, jne, jae */
static u8 JUMP_INSTRUCTIONS[] = {0x82, 0x84, 0x87, 0x86, 0x85, 0x83};

void tc_write_jump_insn(Compiler* self, u64 token, u8** placeholder_p) {
  tc_write32(self, /*cmp r8,r9; j{xx} offset32*/ 0x0fc8394d);
  tc_write8(self, load8(JUMP_INSTRUCTIONS + token - TC_LT));
  tc_write_placeholder(self, placeholder_p);
  self->next_register = 0;
}

void tc_write_goto_insn(Compiler* self, u8** placeholder_p) {
  tc_write8(self, /*jmp offset32*/ 0xe9);
  tc_write_placeholder(self, placeholder_p);
}

void tc_write_loop_insn(Compiler* self, u8* loop_dst) {
  tc_write8(self, /*jmp offset32*/ 0xe9);
  tc_write32(self, loop_dst - (self->dst + 4));
}

void tc_write_mem_insn(Compiler* self, u64 insn, u64 modrm, u64 sib, u64 slot) {
  tc_write16(self, insn);
  if (slot != 0) modrm = modrm + 0x40;
  if (slot >= 16) modrm = modrm + 0x40;
  tc_write8(self, modrm);
  if (sib != 0) tc_write8(self, sib);
  if (slot >= 16) {
    tc_write32(self, slot * 8);
  } else if (slot != 0) {
    tc_write8(self, slot * 8);
  }
}

void tc_write_load_insn(Compiler* self, u64 field) {
  self->dst_mark = self->dst;
  u64 z = self->next_register - 1;
  u64 sib = 0;
  if (z == 4) sib = 0x24;
  tc_write_mem_insn(self, /*mov r{z+8},[r{z+8}+offset]*/ 0x8b4d, z << 3 | z, sib, field);
}

void tc_erase_load_insn(Compiler* self) {
  self->dst = self->dst_mark;
}

void tc_write_store_insn(Compiler* self, u64 field) {
  tc_write_mem_insn(self, /*mov [r8+offset],r9*/ 0x894d, 0x8, 0, field);
  self->next_register = 0;
}

void tc_write_increment_insn(Compiler* self, u64 insn, u64 modrm, u64 slot) {
  if (slot >= 16) {
    tc_write24(self, insn - 512, modrm);
    tc_write32(self, slot * 8);
  } else if (slot != 0) {
    tc_write24(self, insn, modrm);
    tc_write8(self, slot * 8);
  }
}

void tc_write_address_of_insn(Compiler* self, u64 field) {
  u64 z = self->next_register - 1;
  tc_write_increment_insn(self, /*add r{z+8},offset*/ 0x8349, 0xc0 | z, field);
}

void tc_write_ptr_insn(Compiler* self, u64 variable) {
  u64 z = tc_new_register(self);
  tc_write_mem_insn(self, /*lea r{z+8},[rsp+offset]*/ 0x8d4c, 0x04 | z << 3, 0x24,
      self->frame_size - variable);
}

void tc_write_get_insn(Compiler* self, u64 variable) {
  self->dst_mark = self->dst;
  u64 z = tc_new_register(self);
  tc_write_mem_insn(self, /*mov r{z+8},[rsp+offset]*/ 0x8b4c, 0x04 | z << 3, 0x24,
      self->frame_size - variable);
}

void tc_erase_get_insn(Compiler* self) {
  self->dst = self->dst_mark;
  self->next_register = self->next_register - 1;
}

void tc_write_set_insn(Compiler* self, u64 variable) {
  tc_write_mem_insn(self, /*mov [rsp+offset],r8*/ 0x894c, 0x04, 0x24, self->frame_size - variable);
  self->next_register = 0;
}

void tc_write_pop_insn(Compiler* self) { self->next_register = 0; }

void tc_write_fn_insn(Compiler* self, u64 arity) {
  if (arity > MAX_REGISTERS) { panic(107); }
  self->frame_size = 0;
  tc_save_registers(self, arity);
}

void tc_write_call_insn(Compiler* self, Symbol* function, u64 saved_registers) {
  tc_write8(self, /*call offset32*/ 0xe8);
  if (function->kind == SYM_FORWARD_FN) {
    tc_write_placeholder(self, (u8**) &function->value);
  } else {
    tc_write32(self, (u8*) function->value - (self->dst + 4));
  }
  if (function->type->kind != SYM_VOID) {
    if (saved_registers != 0) {
      tc_write24(self, /*mov r{z+8},r8*/ 0x894d, 0xc0 | saved_registers);
    }
    if (saved_registers >= MAX_REGISTERS) { panic(108); }
    self->next_register = saved_registers + 1;
  } else {
    self->next_register = saved_registers;
  }
  self->frame_size = self->frame_size - saved_registers;
  u64 z = 0;
  while (z < saved_registers) {
    tc_write16(self, /*pop r{z+8}*/ 0x5841 | z << 8);
    z = z + 1;
  }
}

void tc_write_return_insn(Compiler* self) {
  tc_write_increment_insn(self, /*add rsp,offset*/ 0x8348, 0xc4, self->frame_size);
  tc_write8(self, /*ret*/ 0xc3);
  self->next_register = 0;
}

