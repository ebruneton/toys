struct DiskBlock {
  next_block: u64,
  next_file: u64,
  name_length: u64,
  name: u64
}

struct Disk {
  id: u64,
  io: &BlockIoProtocol,
  block_buffer: &DiskBlock
}

fn block_get_next(self: &DiskBlock) -> u64 {
  if self.next_block <= 512 { return 0; }
  return self.next_block - 512;
}

fn block_set_next(self: &DiskBlock, next_block: u64) {
  self.next_block = next_block + 512;
}

fn block_size(self: &DiskBlock) -> &u64 {
  return &self.next_block;
}

fn disk_new(heap_p: &&u64, heap_limit: &u64, io: &BlockIoProtocol) -> &Disk {
  let disk = mem_allocate(sizeof(Disk), heap_p, heap_limit) as &Disk;
  let block_buffer = mem_allocate(512, heap_p, heap_limit) as &DiskBlock;
  if disk != null && block_buffer != null {
    disk.id = (*io.media << 32) >> 32;
    disk.io = io;
    disk.block_buffer = block_buffer;
  }
  return disk;
}

const SUPER_BLOCK: u64 = 0;
const FIRST_LOGICAL_BLOCK_ADDRESS: u64 = 16;

const EQUAL: u64 = 0;
const SMALLER: u64 = 1;
const GREATER: u64 = 2;

fn disk_compare_file_name(file: &DiskBlock, name: &u64, length: u64) -> u64 {
  let file_name_length = file.name_length;
  let file_name = &file.name;
  let i = 0;
  while i < file_name_length && i < length {
    if load8(file_name + i) < load8(name + i) { return SMALLER; }
    if load8(file_name + i) > load8(name + i) { return GREATER; }
    i = i + 1;
  }
  if file_name_length < length { return SMALLER; }
  if file_name_length > length { return GREATER; }
  return EQUAL;
}

fn disk_read_block_buffer(self: &Disk, block: u64) -> &DiskBlock {
  let io = self.io;
  let buffer = self.block_buffer;
  let lba = block + FIRST_LOGICAL_BLOCK_ADDRESS;
  if efi_call5(io.read_blocks, io as u64, self.id, lba, 512, buffer as u64) != 0 {
    buffer.next_block = 0;
    buffer.next_file = 0;
    buffer.name_length = 0;
  }
  return buffer;
}

fn disk_write_block_buffer(self: &Disk, block: u64) {
  let io = self.io;
  let lba = block + FIRST_LOGICAL_BLOCK_ADDRESS;
  efi_call5(io.write_blocks, io as u64, self.id, lba, 512, self.block_buffer as u64);
}

fn disk_flush_blocks(self: &Disk) {
  efi_call1(self.io.flush_blocks, self.io as u64);
}

fn disk_find_file(self: &Disk, name: &u64, length: u64, previous_file: &u64) -> u64 {
  let block_buffer = disk_read_block_buffer(self, SUPER_BLOCK);
  let file = block_buffer.next_file;
  let file_name = SMALLER;
  while file != 0 {
    disk_read_block_buffer(self, file);
    file_name = disk_compare_file_name(block_buffer, name, length);
    if file_name == EQUAL { return file; }
    if file_name == GREATER { return 0; }
    if previous_file != null { *previous_file = file; }
    file = block_buffer.next_file;
  }
  return 0;
}

fn disk_get_file_size(self: &Disk, file: u64) -> u64 {
  let file_size = 0;
  let block_buffer = disk_read_block_buffer(self, file);
  let header_size = 24 + block_buffer.name_length;
  loop {
    if block_get_next(block_buffer) == 0 {
      return file_size + *block_size(block_buffer) - header_size;
    }
    file_size = file_size + 512 - header_size;
    disk_read_block_buffer(self, block_get_next(block_buffer));
    header_size = 8;
  }
}

fn mem_copy_non_overlapping(src: &u64, dst: &u64, size: u64) {
  let i = 0;
  while i < size {
    store8(dst + i, load8(src + i));
    i = i + 1;
  }
}

fn disk_read_file(self: &Disk, block: &u64, offset: &u64, dst: &u64, size: u64) -> u64 {
  let block_buffer = self.block_buffer;
  let available = 0;
  loop {
    disk_read_block_buffer(self, *block);
    if block_get_next(block_buffer) == 0 {
      available = *block_size(block_buffer) - *offset;
    } else {
      available = 512 - *offset;
    }
    if available >= size {
      mem_copy_non_overlapping(block_buffer as &u64 + *offset, dst, size);
      *offset = *offset + size;
      return 0;
    }
    mem_copy_non_overlapping(block_buffer as &u64 + *offset, dst, available);
    if block_get_next(block_buffer) == 0 {
      return size - available;
    }
    *block = block_get_next(block_buffer);
    *offset = 8;
    dst = dst + available;
    size = size - available;
  }
}

const OK: u64 = 0;
const INVALID_ARGUMENT: u64 = 1;
const INVALID_STATE: u64 = 2;
const NOT_FOUND: u64 = 3;
const ALREADY_EXISTS: u64 = 4;
const OUT_OF_MEMORY: u64 = 5;
const INTERNAL_ERROR: u64 = 6;

fn error_result(error: u64) -> u64 { return error << 56; }

fn disk_create_file(self: &Disk, name: &u64, length: u64, result: &u64) -> u64 {
  if length == 0 || length > 488 { return INVALID_ARGUMENT; }
  let i = 0;
  while i < length {
    if load8(name + i) <= 32 || load8(name + i) >= 127 {
      return INVALID_ARGUMENT;
    }
    i = i + 1;
  }
  let previous_file = 0;
  let block_buffer = disk_read_block_buffer(self, SUPER_BLOCK);
  let new_file = block_get_next(block_buffer);
  if new_file == 0 { return OUT_OF_MEMORY; }
  let new_first_file = block_buffer.next_file;
  let file = block_buffer.next_file;
  let file_name = SMALLER;
  while file != 0 {
    disk_read_block_buffer(self, file);
    file_name = disk_compare_file_name(block_buffer, name, length);
    if file_name == EQUAL { return ALREADY_EXISTS; }
    if file_name == GREATER { break; }
    previous_file = file;
    file = block_buffer.next_file;
  }
  if previous_file != 0 {
    disk_read_block_buffer(self, previous_file);
    block_buffer.next_file = new_file;
    disk_write_block_buffer(self, previous_file);
  } else {
    new_first_file = new_file;
  }
  disk_read_block_buffer(self, new_file);
  let new_first_free_block = block_get_next(block_buffer);
  *block_size(block_buffer) = 24 + length;
  block_buffer.next_file = file;
  block_buffer.name_length = length;
  mem_copy_non_overlapping(name, &block_buffer.name, length);
  disk_write_block_buffer(self, new_file);
  disk_read_block_buffer(self, SUPER_BLOCK);
  block_buffer.next_file = new_first_file;
  block_set_next(block_buffer, new_first_free_block);
  disk_write_block_buffer(self, SUPER_BLOCK);
  disk_flush_blocks(self);
  *result = new_file;
  return OK;
}

fn disk_write_file(self: &Disk, block: &u64, src: &u64, size: u64) -> u64 {
  let block_buffer = disk_read_block_buffer(self, SUPER_BLOCK);
  let old_first_free_block = block_get_next(block_buffer);
  let new_first_free_block = old_first_free_block;
  disk_read_block_buffer(self, *block);
  let used = *block_size(block_buffer);
  let free = 512 - used;
  let result = OK;
  loop {
    if size <= free {
      *block_size(block_buffer) = used + size;
      mem_copy_non_overlapping(src, block_buffer as &u64 + used, size);
      disk_write_block_buffer(self, *block);
      break;
    }
    if new_first_free_block == 0 {
      result = OUT_OF_MEMORY;
      break;
    }
    block_set_next(block_buffer, new_first_free_block);
    mem_copy_non_overlapping(src, block_buffer as &u64 + used, free);
    disk_write_block_buffer(self, *block);
    *block = new_first_free_block;
    disk_read_block_buffer(self, *block);
    new_first_free_block = block_get_next(block_buffer);
    src = src + free;
    size = size - free;
    used = 8;
    free = 504;
  }
  if new_first_free_block != old_first_free_block {
    disk_read_block_buffer(self, SUPER_BLOCK);
    block_set_next(block_buffer, new_first_free_block);
    disk_write_block_buffer(self, SUPER_BLOCK);
  }
  disk_flush_blocks(self);
  return result;
}

fn disk_delete_file_blocks(self: &Disk, block: u64) {
  let block_buffer = disk_read_block_buffer(self, SUPER_BLOCK);
  let first_free_block = block_get_next(block_buffer);
  let last_block = block;
  disk_read_block_buffer(self, last_block);
  while block_get_next(block_buffer) != 0 {
    last_block = block_get_next(block_buffer);
    disk_read_block_buffer(self, last_block);
  }
  block_set_next(block_buffer, first_free_block);
  disk_write_block_buffer(self, last_block);
}

fn disk_clear_file(self: &Disk, file: u64) {
  let block_buffer = disk_read_block_buffer(self, file);
  let new_first_free_block = block_get_next(block_buffer);
  if new_first_free_block != 0 {
    disk_delete_file_blocks(self, new_first_free_block);
    disk_read_block_buffer(self, SUPER_BLOCK);
    block_set_next(block_buffer, new_first_free_block);
    disk_write_block_buffer(self, SUPER_BLOCK);
  }
  disk_read_block_buffer(self, file);
  *block_size(block_buffer) = 24 + block_buffer.name_length;
  disk_write_block_buffer(self, file);
  disk_flush_blocks(self);
}

fn disk_delete_file(self: &Disk, file: u64, previous_file: u64) {
  let block_buffer = disk_read_block_buffer(self, file);
  let next_file = block_buffer.next_file;
  disk_read_block_buffer(self, SUPER_BLOCK);
  let new_first_file = block_buffer.next_file;
  disk_delete_file_blocks(self, file);
  if previous_file != 0 {
    disk_read_block_buffer(self, previous_file);
    block_buffer.next_file = next_file;
    disk_write_block_buffer(self, previous_file);
  } else {
    new_first_file = next_file;
  }
  disk_read_block_buffer(self, SUPER_BLOCK);
  block_set_next(block_buffer, file);
  block_buffer.next_file = new_first_file;
  disk_write_block_buffer(self, SUPER_BLOCK);
  disk_flush_blocks(self);
}
