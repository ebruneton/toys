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

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "filesystem.h"

static_assert(sizeof(DiskBlock) == 512);

u64_t from_u64(u64 x) {
  u64_t result;
  for (int i = 0; i < 8; ++i) {
    result.bytes[i] = (u8) (x >> (8 * i));
  }
  return result;
}

u64 to_u64(u64_t x) {
  u64 result = 0;
  for (int i = 0; i < 8; ++i) {
    result |= ((u64) x.bytes[i]) << (8 * i);
  }
  return result;
}

u64 block_get_next(DiskBlock* self) {
  if (to_u64(self->next_block) <= 512) { return 0; }
  return to_u64(self->next_block) - 512;
}

void block_set_next(DiskBlock* self, u64 next_block) {
  self->next_block = from_u64(next_block + 512);
}

u64_t* block_size(DiskBlock* self) {
  return &self->next_block;
}

const u64 EQUAL = 0;
const u64 SMALLER = 1;
const u64 GREATER = 2;

u64 disk_compare_file_name(DiskBlock* file, u8* name, u64 length) {
  u64 file_name_length = to_u64(file->name_length);
  u8* file_name = file->name;
  u64 i = 0;
  while (i < file_name_length && i < length) {
    if (load8(file_name + i) < load8(name + i)) { return SMALLER; }
    if (load8(file_name + i) > load8(name + i)) { return GREATER; }
    i = i + 1;
  }
  if (file_name_length < length) { return SMALLER; }
  if (file_name_length > length) { return GREATER; }
  return EQUAL;
}

DiskBlock* disk_read_block_buffer(Disk* self, u64 block_id) {
  memcpy((u8*) self->block_buffer, self->image + block_id * 512, 512);
  return self->block_buffer;
}

void disk_write_block_buffer(Disk* self, u64 block_id) {
  memcpy(self->image + block_id * 512, (u8*) self->block_buffer, 512);
}

void disk_flush_blocks(Disk* self) {
  (void) self;
}

u64 disk_find_file(Disk* self, u8* name, u64 length, u64* previous_file) {
  DiskBlock* block_buffer = disk_read_block_buffer(self, SUPER_BLOCK);
  u64 file = to_u64(block_buffer->next_file);
  u64 file_name = SMALLER;
  while (file != 0) {
    disk_read_block_buffer(self, file);
    file_name = disk_compare_file_name(block_buffer, name, length);
    if (file_name == EQUAL) { return file; }
    if (file_name == GREATER) { return 0; }
    if (previous_file != null) { *previous_file = file; }
    file = to_u64(block_buffer->next_file);
  }
  return 0;
}

u64 disk_get_file_size(Disk* self, u64 file) {
  u64 file_size = 0;
  DiskBlock* block_buffer = disk_read_block_buffer(self, file);
  u64 header_size = 24 + to_u64(block_buffer->name_length);
  while (true) {
    if (block_get_next(block_buffer) == 0) {
      return file_size + to_u64(*block_size(block_buffer)) - header_size;
    }
    file_size = file_size + 512 - header_size;
    disk_read_block_buffer(self, block_get_next(block_buffer));
    header_size = 8;
  }
}

void mem_copy_non_overlapping(u8* src, u8* dst, u64 size) {
  memcpy(dst, src, size);
}

u64 disk_read_file(Disk *self, u64* block, u64* offset, u8* dst, u64 size) {
  DiskBlock* block_buffer = self->block_buffer;
  u64 available = 0;
  while (true) {
    disk_read_block_buffer(self, *block);
    if (block_get_next(block_buffer) == 0) {
      available = to_u64(*block_size(block_buffer)) - *offset;
    } else {
      available = 512 - *offset;
    }
    if (available >= size) {
      mem_copy_non_overlapping((u8*) block_buffer + *offset, dst, size);
      *offset = *offset + size;
      return 0;
    }
    mem_copy_non_overlapping((u8*) block_buffer + *offset, dst, available);
    if (block_get_next(block_buffer) == 0) {
      return size - available;
    }
    *block = block_get_next(block_buffer);
    *offset = 8;
    dst = dst + available;
    size = size - available;
  }
}

u64 disk_create_file(Disk* self, u8* name, u64 length, u64* result) {
  if (length == 0 || length > 488) { return INVALID_ARGUMENT; }
  u64 i = 0;
  while (i < length) {
    if (load8(name + i) <= 32 || load8(name + i) >= 127) {
      return INVALID_ARGUMENT;
    }
    i = i + 1;
  }
  u64 previous_file = 0;
  DiskBlock* block_buffer = disk_read_block_buffer(self, SUPER_BLOCK);
  u64 new_file = block_get_next(block_buffer);
  if (new_file == 0) { return OUT_OF_MEMORY; }
  u64 new_first_file = to_u64(block_buffer->next_file);
  u64 file = to_u64(block_buffer->next_file);
  u64 file_name = SMALLER;
  while (file != 0) {
    disk_read_block_buffer(self, file);
    file_name = disk_compare_file_name(block_buffer, name, length);
    if (file_name == EQUAL) { return ALREADY_EXISTS; }
    if (file_name == GREATER) { break; }
    previous_file = file;
    file = to_u64(block_buffer->next_file);
  }
  if (previous_file != 0) {
    disk_read_block_buffer(self, previous_file);
    block_buffer->next_file = from_u64(new_file);
    disk_write_block_buffer(self, previous_file);
  } else {
    new_first_file = new_file;
  }
  disk_read_block_buffer(self, new_file);
  u64 new_first_free_block = block_get_next(block_buffer);
  *block_size(block_buffer) = from_u64(24 + length);
  block_buffer->next_file = from_u64(file);
  block_buffer->name_length = from_u64(length);
  mem_copy_non_overlapping(name, block_buffer->name, length);
  disk_write_block_buffer(self, new_file);
  disk_read_block_buffer(self, SUPER_BLOCK);
  block_buffer->next_file = from_u64(new_first_file);
  block_set_next(block_buffer, new_first_free_block);
  disk_write_block_buffer(self, SUPER_BLOCK);
  disk_flush_blocks(self);
  *result = new_file;
  return OK;
}

u64 disk_write_file(Disk* self, u64* block, u8* src, u64 size) {
  DiskBlock* block_buffer = disk_read_block_buffer(self, SUPER_BLOCK);
  u64 old_first_free_block = block_get_next(block_buffer);
  u64 new_first_free_block = old_first_free_block;
  disk_read_block_buffer(self, *block);
  u64 used = to_u64(*block_size(block_buffer));
  u64 free = 512 - used;
  u64 result = OK;
  while (true) {
    if (size <= free) {
      *block_size(block_buffer) = from_u64(used + size);
      mem_copy_non_overlapping(src, (u8*) block_buffer + used, size);
      disk_write_block_buffer(self, *block);
      break;
    }
    if (new_first_free_block == 0) {
      result = OUT_OF_MEMORY;
      break;
    }
    block_set_next(block_buffer, new_first_free_block);
    mem_copy_non_overlapping(src, (u8*) block_buffer + used, free);
    disk_write_block_buffer(self, *block);
    *block = new_first_free_block;
    disk_read_block_buffer(self, *block);
    new_first_free_block = block_get_next(block_buffer);
    src = src + free;
    size = size - free;
    used = 8;
    free = 504;
  }
  if (new_first_free_block != old_first_free_block) {
    disk_read_block_buffer(self, SUPER_BLOCK);
    block_set_next(block_buffer, new_first_free_block);
    disk_write_block_buffer(self, SUPER_BLOCK);
  }
  disk_flush_blocks(self);
  return result;
}

void disk_delete_file_blocks(Disk* self, u64 block) {
  DiskBlock* block_buffer = disk_read_block_buffer(self, SUPER_BLOCK);
  u64 first_free_block = block_get_next(block_buffer);
  u64 last_block = block;
  disk_read_block_buffer(self, last_block);
  while (block_get_next(block_buffer) != 0) {
    last_block = block_get_next(block_buffer);
    disk_read_block_buffer(self, last_block);
  }
  block_set_next(block_buffer, first_free_block);
  disk_write_block_buffer(self, last_block);
}

void disk_clear_file(Disk* self, u64 file) {
  DiskBlock* block_buffer = disk_read_block_buffer(self, file);
  u64 new_first_free_block = block_get_next(block_buffer);
  if (new_first_free_block != 0) {
    disk_delete_file_blocks(self, new_first_free_block);
    disk_read_block_buffer(self, SUPER_BLOCK);
    block_set_next(block_buffer, new_first_free_block);
    disk_write_block_buffer(self, SUPER_BLOCK);
  }
  disk_read_block_buffer(self, file);
  *block_size(block_buffer) = from_u64(24 + to_u64(block_buffer->name_length));
  disk_write_block_buffer(self, file);
  disk_flush_blocks(self);
}

void disk_delete_file(Disk* self, u64 file, u64 previous_file) {
  DiskBlock* block_buffer = disk_read_block_buffer(self, file);
  u64 next_file = to_u64(block_buffer->next_file);
  disk_read_block_buffer(self, SUPER_BLOCK);
  u64 new_first_file = to_u64(block_buffer->next_file);
  disk_delete_file_blocks(self, file);
  if (previous_file != 0) {
    disk_read_block_buffer(self, previous_file);
    block_buffer->next_file = from_u64(next_file);
    disk_write_block_buffer(self, previous_file);
  } else {
    new_first_file = next_file;
  }
  disk_read_block_buffer(self, SUPER_BLOCK);
  block_set_next(block_buffer, file);
  block_buffer->next_file = from_u64(new_first_file);
  disk_write_block_buffer(self, SUPER_BLOCK);
  disk_flush_blocks(self);
}
