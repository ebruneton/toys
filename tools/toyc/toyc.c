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

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base.h"
#include "parser.h"
#include "scanner.h"

#define MAX_SRC_SIZE 65536
#define MAX_CODE_SIZE 20480
#define MAX_HEAP_SIZE 49152

static u8 SRC[MAX_SRC_SIZE];
static u8 DST[MAX_CODE_SIZE];

alignas(max_align_t) u8 HEAP[MAX_HEAP_SIZE];

Compiler compiler;

void panic(const u64 error_code) {
  printf("%.*s\n", (int) (compiler.src - (SRC + 1)), SRC + 1);
  printf("ERROR %lu\n", error_code);
  exit(error_code);
}

int main(const int argc, const char* argv[]) {
  if (argc < 3) {
    printf("Usage: %s <output file> <input files>\n", argv[0]);
    return 1;
  }

  u64 src_size = 1;
  for (int i = 2; i < argc; ++i) {
    FILE* in = fopen(argv[i], "r");
    if (in == NULL) {
      printf("Can't open input file\n");
      return 1;
    }
    const u64 n = fread(SRC + src_size, 1, MAX_SRC_SIZE - src_size, in);
    if (ferror(in)) {
      printf("Can't read input file\n");
      return 1;
    }
    if (!feof(in)) {
      printf("Input file too large\n");
      return 1;
    }
    fclose(in);
    src_size += n;
  }

  compiler.src = SRC;
  compiler.src_end = SRC + src_size;
  compiler.dst = DST;
  compiler.dst_limit = DST + MAX_CODE_SIZE;
  compiler.heap = HEAP;
  compiler.heap_limit = HEAP + MAX_HEAP_SIZE;
  compiler.symbols = NULL;

  tc_read_char(&compiler);
  tc_read_token(&compiler);
  tc_parse_program(&compiler);

  FILE* out = fopen(argv[1], "w");
  if (out == NULL) {
    printf("Can't open output file\n");
    return 1;
  }
  u64 n = fwrite(DST, 1, compiler.dst - DST, out);
  if (DST + n != compiler.dst) {
    printf("Can't write to output file\n");
    return 1;
  }
  fclose(out);
  return 0;
}
