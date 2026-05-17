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
#include <string.h>

#include "types.h"

// Type definitions for the PE32 file format, with parts that we don't need
// aggregated into array fields named 'unused'.
//
// See https://learn.microsoft.com/en-us/windows/win32/debug/pe-format.

typedef struct {
  u16 magic;
  u16 unused1[29];
  u32 lfanew;
  u8 unused2[128];
} ms_dos_stub_t;

typedef struct {
  u16 machine;
  u16 number_of_sections;
  u32 unused[3];
  u16 size_of_optional_header;
  u16 characteristics;
} coff_file_header_t;

typedef struct {
  u16 magic;
  u16 linker_version;
  u32 size_of_code;
  u32 size_of_initialized_data;
  u32 size_of_uninitialized_data;
  u32 address_of_entry_point;
  u32 base_of_code;
  u64 image_base;
  u32 section_alignment;
  u32 file_alignment;
  u16 unused1[8];
  u32 size_of_image;
  u32 size_of_headers;
  u32 checksum;
  u16 subsystem;
  u16 unused2[19];
  u32 number_of_rva_and_sizes;
  u64 data_directory[16];
} optional_header_t;

typedef struct {
  ms_dos_stub_t dos_stub;
  u32 signature;
  coff_file_header_t coff_file_header;
  optional_header_t optional_header;
} pe_file_header_t;

typedef struct {
  u8 name[8];
  union {
    u32 physical_address;
    u32 virtual_size;
  } misc;
  u32 virtual_address;
  u32 size_of_raw_data;
  u32 pointer_to_raw_data;
  u32 unused[3];
  u32 characteristics;
} image_section_header_t;

// Size of the headers, before the raw machine code.
#define HEADER_SIZE 512

// Total size of the output file.
#define TOTAL_SIZE 2048

static_assert(sizeof(pe_file_header_t) + sizeof(image_section_header_t) <=
              HEADER_SIZE);

uint32_t align(const uint32_t x, const uint32_t base) {
  return (x + base - 1) & ~(base - 1);
}

int error(const char *msg, const char *arg) {
  printf("%s%s\n", msg, arg);
  return 1;
}

int main(const int argc, const char *argv[]) {
  if (argc != 3) {
    printf("Usage: %s <raw machine code> <output.efi>\n", argv[0]);
    printf("\n");
    printf("Encapsulates the given raw machine code into a PE32+ executable "
           "with a single .text section which can be launched by an UEFI "
           "firmware. The given raw machine code must be position-independent, "
           "and must provide an UEFI Image main entry point at offset 0 "
           "(https://uefi.org/specs/UEFI/2.9_A/04_EFI_System_Table.html"
           "#efi-image-entry-point). It must also be self-sufficient (no "
           "dependency on other sections such as .rdata, .rodata, .bss, "
           "etc).\n");
    return 1;
  }

  const char *input_name = argv[1];
  FILE *in = fopen(input_name, "rb");
  if (in == NULL) {
    return error("Can't open file ", input_name);
  }
  static uint8_t input_buffer[TOTAL_SIZE - HEADER_SIZE + 1];
  memset(input_buffer, 0, sizeof(input_buffer));
  size_t code_size = fread(input_buffer, 1, sizeof(input_buffer), in);
  if (ferror(in)) {
    return error("Can't read ", input_name);
  }
  if (!feof(in) || code_size > TOTAL_SIZE - HEADER_SIZE) {
    return error("File too large ", input_name);
  }
  fclose(in);
  code_size = TOTAL_SIZE - HEADER_SIZE;

  const pe_file_header_t file_header = {
      .dos_stub =
          {
              .magic = {{'M', 'Z'}},
              .lfanew = to_u32(offsetof(pe_file_header_t, signature)),
          },
      .signature = {{'P', 'E'}},
      .coff_file_header =
          {
              .machine = to_u16(0x8664) /* IMAGE_FILE_MACHINE_AMD64 */,
              .number_of_sections = to_u16(1), /* The .text section */
              .size_of_optional_header = to_u16(sizeof(optional_header_t)),
              .characteristics = to_u16(0x2002) /*executable, dll*/,
          },
      .optional_header =
          {
              .magic = to_u16(0x20b), /* PE32+ */
              .size_of_code = to_u32(code_size),
              .address_of_entry_point = to_u32(4096),
              .base_of_code = to_u32(4096),
              .section_alignment = to_u32(4096),
              .file_alignment = to_u32(32),
              .size_of_headers = to_u32(HEADER_SIZE),
              .size_of_image = to_u32(align(4096 + code_size, 4096)),
              .subsystem = to_u16(10), /* EFI application */
              .number_of_rva_and_sizes = to_u32(16),
          },
  };

  const uint32_t pointer_to_raw_data = HEADER_SIZE;
  const uint32_t size_of_raw_data = align(code_size, 32);

  const image_section_header_t text_section_header = {
      .name = ".text",
      .misc = {.virtual_size = to_u32(code_size)},
      .virtual_address = to_u32(4096),
      .size_of_raw_data = to_u32(size_of_raw_data),
      .pointer_to_raw_data = to_u32(pointer_to_raw_data),
      // code, not paged, read, write, execute
      // learn.microsoft.com/en-us/windows/win32/debug/pe-format#section-flags
      .characteristics = to_u32(0xE8000020),
  };

  const char *output_name = argv[2];
  FILE *out = fopen(output_name, "wb");
  if (!out) {
    return error("Can't open ", output_name);
  }
  if (fwrite(&file_header, sizeof(file_header), 1, out) != 1) {
    return error("Can't write PE header ", output_name);
  }
  if (fwrite(&text_section_header, sizeof(text_section_header), 1, out) != 1) {
    return error("Can't write PE image section header ", output_name);
  }
  if (fseek(out, pointer_to_raw_data, SEEK_SET) != 0) {
    return error("Can't write raw code data ", output_name);
  }
  if (fwrite(input_buffer, size_of_raw_data, 1, out) != 1) {
    return error("Can't write raw code data ", output_name);
  }
  fclose(out);
  return 0;
}
