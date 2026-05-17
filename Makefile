# Copyright (c) 2026 Eric Bruneton
# All rights reserved.
#
# This file is part of Toys (https://github.com/ebruneton/toys).
#
# Toys is free software: you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# Toys is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# Toys. If not, see <https://www.gnu.org/licenses/>

CC = gcc
TOYCC = bin/toycc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2

bin/backend.o: tools/toyc/base.h tools/toyc/backend.c
	$(CC) $(CFLAGS) -c -o bin/backend.o tools/toyc/backend.c

bin/parser.o: tools/toyc/base.h tools/toyc/parser.c
	$(CC) $(CFLAGS) -c -o bin/parser.o tools/toyc/parser.c

bin/scanner.o: tools/toyc/base.h tools/toyc/scanner.c
	$(CC) $(CFLAGS) -c -o bin/scanner.o tools/toyc/scanner.c

TOYCC_OBJS = bin/backend.o bin/parser.o bin/scanner.o

bin/toycc: tools/toyc/base.h tools/toyc/toyc.c $(TOYCC_OBJS)
	$(CC) $(CFLAGS) -o $(TOYCC) tools/toyc/toyc.c $(TOYCC_OBJS)

