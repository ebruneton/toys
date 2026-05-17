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
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "types.h"

#define SECTOR_SIZE 512
#define CLUSTER_SIZE 2048
#define NUM_BOOT_LOADER_CLUSTERS 1

// Type definitions for the FAT12 file system data structures, with parts that
// we don't need aggregated into array fields named 'unused'.
//
// - https://wiki.osdev.org/FAT
// - https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system

typedef struct {
  u8 bootjmp[3];
  u8 oem_name[8];
  u16 bytes_per_sector;
  u8 sectors_per_cluster;
  u16 reserved_sector_count;
  u8 table_count;
  u16 root_entry_count;
  u16 total_sectors_16;
  u8 media_type;
  u16 table_size_16;
  u16 sectors_per_track;
  u16 head_side_count;
  u32 hidden_sector_count;
  u32 total_sectors_32;
  u8 bios_drive_num;
  u8 reserved1;
  u8 boot_signature;
  u32 volume_id;
  u8 volume_label[11];
  u8 fat_type_label[8];
  u8 boot_code[448];
  u16 signature;
} boot_sector_t;

typedef struct {
  u8 bytes[1024];
} file_allocation_table_t;

typedef struct {
  u8 name[11];
  u8 attributes;
  u8 unused[14];
  u16 first_cluster;
  u32 file_size;
} directory_entry_t;

typedef struct {
  directory_entry_t entries[SECTOR_SIZE / sizeof(directory_entry_t)];
} root_directory_t;

typedef struct {
  directory_entry_t entries[CLUSTER_SIZE / sizeof(directory_entry_t)];
} directory_t;

// The specific FAT12 partition content written by this tool, before the actual
// content of the TOYSFS file: an EFI directory, a BOOT subdirectory, and the
// BOOTX64.EFI file sectors. The total size of this "header" is 16 sectors.
typedef struct {
  boot_sector_t boot_sector;
  file_allocation_table_t fat;
  root_directory_t root_directory;
  directory_t efi_directory;
  directory_t efi_boot_directory;
  u8 boot_loader_data[NUM_BOOT_LOADER_CLUSTERS * CLUSTER_SIZE];
} partition_t;

static_assert(sizeof(boot_sector_t) == SECTOR_SIZE);
static_assert(sizeof(file_allocation_table_t) % SECTOR_SIZE == 0);
static_assert(sizeof(root_directory_t) == SECTOR_SIZE);
static_assert(sizeof(directory_t) == CLUSTER_SIZE);
static_assert(sizeof(partition_t) == 16 * SECTOR_SIZE);

void set_nibble(u8 *bytes, const int nibble_index, const int value) {
  bytes[nibble_index / 2] |= nibble_index % 2 == 0 ? value : (value << 4);
}

void set_fat_entry(u8 *fat, const int entry_index, const int value) {
  set_nibble(fat, 3 * entry_index, value & 0xF);
  set_nibble(fat, 3 * entry_index + 1, (value >> 4) & 0xF);
  set_nibble(fat, 3 * entry_index + 2, (value >> 8) & 0xF);
}

int error(const char *msg, const char *arg) {
  printf("%s%s\n", msg, arg);
  return 1;
}

int main(const int argc, const char *argv[]) {
  if (argc != 4) {
    printf("Usage: %s <boot.efi> <toysfs> <output FAT12 image>\n", argv[0]);
    printf("\n");
    printf("Creates an image of a FAT12 UEFI System partition containing the "
           "two given files. <boot.efi> is stored as EFI/BOOT/BOOTX64.EFI. "
           "<toyfs> is stored as TOYSFS, in contiguous sectors starting from "
           "sector 16.\n");
    return 1;
  }

  const char *input_name[2];
  size_t input_size[2];
  FILE *input[2];
  for (int i = 0; i < 2; ++i) {
    input_name[i] = argv[i + 1];
    input[i] = fopen(input_name[i], "rb");
    if (input[i] == NULL) {
      return error("Can't open file ", input_name[i]);
    }
    fseek(input[i], 0, SEEK_END);
    input_size[i] = ftell(input[i]);
    fseek(input[i], 0, SEEK_SET);
  }
  if (input_size[0] > NUM_BOOT_LOADER_CLUSTERS * CLUSTER_SIZE) {
    return error("Boot loader too large ", input_name[0]);
  }
  if (input_size[1] % SECTOR_SIZE != 0) {
    return error("Invalid Toys file system size", input_name[1]);
  }
  const int num_input_clusters =
      (input_size[1] + CLUSTER_SIZE - 1) / CLUSTER_SIZE;
  const int max_clusters = sizeof(file_allocation_table_t) * 3 / 2;
  if (4 + NUM_BOOT_LOADER_CLUSTERS + num_input_clusters > max_clusters) {
    return error("Toys file system too large ", input_name[0]);
  }

  // https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system#BPB20_OFS_0Ah
  const uint8_t FIXED_DISK_MEDIA_TYPE = 0xF8;

  // Attributes of directory entries
  const uint8_t DIRECTORY_ENTRY = 0x10;
  const uint8_t FILE_ENTRY = 0x20;

  const int partition_size = sizeof(partition_t) + input_size[1];
  partition_t partition = {
      .boot_sector =
          {
              .bootjmp = {0xEB, 0x3C, 0x90},
              .oem_name = "MSWIN4.1",
              .bytes_per_sector = to_u16(SECTOR_SIZE),
              .sectors_per_cluster = CLUSTER_SIZE / SECTOR_SIZE,
              .reserved_sector_count = to_u16(1),
              .table_count = 1,
              .root_entry_count =
                  to_u16(SECTOR_SIZE / sizeof(directory_entry_t)),
              .total_sectors_16 = to_u16(partition_size / SECTOR_SIZE),
              .media_type = FIXED_DISK_MEDIA_TYPE,
              .table_size_16 =
                  to_u16(sizeof(file_allocation_table_t) / SECTOR_SIZE),
              .sectors_per_track = to_u16(63),
              .head_side_count = to_u16(16),
              .hidden_sector_count = to_u32(0),
              .bios_drive_num = 128,
              .boot_signature = 0x29,
              .volume_label = "EFI SYSTEM ",
              .fat_type_label = "FAT12   ",
              .signature = to_u16(0xAA55),
          },
      .fat =
          {
              // 2 reserved entries, 2 directory entries with no next entry.
              // The other entries depend on the input files and are set below.
              .bytes = {FIXED_DISK_MEDIA_TYPE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
          },
      .root_directory =
          {
              .entries[0] =
                  {
                      .name = "EFI        ",
                      .attributes = DIRECTORY_ENTRY,
                      .first_cluster = to_u16(2),
                  },
              .entries[1] =
                  {
                      .name = "TOYSFS     ",
                      .attributes = FILE_ENTRY,
                      .first_cluster = to_u16(4 + NUM_BOOT_LOADER_CLUSTERS),
                      .file_size = to_u32(input_size[1]),
                  },
          },
      .efi_directory =
          {
              .entries[0] =
                  {
                      .name = ".          ",
                      .attributes = DIRECTORY_ENTRY,
                      .first_cluster = to_u16(2),
                  },
              .entries[1] =
                  {
                      .name = "..         ",
                      .attributes = DIRECTORY_ENTRY,
                      .first_cluster = to_u16(0),
                  },
              .entries[2] =
                  {
                      .name = "BOOT       ",
                      .attributes = DIRECTORY_ENTRY,
                      .first_cluster = to_u16(3),
                  },
          },
      .efi_boot_directory =
          {
              .entries[0] =
                  {
                      .name = ".          ",
                      .attributes = DIRECTORY_ENTRY,
                      .first_cluster = to_u16(3),
                  },
              .entries[1] =
                  {
                      .name = "..         ",
                      .attributes = DIRECTORY_ENTRY,
                      .first_cluster = to_u16(2),
                  },
              .entries[2] =
                  {
                      .name = "BOOTX64 EFI",
                      .attributes = FILE_ENTRY,
                      .first_cluster = to_u16(4),
                      .file_size = to_u32(input_size[0]),
                  },
          },
  };

  for (int i = 0; i < 2; ++i) {
    int fat_entry = 4 + NUM_BOOT_LOADER_CLUSTERS * i;
    int size = input_size[i];
    while (size > CLUSTER_SIZE) {
      set_fat_entry(partition.fat.bytes, fat_entry, fat_entry + 1);
      fat_entry += 1;
      size -= CLUSTER_SIZE;
    }
    if (size > 0) {
      set_fat_entry(partition.fat.bytes, fat_entry, -1);
    }
  }

  if (fread(partition.boot_loader_data, input_size[0], 1, input[0]) != 1) {
    return error("Cant't read ", input_name[0]);
  }

  const char *output_name = argv[3];
  FILE *out = fopen(output_name, "wb");
  if (!out) {
    return error("Can't open ", output_name);
  }
  if (fwrite(&partition, sizeof(partition_t), 1, out) != 1) {
    return error("Can't write ", output_name);
  }
  u8 buffer[SECTOR_SIZE];
  for (size_t i = 0; i < input_size[1] / SECTOR_SIZE; ++i) {
    if (fread(buffer, SECTOR_SIZE, 1, input[1]) != 1) {
      return error("Cant't read ", input_name[1]);
    }
    if (fwrite(buffer, SECTOR_SIZE, 1, out) != 1) {
      return error("Can't write ", output_name);
    }
  }
  fclose(out);
  return 0;
}
