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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "toys/filesystem.h"

#define BLOCK_SIZE 512
#define TAR_BLOCK_SIZE 512

// A TAR header, put before the content of each file in a TAR archive.
//
// See https://en.wikipedia.org/wiki/Tar_(computing).

typedef struct {
  char name[100];
  char mode[8];
  char owner[8];
  char group[8];
  char size[12];
  char mtime[12];
  char checksum[8];
  char type;
  char linkname[100];
  char padding[255];
} tar_header_t;

static_assert(sizeof(tar_header_t) == TAR_BLOCK_SIZE);

tar_header_t compute_tar_header(const char *name, uint32_t size) {
  tar_header_t header = {
      .mode = "0000664\0",
      .owner = "0001750\0",
      .group = "0001750\0",
      .mtime = "00000000000\0",
      .checksum = "        ",
      .type = '0',
  };
  strcpy(header.name, name);
  sprintf(header.size, "%o", size);
  uint32_t checksum = 0;
  for (size_t i = 0; i < sizeof(tar_header_t); ++i) {
    checksum += ((char *)&header)[i];
  }
  sprintf(header.checksum, "%06o", checksum);
  return header;
}

int error(const char *msg, const char *arg) {
  printf("%s%s\n", msg, arg);
  return 1;
}

int main(const int argc, const char *argv[]) {
  if (argc != 3) {
    printf("Usage: %s <Toys FS image> <output tar or csv file>\n", argv[0]);
    printf("\n");
    printf("Reads the files in a Toys file system image and either writes them "
           "in a TAR archive, or stores their name and size in a CSV file.\n");
    return 1;
  }

  const char *input_name = argv[1];
  FILE *input = fopen(input_name, "rb");
  if (input == NULL) {
    return error("Can't open file ", input_name);
  }
  fseek(input, 0, SEEK_END);
  const size_t input_size = ftell(input);
  fseek(input, 0, SEEK_SET);

  DiskBlock block_buffer;

  Disk disk;
  disk.block_buffer = &block_buffer;
  disk.image = malloc(input_size);
  if (disk.image == NULL) {
    return error("Out of memory", "");
  }
  if (fread(disk.image, 1, input_size, input) != input_size) {
    return error("Can't read file ", input_name);
  }

  const char *output_name = argv[2];
  FILE *out = fopen(output_name, "wb");
  if (!out) {
    return error("Can't open ", output_name);
  }
  bool tar_mode = strlen(output_name) > 4 &&
                  strcmp(output_name + strlen(output_name) - 4, ".tar") == 0;
  if (!tar_mode) {
    fprintf(out, "name,bytes,lines\n");
  }
  u64 total_size = 0;
  u64 total_line_count = 0;

  disk_read_block_buffer(&disk, SUPER_BLOCK);
  u64 file = to_u64(disk.block_buffer->next_file);
  while (file != 0) {
    u64 block = file;
    disk_read_block_buffer(&disk, file);
    file = to_u64(disk.block_buffer->next_file);

    char name[512];
    uint32_t name_length = to_u64(disk.block_buffer->name_length);
    sprintf(name, "%.*s", name_length, disk.block_buffer->name);
    u64 size = disk_get_file_size(&disk, block);

    if (tar_mode) {
      if (strlen(name) > 99) {
        printf("Filename too long for TAR archive: '%s', SKIPPED", name);
        continue;
      }
      tar_header_t tar_header = compute_tar_header(name, size);
      if (fwrite(&tar_header, sizeof(tar_header_t), 1, out) != 1) {
        return error("Can't write ", output_name);
      }
    }

    bool is_source_file =
        strlen(name) > 4 && strcmp(name + name_length - 4, ".toy") == 0;
    const int NEW_LINE = 10;
    u8 last_char = NEW_LINE;
    u64 line_count = 0;
    u64 offset = 24 + name_length;
    u64 remaining = size;
    while (remaining > 0) {
      u8 buffer[TAR_BLOCK_SIZE];
      u64 n = TAR_BLOCK_SIZE -
              disk_read_file(&disk, &block, &offset, buffer, TAR_BLOCK_SIZE);
      memset(buffer + n, 0, TAR_BLOCK_SIZE - n);
      if (tar_mode) {
        if (fwrite(buffer, TAR_BLOCK_SIZE, 1, out) != 1) {
          return error("Can't write ", output_name);
        }
      } else if (is_source_file) {
        for (u64 i = 0; i < n; ++i) {
          if (last_char == NEW_LINE) {
            line_count += 1;
          }
          last_char = buffer[i];
        }
      }
      remaining -= n;
    }
    if (!tar_mode) {
      fprintf(out, "%s,%lu,%lu\n", name, size, line_count);
    }

    total_size += size;
    total_line_count += line_count;
  }

  if (tar_mode) {
    for (int i = 0; i < 2 * TAR_BLOCK_SIZE; ++i) {
      uint8_t zero = 0;
      if (fwrite(&zero, 1, 1, out) != 1) {
        return error("Can't write ", output_name);
      }
    }
  } else {
    printf("TOTAL: %lu bytes, %lu lines\n", total_size, total_line_count);
  }
  fclose(out);
  return 0;
}
