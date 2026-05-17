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

#ifndef _FILE_SYSTEM_H_
#define _FILE_SYSTEM_H_

#include <stdint.h>
#include <stdlib.h>

#define null NULL

typedef uint8_t u8;
typedef uint64_t u64;

inline u64 load8(u8 *ptr) { return (u64)*ptr; }

#define SUPER_BLOCK 0

#define OK 0
#define INVALID_ARGUMENT 1
#define INVALID_STATE 2
#define NOT_FOUND 3
#define ALREADY_EXISTS 4
#define OUT_OF_MEMORY 5
#define INTERNAL_ERROR 6

typedef struct {
  uint8_t bytes[8];
} u64_t;

typedef struct {
  u64_t next_block;
  u64_t next_file;
  u64_t name_length;
  u8 name[488];
} DiskBlock;

typedef struct {
  DiskBlock *block_buffer;
  u8 *image;
} Disk;

u64_t from_u64(u64 x);
u64 to_u64(u64_t x);

u64 block_get_next(DiskBlock* self);
void block_set_next(DiskBlock* self, u64 next_block);
u64_t* block_size(DiskBlock* self);
DiskBlock* disk_read_block_buffer(Disk *self, u64 block_id);
void disk_write_block_buffer(Disk *self, u64 block_id);
u64 disk_find_file(Disk *self, u8 *name, u64 length, u64 *previous_file);
u64 disk_get_file_size(Disk *self, u64 file);
u64 disk_read_file(Disk *self, u64 *block, u64 *offset, u8 *dst, u64 size);
u64 disk_create_file(Disk *self, u8 *name, u64 length, u64 *result);
u64 disk_write_file(Disk *self, u64 *file, u8 *src, u64 size);
void disk_delete_file_blocks(Disk *self, u64 block);
void disk_clear_file(Disk *self, u64 file);

#endif
