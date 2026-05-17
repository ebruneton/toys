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

#ifndef _BACKEND_H_
#define _BACKEND_H_

#include "base.h"

u8* mem_allocate(uintptr_t size, u8** ptr_p, u8* ptr_limit);

void tc_fill_placeholders(u8* placeholder, u8* value);

void tc_write8(Compiler* self, u64 value);
void tc_save_registers(Compiler* self, u64 n);
void tc_write_cst_insn(Compiler* self, u64 value);
void tc_write_static_insn(Compiler* self, u8* dst);
void tc_write_binary_insn(Compiler* self, u64 token);
void tc_write_jump_insn(Compiler* self, u64 token, u8** placeholder_p);
void tc_write_goto_insn(Compiler* self, u8** placeholder_p);
void tc_write_loop_insn(Compiler* self, u8* loop_dst);
void tc_write_mem_insn(Compiler* self, u64 insn, u64 modrm, u64 sib, u64 slot);
void tc_write_load_insn(Compiler* self, u64 field);
void tc_erase_load_insn(Compiler* self);
void tc_write_store_insn(Compiler* self, u64 field);
void tc_write_increment_insn(Compiler* self, u64 insn, u64 modrm, u64 offset);
void tc_write_address_of_insn(Compiler* self, u64 field);
void tc_write_ptr_insn(Compiler* self, u64 variable);
void tc_write_get_insn(Compiler* self, u64 variable);
void tc_erase_get_insn(Compiler* self);
void tc_write_set_insn(Compiler* self, u64 variable);
void tc_write_pop_insn(Compiler* self);
void tc_write_fn_insn(Compiler* self, u64 arity);
void tc_write_call_insn(Compiler* self, Symbol* function, u64 saved_registers);
void tc_write_return_insn(Compiler* self);

#endif
