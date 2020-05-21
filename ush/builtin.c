/*
 * CSCI 347 Microshell
 * Jamal Marri
 * Spring Quarter 2020
 */

#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "defn.h"

// Prototypes
void exit_shell(char **argpointers, int argc);
void envset(char **argpointers, int argc);
void envunset(char **argpointers, int argc);
void cd(char **argpointers, int argc);
void shift(char **argpointers, int argc);
void unshift(char **argpointers, int argc);
void sstat(char **argpointers, int argc);

// Global Variables
int builtin_outfd;

// Used for clean function redirection
typedef void (*funcptr)(char **, int argc);
struct builtin {
  char *name;
  funcptr function;
};

// List of builtins
static struct builtin builtins[] = {{"exit", exit_shell},
                                    {"envset", envset},
                                    {"envunset", envunset},
                                    {"cd", cd},
                                    {"shift", shift},
                                    {"unshift", unshift},
                                    {"sstat", sstat}};

int check_for_builtin(char **argpointers, int argc, int outfd) {
  builtin_outfd = outfd;
  // Try to locate the correct builtin
  int num_builtins = (int) (sizeof(builtins) / sizeof(builtins[0]));
  for (int i = 0; i < num_builtins; i++) {
    if (!strcmp(argpointers[0], builtins[i].name)) {
      // Execute builtin passing arguments and argc
      (*builtins[i].function)(argpointers, argc);
      return 1;
    }
  }
  return 0;
}

void exit_shell(char **argpointers, int argc) {
  if (argc < 2) {
    // Default exit code
    exit(0);
  } else {
    exit(atoi(argpointers[1]));
  }
}

void envset(char **argpointers, int argc){
  if (argc < 3 || argc > 3) {
    fprintf(stderr, "Usage: envset NAME VALUE.\n");
    last_exit = 1;
  } else {
    if (setenv(argpointers[1], argpointers[2], 1)) {
      perror("setenv");
      last_exit = 1;
    } else {
      last_exit = 0;
    }
  }
}

void envunset(char **argpointers, int argc) {
  if (argc < 2 || argc > 2) {
    fprintf(stderr, "Usage: envunset NAME.\n");
    last_exit = 1;
  } else {
    if (unsetenv(argpointers[1])) {
      perror("unsetenv");
      last_exit = 1;
    } else {
      last_exit = 0;
    }
  }
}

void cd(char **argpointers, int argc) {
  if (argc < 2) {
    if (chdir(getenv("HOME"))) {
      fprintf(stderr, "Changing directory to home failed!\n");
      last_exit = 1;
    } else {
      last_exit = 0;
    }
  } else if (argc > 2) {
    fprintf(stderr, "Usage: cd [DIR]\n");
    last_exit = 1;
  } else {
    if (chdir(argpointers[1])) {
      perror("chdir");
      last_exit = 1;
    } else {
      last_exit = 0;
    }
  }
}

void shift(char **argpointers, int argc) {
  int n;
  // Determine shift value
  if (argc < 2) {
    // Default value
    n = 1;
  } else if (argc > 2) {
    fprintf(stderr, "Usage: shift [VALUE]\n");
    last_exit = 1;
  } else {
    n = atoi(argpointers[1]);
  }
  // Check for valid shift amount
  if (n < 0) {
    fprintf(stderr, "Invalid amount to shift.\n");
    last_exit = 1;
  } else if (mainargc - (n + cur_shift) < 2) {
    fprintf(stderr, "Not enough arguments to shift %d.\n", n);
    last_exit = 1;
  } else {
    cur_shift += n;
    last_exit = 0;
  }
}

void unshift(char **argpointers, int argc) {
  if (argc < 2) {
    // Default case
    cur_shift = 0;
    last_exit = 0;
  } else if (argc > 2) {
    fprintf(stderr, "Usage: unshift [VALUE]\n");
    last_exit = 1;
  } else {
    int unshift_amount = atoi(argpointers[1]);
    // Check for valid unshift amount
    if (unshift_amount < 0) {
      fprintf(stderr, "Invalid amount to unshift.\n");
      last_exit = 1;
    } else if (unshift_amount > cur_shift) {
      fprintf(stderr, "Only shifted %d right now. Can't unshift.\n", cur_shift);
      last_exit = 1;
    } else {
      cur_shift -= unshift_amount;
      last_exit = 0;
    }
  }
}

void sstat(char **argpointers, int argc) {
  if (argc < 2) {
    fprintf(stderr, "Usage: sstat FILE [FILE...]\n");
    last_exit = 1;
  } else {
    int exit_value = 0;
    // For every file specified...
    for (int i = 1; i < argc; i++) {
      // Attempt to stat it
      struct stat buf;
      int statreturn = stat(argpointers[i], &buf);
      if (statreturn) {
        perror("stat");
        exit_value = 1;
        continue;
      }
      // Print the file name
      dprintf(builtin_outfd, "%s ", argpointers[i]);
      // Print the username, or UID if username isn't found
      struct passwd *userinfo = getpwuid(buf.st_uid);
      if (userinfo == NULL) {
        dprintf(builtin_outfd, "%d ", buf.st_uid);
      } else {
        dprintf(builtin_outfd, "%s ", userinfo->pw_name);
      }
      // Print the groupname, or GID if groupname isn't found
      struct group *groupinfo = getgrgid(buf.st_gid);
      if (groupinfo == NULL) {
        dprintf(builtin_outfd, "%d ", buf.st_gid);
      } else {
        dprintf(builtin_outfd, "%s ", groupinfo->gr_name);
      }
      // Print the permission bits in a nice format
      char mode[12];
      strmode(buf.st_mode, mode);
      dprintf(builtin_outfd, "%s", mode);
      // Get date and format it
      char *date = asctime(localtime(&buf.st_mtime));
      // Print number of links, size in bytes, and formatted date
      dprintf(builtin_outfd, "%ld %ld %s", buf.st_nlink, buf.st_size, date);
    }
    last_exit = exit_value;
  }
}
