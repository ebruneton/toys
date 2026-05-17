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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "toys/filesystem.h"

#define BLOCK_SIZE 512
#define IMAGE_SIZE (1024 * 1024)

// Initializes the linked list of free blocks, containing all the blocks.
void disk_format(Disk *self, int num_blocks) {
  DiskBlock *block_buffer = disk_read_block_buffer(self, SUPER_BLOCK);
  block_set_next(block_buffer, SUPER_BLOCK + 1);
  block_buffer->next_file = from_u64(0);
  disk_write_block_buffer(self, SUPER_BLOCK);
  for (int i = SUPER_BLOCK + 1; i < num_blocks; ++i) {
    disk_read_block_buffer(self, i);
    block_set_next(block_buffer, (i + 1) % num_blocks);
    disk_write_block_buffer(self, i);
  }
}

int error(const char *msg, const char *arg) {
  printf("%s%s\n", msg, arg);
  return 1;
}

int main(const int argc, const char *argv[]) {
  if (argc < 3) {
    printf("Usage: %s <input files> <output Toys FS image>\n", argv[0]);
    printf("\n");
    printf("Creates an image of a Toys file system containing the given "
           "input files.\n");
    return 1;
  }

  static DiskBlock block_buffer;
  static u8 image[IMAGE_SIZE];

  Disk disk;
  disk.block_buffer = &block_buffer;
  disk.image = image;
  disk_format(&disk, IMAGE_SIZE / BLOCK_SIZE);

  for (int i = 1; i < argc - 1; ++i) {
    const char *input_name = argv[i];
    const char *name = input_name;
    u64 name_length = strlen(name);
    if (name_length >= 4 && strncmp(name, "bin/", 4) == 0) {
      if (name_length != 8 || strncmp(name, "bin/toys", 8) != 0) {
        name += 4;
        name_length -= 4;
      }
    } else if (name_length == 7 && strncmp(name, "LICENSE", 7) == 0) {
      name = "src/LICENSE";
      name_length = strlen(name);
    }
    u64 file_block_id;
    if (disk_create_file(&disk, (u8 *)name, name_length, &file_block_id) !=
        OK) {
      return error("Can't create file in image: ", input_name);
    }
    FILE *in = fopen(input_name, "rb");
    if (in == NULL) {
      return error("Can't open ", input_name);
    }
    u8 buffer[BLOCK_SIZE];
    while (true) {
      const size_t n = fread(buffer, 1, BLOCK_SIZE, in);
      if (ferror(in)) {
        return error("Can't read ", input_name);
      }
      if (disk_write_file(&disk, &file_block_id, buffer, n) != OK) {
        return error("Can't write file in image: ", input_name);
      }
      if (feof(in)) {
        break;
      }
    }
    fclose(in);
  }

  const char *output_name = argv[argc - 1];
  FILE *out = fopen(output_name, "wb");
  if (out == NULL) {
    return error("Can't open file", output_name);
  }
  const size_t n = fwrite(disk.image, 1, IMAGE_SIZE, out);
  if (n != IMAGE_SIZE) {
    return error("Can't write ", output_name);
  }
  fclose(out);
  return 0;
}
