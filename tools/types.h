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

#ifndef _TYPES_H_
#define _TYPES_H_

#include <stdint.h>

// Utility integer types to avoid packing and alignment issues when defining
// structs corresponding to standard file formats, and endianness issues when
// writing them to disk.

typedef uint8_t u8;

typedef struct {
  uint8_t bytes[2];
} u16;

typedef struct {
  uint8_t bytes[4];
} u32;

typedef struct {
  uint8_t bytes[8];
} u64;

u16 to_u16(uint16_t x) {
  u16 result = {
      .bytes = {(uint8_t)x, (uint8_t)(x >> 8)},
  };
  return result;
}

u32 to_u32(uint32_t x) {
  u32 result = {
      .bytes = {(uint8_t)x, (uint8_t)(x >> 8), (uint8_t)(x >> 16),
                (uint8_t)(x >> 24)},
  };
  return result;
}

u64 to_u64(uint64_t x) {
  u64 result = {
      .bytes = {(uint8_t)x, (uint8_t)(x >> 8), (uint8_t)(x >> 16),
                (uint8_t)(x >> 24), (uint8_t)(x >> 32), (uint8_t)(x >> 40),
                (uint8_t)(x >> 48), (uint8_t)(x >> 56)},
  };
  return result;
}

#endif
