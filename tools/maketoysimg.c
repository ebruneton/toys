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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "types.h"

#define SECTOR_SIZE 512

// Type definitions for the GUID Partition Table data structures, with parts
// that we don't need aggregated into array fields named 'unused'.
//
// - https://wiki.osdev.org/GPT
// - https://en.wikipedia.org/wiki/GUID_Partition_Table

typedef struct {
  u8 unused1[446];
  u32 starting_chs;
  u32 ending_chs_and_os_type;
  u32 starting_lba;
  u32 size_in_lba;
  u8 unused2[48];
  u16 signature;
} master_boot_record_t;

typedef struct {
  u8 bytes[16];
} guid_t;

typedef struct {
  char signature[8];
  u32 revision;
  u32 header_size;
  u32 header_crc32;
  u32 reserved1;
  u64 my_lba;
  u64 alternate_lba;
  u64 first_usable_lba;
  u64 last_usable_lba;
  guid_t disk_guid;
  u64 partition_entry_lba;
  u32 number_of_partition_entries;
  u32 size_of_partition_entry;
  u32 partition_entry_array_crc32;
  u8 reserved2[420];
} partition_table_header_t;

typedef struct {
  guid_t partition_type_guid;
  guid_t partition_guid;
  u64 starting_lba;
  u64 ending_lba;
  u64 attributes;
  u8 name[72];
} partition_entry_t;

typedef struct {
  partition_entry_t entry[128];
} partition_entries_t;

static_assert(sizeof(master_boot_record_t) == 512);
static_assert(sizeof(partition_table_header_t) == 512);
static_assert(sizeof(partition_entry_t) == 128);
static_assert(sizeof(partition_entries_t) == 16384);

// https://wiki.osdev.org/EFI_System_Partition#Identify
static guid_t EFI_SYSTEM_GUID = {.bytes = {0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8,
                                           0xD2, 0x11, 0xBA, 0x4B, 0x00, 0xA0,
                                           0xC9, 0x3E, 0xC9, 0x3B}};

guid_t random_guid() {
  guid_t result;
  for (int i = 0; i < 16; ++i) {
    result.bytes[i] = rand();
  }
  return result;
}

uint32_t crc32(const uint8_t *data, const int size) {
  // https://stackoverflow.com/questions/21001659/
  uint32_t crc = 0xFFFFFFFF;
  for (int i = 0; i < size; ++i) {
    uint8_t byte = data[i];
    crc = crc ^ byte;
    for (int j = 0; j < 8; ++j) {
      uint32_t mask = -(crc & 1);
      crc = (crc >> 1) ^ (0xEDB88320 & mask);
    }
  }
  return ~crc;
}

int error(const char *msg, const char *arg) {
  printf("%s%s\n", msg, arg);
  return 1;
}

int main(const int argc, const char *argv[]) {
  if (argc != 3) {
    printf("Usage: %s <FAT12 partition image> <output disk image>\n", argv[0]);
    printf("\n");
    printf("Creates the image of a disk formatted with a GUID partition table, "
           "containing the given FAT12 partition as unique partition.\n");
    return 1;
  }

  const char *input_name = argv[1];
  FILE *input = fopen(input_name, "rb");
  if (input == NULL) {
    return error("Can't open file ", input_name);
  }
  fseek(input, 0, SEEK_END);
  const size_t input_size = ftell(input);
  if (input_size % SECTOR_SIZE != 0) {
    return error("Invalid file size ", input_name);
  }
  fseek(input, 0, SEEK_SET);

  const int num_table_entry_sectors = 32;
  const int num_partition_sectors = input_size / SECTOR_SIZE;
  int num_image_sectors =
      3 + 2 * num_table_entry_sectors + num_partition_sectors;
  if (num_image_sectors < 2048) {
    num_image_sectors = 2048;
  }

  const int header_size = 96;
  const int num_partitions = 1;
  const int first_usable_lba = 2 + num_table_entry_sectors;
  const int last_usable_lba = first_usable_lba + num_partition_sectors - 1;

  const master_boot_record_t mbr = {
      .starting_chs = to_u32(0x20000),
      .ending_chs_and_os_type = to_u32(0xFFFFFFEE),
      .starting_lba = to_u32(1),
      .size_in_lba = to_u32(num_image_sectors - 1),
      .signature = to_u16(0xAA55),
  };
  partition_table_header_t header = {
      .signature = "EFI PART",
      .revision = to_u32(0x00010000),
      .header_size = to_u32(header_size),
      .my_lba = to_u64(1),
      .alternate_lba = to_u64(num_image_sectors - 1),
      .first_usable_lba = to_u64(first_usable_lba),
      .last_usable_lba = to_u64(last_usable_lba),
      .disk_guid = random_guid(),
      .partition_entry_lba = to_u64(2),
      .number_of_partition_entries = to_u32(num_partitions),
      .size_of_partition_entry = to_u32(sizeof(partition_entry_t)),
  };
  partition_entries_t partition_entries = {
      .entry[0] =
          {
              .partition_type_guid = EFI_SYSTEM_GUID,
              .partition_guid = random_guid(),
              .starting_lba = to_u64(first_usable_lba),
              .ending_lba = to_u64(last_usable_lba),
              .name = {'u', 0, 'e', 0, 'f', 0, 'i', 0},
          },
  };
  partition_table_header_t alternate_header = header;
  alternate_header.my_lba = header.alternate_lba;
  alternate_header.alternate_lba = header.my_lba;
  alternate_header.partition_entry_lba = to_u64(last_usable_lba + 1);

  header.partition_entry_array_crc32 = to_u32(crc32(
      (u8 *)&partition_entries, num_partitions * sizeof(partition_entry_t)));
  header.header_crc32 = to_u32(crc32((u8 *)&header, header_size));

  alternate_header.partition_entry_array_crc32 =
      header.partition_entry_array_crc32;
  alternate_header.header_crc32 =
      to_u32(crc32((u8 *)&alternate_header, header_size));

  const char *output_name = argv[2];
  FILE *out = fopen(output_name, "wb");
  if (!out) {
    return error("Can't open ", output_name);
  }
  if (fwrite(&mbr, sizeof(mbr), 1, out) != 1 ||
      fwrite(&header, sizeof(header), 1, out) != 1 ||
      fwrite(&partition_entries, sizeof(partition_entries), 1, out) != 1) {
    return error("Can't write ", output_name);
  }
  unsigned char buffer[SECTOR_SIZE];
  const int num_sectors = input_size / SECTOR_SIZE;
  for (int j = 0; j < num_sectors; ++j) {
    if (fread(buffer, SECTOR_SIZE, 1, input) != 1) {
      return error("Cant't read ", input_name);
    }
    if (fwrite(buffer, SECTOR_SIZE, 1, out) != 1) {
      return error("Can't write ", output_name);
    }
  }
  if (fwrite(&partition_entries, sizeof(partition_entries), 1, out) != 1 ||
      fwrite(&alternate_header, sizeof(alternate_header), 1, out) != 1) {
    return error("Can't write ", output_name);
  }
  fclose(out);
  return 0;
}
