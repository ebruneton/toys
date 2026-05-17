fn mem_allocate(size: u64, ptr_p: &&u64, ptr_limit: &u64) -> &u64 {
  let ptr = *ptr_p;
  if size > ptr_limit as u64 || ptr > ptr_limit - size { panic(1); }
  *ptr_p = ptr + size;
  return ptr;
}

fn tc_write8(self: &Compiler, value: u64) {
  store8(mem_allocate(1, &self.dst, self.dst_limit), value);
}

fn tc_write16(self: &Compiler, value: u64) {
  store16(mem_allocate(2, &self.dst, self.dst_limit), value);
}

fn tc_write24(self: &Compiler, opcode: u64, args: u64) {
  tc_write16(self, opcode);
  tc_write8(self, args);
}

fn tc_write32(self: &Compiler, value: u64) {
  store32(mem_allocate(4, &self.dst, self.dst_limit), value);
}

fn tc_write_placeholder(self: &Compiler, placeholder_p: &&u64) {
  let new_placeholder = self.dst;
  let last_placeholder = *placeholder_p;
  *placeholder_p = new_placeholder;
  if last_placeholder == null { last_placeholder = new_placeholder; }
  tc_write32(self, new_placeholder - last_placeholder);
}

fn tc_fill_placeholders(placeholder: &u64, value: &u64) {
  let offset = 0;
  while placeholder != null {
    offset = load32(placeholder);
    store32(placeholder, value - (placeholder + 4));
    if offset == 0 { break; }
    placeholder = placeholder - offset;
  }
}

const MAX_REGISTERS: u64 = 8;

fn tc_new_register(self: &Compiler) -> u64 {
  let register = self.next_register;
  if register >= MAX_REGISTERS { panic(105); }
  self.next_register = register + 1;
  return register;
}

fn tc_save_registers(self: &Compiler, n: u64) {
  self.frame_size = self.frame_size + n;
  self.next_register = 0;
  while n > 0 {
    n = n - 1;
    tc_write16(self, /*push r{n+8}*/ 20545 | (n << 8));
  }
}

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

fn tc_write_static_insn(self: &Compiler, dst: u64) {
  let z = tc_new_register(self);
  tc_write24(self, /*lea r{z+8},[rip+offset32]*/ 36172, 5 | z << 3);
  tc_write32(self, dst as &u64 - (self.dst + 4));
}

fn tc_write_binary_insn(self: &Compiler, token: u64) {
  let y = self.next_register - 1;
  let z = y - 1;
  if token == TC_ADD {
    tc_write24(self, /*add r{z+8},r{y+8}*/ 845, 192 | z << 3 | y);
  } else if token == TC_SUB {
    tc_write24(self, /*sub r{z+8},r{y+8}*/ 11085, 192 | z << 3 | y);
  } else if token == TC_MUL {
    tc_write32(self, /*imul r{z+8},r{y+8}*/ 3232698189 | z << 27 | y << 24);
  } else if token == TC_DIV {
    tc_write16(self, /*xor edx,edx*/ 53809);
    tc_write24(self, /*mov rax,r{z+8}*/ 35148, 192 | z << 3);
    tc_write24(self, /*div r{y+8}*/ 63305, 240 | y);
    tc_write24(self, /*mov r{z+8},rax*/ 35145, 192 | z);
  } else if token == TC_BIT_AND {
    tc_write24(self, /*and r{z+8},r{y+8}*/ 9037, 192 | z << 3 | y);
  } else if token == TC_BIT_OR {
    tc_write24(self, /*or r{z+8},r{y+8}*/ 2893, 192 | z << 3 | y);
  } else if token == TC_SHIFT_LEFT {
    tc_write24(self, /*mov ecx,r{y+8}*/ 35140, 193 | y << 3);
    tc_write24(self, /*shl r{z+8},cl*/ 54089, 224 | z);
  } else {
    tc_write24(self, /*mov ecx,r{y+8}*/ 35140, 193 | y << 3);
    tc_write24(self, /*shr r{z+8},cl*/ 54089, 232 | z);
  }
  self.next_register = y;
}

static JUMP_INSTRUCTIONS = [130, 132, 135, 134, 133, 131];

fn tc_write_jump_insn(self: &Compiler, token: u64, placeholder_p: &&u64) {
  tc_write32(self, /*cmp r8,r9; j{xx} offset32*/ 264780109);
  tc_write8(self, load8(JUMP_INSTRUCTIONS + token - TC_LT));
  tc_write_placeholder(self, placeholder_p);
  self.next_register = 0;
}

fn tc_write_goto_insn(self: &Compiler, placeholder_p: &&u64) {
  tc_write8(self, /*jmp offset32*/ 233);
  tc_write_placeholder(self, placeholder_p);
}

fn tc_write_loop_insn(self: &Compiler, loop_dst: &u64) {
  tc_write8(self, /*jmp offset32*/ 233);
  tc_write32(self, loop_dst - (self.dst + 4));
}

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

fn tc_write_load_insn(self: &Compiler, field: u64) {
  self.dst_mark = self.dst;
  let z = self.next_register - 1;
  let sib = 0;
  if z == 4 { sib = 36; }
  tc_write_mem_insn(self, /*mov r{z+8},[r{z+8}+offset]*/ 35661, z << 3 | z, sib, field);
}

fn tc_erase_load_insn(self: &Compiler) {
  self.dst = self.dst_mark;
}

fn tc_write_store_insn(self: &Compiler, field: u64) {
  tc_write_mem_insn(self, /*mov [r8+offset],r9*/ 35149, 8, 0, field);
  self.next_register = 0;
}

fn tc_write_increment_insn(self: &Compiler, insn: u64, modrm: u64, slot: u64) {
  if slot >= 16 {
    tc_write24(self, insn - 512, modrm);
    tc_write32(self, slot * 8);
  } else if slot != 0 {
    tc_write24(self, insn, modrm);
    tc_write8(self, slot * 8);
  }
}

fn tc_write_address_of_insn(self: &Compiler, field: u64) {
  let z = self.next_register - 1;
  tc_write_increment_insn(self, /*add r{z+8},offset*/ 33609, 192 | z, field);
}

fn tc_write_ptr_insn(self: &Compiler, variable: u64) {
  let z = tc_new_register(self);
  tc_write_mem_insn(self, /*lea r{z+8},[rsp+offset]*/ 36172, 4 | z << 3, 36,
      self.frame_size - variable);
}

fn tc_write_get_insn(self: &Compiler, variable: u64) {
  self.dst_mark = self.dst;
  let z = tc_new_register(self);
  tc_write_mem_insn(self, /*mov r{z+8},[rsp+offset]*/ 35660, 4 | z << 3, 36,
      self.frame_size - variable);
}

fn tc_erase_get_insn(self: &Compiler) {
  self.dst = self.dst_mark;
  self.next_register = self.next_register - 1;
}

fn tc_write_set_insn(self: &Compiler, variable: u64) {
  tc_write_mem_insn(self, /*mov [rsp+offset],r8*/ 35148, 4, 36, self.frame_size - variable);
  self.next_register = 0;
}

fn tc_write_pop_insn(self: &Compiler) { self.next_register = 0; }

fn tc_write_fn_insn(self: &Compiler, arity: u64) {
  if arity > MAX_REGISTERS { panic(107); }
  self.frame_size = 0;
  tc_save_registers(self, arity);
}

fn tc_write_call_insn(self: &Compiler, function: &Symbol, saved_registers: u64) {
  tc_write8(self, /*call offset32*/ 232);
  if function.kind == SYM_FORWARD_FN {
    tc_write_placeholder(self, &function.value as &&u64);
  } else {
    tc_write32(self, function.value as &u64 - (self.dst + 4));
  }
  if function.type.kind != SYM_VOID {
    if saved_registers != 0 {
      tc_write24(self, /*mov r{z+8},r8*/ 35149, 192 | saved_registers);
    }
    if saved_registers >= MAX_REGISTERS { panic(108); }
    self.next_register = saved_registers + 1;
  } else {
    self.next_register = saved_registers;
  }
  self.frame_size = self.frame_size - saved_registers;
  let z = 0;
  while z < saved_registers {
    tc_write16(self, /*pop r{z+8}*/ 22593 | z << 8);
    z = z + 1;
  }
}

fn tc_write_return_insn(self: &Compiler) {
  tc_write_increment_insn(self, /*add rsp,offset*/ 33608, 196, self.frame_size);
  tc_write8(self, /*ret*/ 195);
  self.next_register = 0;
}
