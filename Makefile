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

VERSION = 1.0

default: run

image: bin/toys-$(VERSION)-amd64.img

tests: toyc_test toys_test

clean:
	rm -fv bin/*

################################################################################
# Toys operating system partition
################################################################################

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

BOOT_SRCS = src/boot/uefi/api.toy src/boot/boot.toy src/boot/uefi/lib.toy

bin/boot: $(TOYCC) $(BOOT_SRCS)
	$(TOYCC) bin/boot $(BOOT_SRCS)

bin/copy: $(TOYCC) src/base.toy src/copy/copy.toy
	$(TOYCC) bin/copy src/base.toy src/copy/copy.toy

bin/delete: $(TOYCC) src/base.toy src/delete/delete.toy
	$(TOYCC) bin/delete src/base.toy src/delete/delete.toy

bin/edit: $(TOYCC) src/base.toy src/gpu.toy src/edit/edit.toy
	$(TOYCC) bin/edit src/base.toy src/gpu.toy src/edit/edit.toy

bin/list: $(TOYCC) src/base.toy src/memory.toy src/list/list.toy
	$(TOYCC) bin/list src/base.toy src/memory.toy src/list/list.toy

bin/reboot: $(TOYCC) src/base.toy src/reboot/reboot.toy
	$(TOYCC) bin/reboot src/base.toy src/reboot/reboot.toy

SHELL_SRCS =\
  src/base.toy\
  src/gpu.toy\
  src/memory.toy\
  src/shell/shell.toy

bin/shell: $(TOYCC) $(SHELL_SRCS)
	$(TOYCC) bin/shell $(SHELL_SRCS)

SNAKE_SRCS =\
  src/base.toy\
  src/gpu.toy\
  src/memory.toy\
  src/snake/snake.toy

bin/snake: $(TOYCC) $(SNAKE_SRCS)
	$(TOYCC) bin/snake $(SNAKE_SRCS)

bin/stat: $(TOYCC) src/base.toy src/stat/stat.toy
	$(TOYCC) bin/stat src/base.toy src/stat/stat.toy

TOYC_SRCS = \
  src/base.toy\
  src/toyc/base.toy\
  src/toyc/scanner.toy\
  src/toyc/backend.toy\
  src/toyc/parser.toy\
  src/toyc/toyc.toy

bin/toyc: $(TOYCC) $(TOYC_SRCS)
	$(TOYCC) bin/toyc $(TOYC_SRCS)

TOYS_SRCS = \
  src/boot/uefi/api.toy\
  src/toys/drivers.toy\
  src/toys/filesystem.toy\
  src/toys/processes.toy\
  src/toys/systemcalls.toy\
  src/toys/toys.toy\
  src/boot/uefi/lib.toy

bin/toys: $(TOYCC) $(TOYS_SRCS)
	$(TOYCC) bin/toys $(TOYS_SRCS)

bin/filesystem.o: tools/toys/filesystem.h tools/toys/filesystem.c
	$(CC) $(CFLAGS) -c -o bin/filesystem.o tools/toys/filesystem.c

bin/maketoysfs: bin/filesystem.o tools/maketoysfs.c
	$(CC) $(CFLAGS) -o bin/maketoysfs bin/filesystem.o tools/maketoysfs.c

bin/dumptoysfs: bin/filesystem.o tools/dumptoysfs.c
	$(CC) $(CFLAGS) -o bin/dumptoysfs bin/filesystem.o tools/dumptoysfs.c

PARTITION_SRCS = \
  LICENSE\
  src/boot/BUILD\
  src/boot/boot.toy\
  src/boot/uefi/api.toy\
  src/boot/uefi/lib.toy\
  src/copy/BUILD\
  src/copy/copy.toy\
  src/delete/BUILD\
  src/delete/delete.toy\
  src/edit/BUILD\
  src/edit/edit.toy\
  src/list/BUILD\
  src/list/list.toy\
  src/reboot/BUILD\
  src/reboot/reboot.toy\
  src/shell/BUILD\
  src/shell/banner.txt\
  src/shell/shell.toy\
  src/snake/BUILD\
  src/snake/snake.toy\
  src/stat/BUILD\
  src/stat/stat.toy\
  src/toyc/BUILD\
  src/toyc/backend.toy\
  src/toyc/base.toy\
  src/toyc/parser.toy\
  src/toyc/scanner.toy\
  src/toyc/toyc.toy\
  src/toys/BUILD\
  src/toys/drivers.toy\
  src/toys/filesystem.toy\
  src/toys/processes.toy\
  src/toys/systemcalls.toy\
  src/toys/toys.toy\
  src/base.toy\
  src/gpu.toy\
  src/memory.toy\
  bin/copy\
  bin/delete\
  bin/edit\
  bin/list\
  bin/reboot\
  bin/shell\
  bin/snake\
  bin/stat\
  bin/toyc\
  bin/toys

bin/toysfs: bin/maketoysfs $(PARTITION_SRCS)
	bin/maketoysfs $(PARTITION_SRCS) bin/toysfs

################################################################################
# UEFI System partition
################################################################################

bin/makebootefi: tools/makebootefi.c
	$(CC) $(CFLAGS) -o bin/makebootefi tools/makebootefi.c

bin/boot.efi: bin/makebootefi bin/boot
	bin/makebootefi bin/boot bin/boot.efi

bin/makefatfs: tools/makefatfs.c
	$(CC) $(CFLAGS) -o bin/makefatfs tools/makefatfs.c

bin/fatfs: bin/makefatfs bin/boot.efi bin/toysfs
	bin/makefatfs bin/boot.efi bin/toysfs bin/fatfs

################################################################################
# Toys operating system disk image
################################################################################

MKGPT = tools/mkgpt
TOYS_FS_GUID = 23bbe6a3-25ec-4e2e-a8af-386177beb34c

bin/crc32.o: $(MKGPT)/crc32.h $(MKGPT)/crc32.c
	$(CC) $(CFLAGS) -c -o bin/crc32.o $(MKGPT)/crc32.c

bin/guid.o: $(MKGPT)/unaligned.h $(MKGPT)/guid.h $(MKGPT)/guid.c
	$(CC) $(CFLAGS) -c -o bin/guid.o $(MKGPT)/guid.c

bin/part_ids.o: $(MKGPT)/guid.h $(MKGPT)/part_ids.h $(MKGPT)/part_ids.c
	$(CC) $(CFLAGS) -c -o bin/part_ids.o $(MKGPT)/part_ids.c

bin/mkgpt.o: $(MKGPT)/mkgpt.c
	$(CC) $(CFLAGS) -c -o bin/mkgpt.o $(MKGPT)/mkgpt.c

bin/mkgpt: bin/crc32.o bin/guid.o bin/part_ids.o bin/mkgpt.o
	$(CC) $(CFLAGS) -o bin/mkgpt bin/crc32.o bin/guid.o bin/part_ids.o bin/mkgpt.o

bin/maketoysimg: tools/maketoysimg.c
	$(CC) $(CFLAGS) -o bin/maketoysimg tools/maketoysimg.c

bin/toys-$(VERSION)-amd64.img: bin/maketoysimg bin/fatfs
	bin/maketoysimg bin/fatfs bin/toys-$(VERSION)-amd64.img

################################################################################
# Automated tests
################################################################################

TEST_TOYC_SRCS = \
  test/toyc/base.toy\
  src/toyc/base.toy\
  src/toyc/scanner.toy\
  src/toyc/backend.toy\
  src/toyc/parser.toy\
  test/toyc/toyc.toy

bin/test_toyc: $(TOYCC) $(TEST_TOYC_SRCS)
	$(TOYCC) bin/test_toyc $(TEST_TOYC_SRCS)

bin/toyc_test: test/toyc_test.c
	$(CC) $(CFLAGS) -o bin/toyc_test test/toyc_test.c

toyc_test: bin/toyc_test bin/test_toyc $(TEST_TOYC_SRCS)
	bin/toyc_test bin/test_toyc $(TEST_TOYC_SRCS)

bin/toys_test: test/toys_test.c
	$(CC) $(CFLAGS) -o bin/toys_test test/toys_test.c

QEMU_CMD = qemu-system-x86_64 -machine q35 -enable-kvm \
  -drive if=pflash,format=raw,readonly=on,file=test/ovmf/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,readonly=on,file=test/ovmf/OVMF_VARS_4M.fd \
  -drive file=bin/toys-$(VERSION)-amd64.img,format=raw

toys_test: bin/toys_test bin/toys-$(VERSION)-amd64.img
	bin/toys_test $(QEMU_CMD) -serial stdio

################################################################################
# Manual tests
################################################################################

run: bin/toys-$(VERSION)-amd64.img
	$(QEMU_CMD)
