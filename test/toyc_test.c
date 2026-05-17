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
#include <sys/mman.h>

#define MAX_COMPILED_SIZE 24576
#define MAX_SOURCE_SIZE 65536
#define HEAP_SIZE 131072

int error(const char *msg, const char *arg) {
  printf("%s%s\n", msg, arg);
  return 1;
}

int read_file(const char *name, uint8_t *dst, size_t max_size, size_t *size) {
  FILE *in = fopen(name, "rb");
  if (in == NULL) {
    return error("Can't open file ", name);
  }
  *size = fread(dst, 1, max_size, in);
  if (ferror(in)) {
    return error("Can't read ", name);
  }
  if (!feof(in)) {
    return error("File too large ", name);
  }
  fclose(in);
  return 0;
}

int main(const int argc, const char *argv[]) {
  if (argc < 3) {
    printf("Usage: %s <toyc binary> <toyc sources>\n", argv[0]);
    printf("\n");
    printf(
        "Compiles the given Toy source files with the given Toy compiler, and "
        "checks that the result binary is identical to the given one.\n");
    return 1;
  }

  uint8_t *toyc_binary =
      mmap(NULL, MAX_COMPILED_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC,
           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (toyc_binary == MAP_FAILED) {
    return error("Failed to allocate memory", "");
  }
  uint64_t (*toyc)(void *, void *, void *, void *);
  *((void **)&toyc) = toyc_binary;

  static uint8_t toyc_source[MAX_SOURCE_SIZE];
  size_t binary_size = 0;
  size_t source_size = 0;
  if (read_file(argv[1], toyc_binary, MAX_COMPILED_SIZE, &binary_size) != 0) {
    return 1;
  }
  for (int i = 2; i < argc; ++i) {
    size_t file_size = 0;
    if (read_file(argv[i], toyc_source + source_size,
                  MAX_SOURCE_SIZE - source_size, &file_size) != 0) {
      return 1;
    }
    source_size += file_size;
  }

  static uint8_t heap[HEAP_SIZE];
  uint64_t result =
      toyc(toyc_source, toyc_source + source_size, heap, heap + HEAP_SIZE);

  int status = result >> 56;
  if (status != 0) {
    printf("%.*s\n", (uint32_t)result, toyc_source);
    return status;
  }
  if (result != binary_size || memcmp(toyc_binary, heap, binary_size)) {
    FILE *out = fopen("toyc_self_compiled", "wb");
    if (out) {
      if (fwrite(heap, result, 1, out) == 1) {
        printf("Self compilation result written to 'toyc_self_compiled'\'n");
      }
      fclose(out);
    }
    return error("Self compilation test failed!", "");
  }
  return 0;
}
