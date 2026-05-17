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

#define _POSIX_SOURCE

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/wait.h>

// A log of QEMU's serial output, with ASCII Control Sequence Introducer escape
// codes filtered out, and consecutive space characters merged into one. See
// en.wikipedia.org/wiki/ANSI_escape_code#Control_Sequence_Introducer_commands.

typedef struct {
  char *content;
  size_t length;
  size_t capacity;
  // True if last char was ESC (0x1B).
  bool start_of_escape_code;
  // True if ESC was followed by '[', and no "final byte" was found yet.
  bool in_escape_code;
} log_t;

log_t *log_new(size_t capacity) {
  char *content = (char *)malloc(capacity);
  log_t *log = (log_t *)malloc(sizeof(log_t));
  if (log == NULL || content == NULL) {
    return NULL;
  }
  log->content = content;
  log->length = 0;
  log->capacity = capacity;
  log->start_of_escape_code = false;
  log->in_escape_code = false;
  return log;
}

bool log_append(log_t *self, uint8_t c) {
  if (c == 0x1B) {
    self->start_of_escape_code = true;
    self->in_escape_code = false;
    return true;
  }
  if (self->start_of_escape_code) {
    self->start_of_escape_code = false;
    self->in_escape_code = c == '[';
    return true;
  }
  if (self->in_escape_code) {
    // Is 'c' a "final byte" of a Control Sequence Introducer command?
    if (c >= 0x40 && c <= 0x7E) {
      self->in_escape_code = false;
    }
    return true;
  }
  if (c == ' ' && self->length > 0 && self->content[self->length - 1] == ' ') {
    return true;
  }
  if (self->length >= self->capacity) {
    return false;
  }
  self->content[self->length++] = c;
  return true;
}

bool log_starts_with(log_t *self, const char *str) {
  const size_t len = strlen(str);
  return len <= self->length && strncmp(self->content, str, len) == 0;
}

bool log_ends_with(log_t *self, const char *str) {
  const size_t len = strlen(str);
  size_t trimmed_length = self->length;
  while (trimmed_length > 0 && self->content[trimmed_length - 1] == ' ') {
    trimmed_length -= 1;
  }
  return len <= trimmed_length &&
         strncmp(self->content + trimmed_length - len, str, len) == 0;
}

void log_clear(log_t *self) { self->length = 0; }

int error(const char *msg) {
  printf("%s\n", msg);
  return 1;
}

int send_command(int to_qemu, const char *command) {
  const size_t len = strlen(command);
  if (write(to_qemu, command, len) != (ssize_t)len) {
    return error("Can't send command to QEMU");
  }
  return 0;
}

// Tests that Toys is fully self-hosting by using it (in QEMU) to recompile the
// Toy compiler, recompile the kernel with this new compiler, and to check that
// everything works with a reboot.
int test_self_hosting(int to_qemu, int from_qemu) {
  log_t *log = log_new(8192);
  if (log == NULL) {
    return error("Out of memory");
  }

  int step = 0;
  const char *command = NULL;
  const char *echo = NULL;
  bool wait_prompt = true;

  while (true) {
    uint8_t buffer[512];
    int count = read(from_qemu, buffer, sizeof(buffer));
    if (count < 0) {
      return error("Can't read QEMU's output");
    }
    for (int i = 0; i < count; ++i) {
      log_append(log, buffer[i]);
    }
    if (wait_prompt) {
      if (!log_ends_with(log, ">")) {
        continue;
      }
      wait_prompt = false;
    } else {
      if (log_ends_with(log, echo)) {
        wait_prompt = true;
      }
      continue;
    }
    switch (step) {
    case 0:
      // Test that Toys booted correctly.
      if (!log_ends_with(log,
                         "Type 'list' for a list of available commands. >")) {
        return error("Boot failed!");
      }
      // Test that 'list' works.
      command = "list\n";
      echo = ">list >list";
      if (send_command(to_qemu, command) != 0) {
        return 1;
      }
      break;
    case 1:
      if (!log_ends_with(
              log,
              "copy delete edit list reboot shell snake src/ stat toyc >")) {
        return error("List command failed!");
      }
      // Test that when a program crashes, it does not crash the whole OS.
      // For this try to launch the kernel as an application. This should fail
      // since the kernel does not provide the expected 'entry' point, and uses
      // privileged instructions.
      command = "bin/toys\n";
      echo = ">bin/toys >bin/toys";
      if (send_command(to_qemu, command) != 0) {
        return 1;
      }
      break;
    case 2:
      if (!log_ends_with(log, "bin/toys crashed >")) {
        return error("Crash test failed!");
      }
      // Test that the Toy compiler can compile itself.
      command = "shell src/toyc/BUILD\n";
      echo = ">shell src/toyc/BUILD";
      if (send_command(to_qemu, command) != 0) {
        return 1;
      }
      break;
    case 3:
      if (!log_ends_with(log, ">shell src/toyc/BUILD >")) {
        return error("Toyc recompilation failed!");
      }
      // Test that the recompiled compiler works by using it to recompile
      // the kernel.
      command = "shell src/toys/BUILD\n";
      echo = ">shell src/toys/BUILD";
      if (send_command(to_qemu, command) != 0) {
        return 1;
      }
      break;
    case 4:
      if (!log_ends_with(log, ">shell src/toys/BUILD >")) {
        return error("Toys kernel recompilation failed!");
      }
      // Test that the recompiled kernel work by rebooting.
      command = "reboot\n";
      echo = ">reboot";
      if (send_command(to_qemu, command) != 0) {
        return 1;
      }
      break;
    case 5:
      // Test that the new kernel booted correctly.
      if (!log_ends_with(log,
                         "Type 'list' for a list of available commands. >")) {
        return error("Reboot failed!");
      }
      return 0;
    }
    log_clear(log);
    step += 1;
  }
}

#define INPUT 0
#define OUTPUT 1

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: %s <qemu command line>\n", argv[0]);
    printf("\n");
    printf("Runs a series of tests from inside Toys, running in QEMU, to check "
           "that it is fully self-hosted. For this, the provided command line "
           "should launch Toys in QEMU (it is used to fork a child process, to "
           "which test command lines are sent).\n");
    return 1;
  }
  int from_qemu[2];
  int to_qemu[2];
  if (pipe(from_qemu) != 0 || pipe(to_qemu) != 0) {
    return error("Can't create pipes for QEMU child process");
  }
  int child_pid = fork();
  if (child_pid == 0) {
    dup2(to_qemu[INPUT], STDIN_FILENO);
    dup2(from_qemu[OUTPUT], STDOUT_FILENO);
    close(from_qemu[INPUT]);
    close(from_qemu[OUTPUT]);
    close(to_qemu[INPUT]);
    close(to_qemu[OUTPUT]);
    execvp(argv[1], argv + 1);
    return 0;
  } else {
    close(from_qemu[OUTPUT]);
    close(to_qemu[INPUT]);
    int result = test_self_hosting(to_qemu[OUTPUT], from_qemu[INPUT]);
    kill(child_pid, SIGKILL);
    waitpid(child_pid, NULL, 0);
    return result;
  }
}