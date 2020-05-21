/*
 * CSCI 347 Microshell
 * Jamal Marri
 * Spring Quarter 2020
 */

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "defn.h"

#define NON_ENV_OVERFLOW 1
#define PID_OVERFLOW 2
#define MATCHING_ENV_OVERFLOW 3
#define ENV_OVERFLOW 4
#define MATCHING_CMD_OVERFLOW 5
#define CMD_FORK_ERROR 6
#define CMD_EXP_OVERFLOW 7
#define ARGC_OVERFLOW 8
#define ARGN_OVERFLOW 9
#define LAST_EXIT_OVERFLOW 10
#define WILD_OVERFLOW 11

// Prototypes
void print_error(int error_type);

int expand(char *orig, char *new, int newsize) {
  // "Pointer" for current position in new
  int ptr = 0;
  for (int i = 0; orig[i] != 0; i++) {
    // Check if any environment variables are possible
    if (orig[i] == '$') {
      i++;
      if (orig[i] == '$') {
        // Attempt to expand PID
        int chars = snprintf(&new[ptr], newsize - ptr, "%d", getpid());
        if (chars > newsize - ptr) {
          print_error(PID_OVERFLOW);
          return 0;
        }
        ptr += chars;
      } else if (orig[i] == '{') {
        // Try to parse an environment variable
        i++;
        char *var_name = &orig[i];
        while (orig[i] != '}') {
          i++;
          if (orig[i] == 0) {
            print_error(MATCHING_ENV_OVERFLOW);
            return 0;
          }
        }
        // Use var_name as a substring
        orig[i] = 0;
        char *value = getenv(var_name);
        if (value != NULL) {
          // Attempt to expand environment variable
          int chars = snprintf(&new[ptr], newsize - ptr, "%s", value);
          if (chars > newsize - ptr) {
            print_error(ENV_OVERFLOW);
            return 0;
          }
          ptr += chars;
        }
        // Clean up after ourselves
        orig[i] = '}';
      } else if (orig[i] == '(') {
        // Attempt command expansion
        i++;
        char *cmd_exp = &orig[i];
        int stack; // Integer representing the "stack" for all parentheses found
        // Find matching parenthesis
        for (stack = 1; stack > 0; i++) {
          if (orig[i] == '(') {
            stack++;
          } else if (orig[i] == ')') {
            stack--;
          } else if (orig[i] == 0) {
            print_error(MATCHING_CMD_OVERFLOW);
            return 0;
          }
        }
        // Use cmp_exp as a substring
        orig[i - 1] = 0;
        // Create a new pipe
        int pipefd[2];
        if (pipe(pipefd)) {
          perror("pipe");
          return 0;
        }
        // Call processline using cmd_exp as buffer and pipe as outfd
        int cpid = processline(cmd_exp, 0, pipefd[1], NOWAIT | EXPAND);
        if (cpid < 0) {
          print_error(CMD_FORK_ERROR);
          if (close(pipefd[0])) {
            perror("close");
          }
          if (close(pipefd[1])) {
            perror("close");
          }
          return 0;
        }
        // No writing is done on this end
        if (close(pipefd[1])) {
          perror("close");
          return 0;
        }
        // Read from pipe
        int tmp_ptr = ptr; // Save current pointer for later
        int chars = -1;
        while (chars != 0) {
          if (sigint_caught) {
            if (close(pipefd[0])) {
              perror("close");
            }
            return 0;
          }
          chars = read(pipefd[0], &new[ptr], newsize - ptr);
          if (chars < 0) {
            perror("read");
            if (close(pipefd[0])) {
              perror("close");
            }
            return 0;
          }
          if (chars >= newsize - ptr) {
            print_error(CMD_EXP_OVERFLOW);
            if (close(pipefd[0])) {
              perror("close");
            }
            return 0;
          }
          ptr += chars;
        }
        // Remove ending newline
        if (new[ptr - 1] == '\n') {
          ptr--;
        }
        // Replace all other newlines with spaces
        while (tmp_ptr < ptr) {
          if (new[tmp_ptr] == '\n') {
            new[tmp_ptr] = ' ';
          }
          tmp_ptr++;
        }
        // Close read end of pipe
        if (close(pipefd[0])) {
          perror("close");
          return 0;
        }
        // Wait on child if one was created
        if (cpid > 0) {
          waiting_on = cpid;
          int status;
          if (waitpid(cpid, &status, 0) < 0) {
            perror("waitpid");
          } else {
            waiting_on = 0;
            // Update last exit global value
            if (WIFEXITED(status)) {
              last_exit = WEXITSTATUS(status);
            }
          }
        }
        // Clean up after ourselves
        i--;
        orig[i] = ')';
      } else if (orig[i] == '#') {
        // Apply shift offset
        int argc = mainargc - cur_shift;
        // If running a script, ush isn't argument 0
        if (argc > 1) {
          // Decrement argc for accurate count
          argc--;
        }
        // Attempt to expand number of arguments
        int chars = snprintf(&new[ptr], newsize - ptr, "%d", argc);
        if (chars > newsize - ptr) {
          print_error(ARGC_OVERFLOW);
          return 0;
        }
        ptr += chars;
      } else if (isdigit(orig[i])) {
        int minarg = 0;
        int argnumber = atoi(&orig[i]);
        // If running a script, ush isn't argument 0
        if (mainargc > 1) {
          // Update argnumber and minarg appropriately
          argnumber++;
          minarg = 1;
        }
        if (argnumber == minarg) {
          // Base case (script name)
          char *argument = mainargv[argnumber];
          // Attempt to expand base argument (script name)
          int chars = snprintf(&new[ptr], newsize - ptr, "%s", argument);
          if (chars > newsize - ptr) {
            print_error(ARGN_OVERFLOW);
            return 0;
          }
          ptr += chars;
        } else {
          // Apply shift offset
          argnumber += cur_shift;
          if (argnumber >= minarg && argnumber < mainargc) {
            // If argnumber is within bounds, attempt to expand it
            char *argument = mainargv[argnumber];
            int chars = snprintf(&new[ptr], newsize - ptr, "%s", argument);
            if (chars > newsize - ptr) {
              print_error(ARGN_OVERFLOW);
              return 0;
            }
            ptr += chars;
          }
        }
        // Skip remaining original argument
        while (isdigit(orig[i])) {
          i++;
        }
        // Step back one to prevent overwriting next character
        i--;
      } else if (orig[i] == '?') {
        // Attempt to expand last command exit value
        int chars = snprintf(&new[ptr], newsize - ptr, "%d", last_exit);
        if (chars > newsize - ptr) {
          print_error(LAST_EXIT_OVERFLOW);
          return 0;
        }
        ptr += chars;
      } else {
        // We got ahead of ourselves
        // Back it up and copy the $
        i--;
        new[ptr] = orig[i];
        ptr++;
      }
    } else if (orig[i] == '*' && (orig[i - 1] == ' ' || orig[i - 1] == '"')) {
      // Valid wildcard found, attempt to open the current directory
      DIR *cur_dir = opendir(".");
      if (cur_dir == NULL) {
        perror("opendir");
        return 0;
      }
      struct dirent *direntry;
      int entries_found = 0;
      if (orig[i + 1] == ' ' || orig[i + 1] == '"' || orig[i + 1] == 0) {
        // Default *
        while ((direntry = readdir(cur_dir))) {
          // Find all valid entry names (doesn't start with '.')
          char *entname = direntry->d_name;
          if (entname[0] != '.') {
            entries_found++;
            // Attempt to expand entry name
            int chars = snprintf(&new[ptr], newsize - ptr, "%s ", entname);
            if (chars > newsize - ptr) {
              print_error(WILD_OVERFLOW);
              return 0;
            }
            ptr += chars;
          }
        }
      } else {
        // Pattern matching *
        int pat_chrs = 0;
        while (orig[i + pat_chrs + 1] != 0) {
          if (orig[i + pat_chrs + 1] != ' ' && orig[i + pat_chrs + 1] != '"') {
            // Find end of pattern, throw error and return if a '/' is found
            if (orig[i + pat_chrs + 1] == '/') {
              fprintf(stderr, "Wildcard pattern cannot contain '/'.\n");
              return 0;
            }
            pat_chrs++;
          } else {
            break;
          }
        }
        // Declare and initialize pattern array
        char pattern[pat_chrs];
        for (int j = 0; j < pat_chrs; j++) {
          pattern[j] = orig[i + j + 1];
        }
        while ((direntry = readdir(cur_dir))) {
          // Find all valid entry names (doesn't start with '.')
          char *entname = direntry->d_name;
          if (entname[0] != '.') {
            // Check if pattern matches
            int pattern_pos = strlen(entname) - pat_chrs;
            if (!strncmp(&entname[pattern_pos], pattern, pat_chrs)) {
              entries_found++;
              // Attempt to expand entry name
              int chars = snprintf(&new[ptr], newsize - ptr, "%s ", entname);
              if (chars > newsize - ptr) {
                print_error(WILD_OVERFLOW);
                return 0;
              }
              ptr += chars;
            }
          }
        }
        if (entries_found) {
          i += pat_chrs;
        } else {
          // If no entries were found, simply print the '*' and continue
          new[ptr] = '*';
          ptr++;
        }
      }
      // Remove last trailing space character if any valid entries were found
      if (entries_found) {
        ptr--;
      }
      // Attempt to close current directory
      if (closedir(cur_dir)) {
        perror("closedir");
      }
    } else if (orig[i] == '*' && orig[i - 1] == '\\') {
      // Escape sequence '\*', just print '*'
      new[ptr - 1] = '*';
    } else {
      // Business as usual, copy the character
      if (ptr < newsize) {
        new[ptr] = orig[i];
        ptr++;
      } else {
        print_error(NON_ENV_OVERFLOW);
        return 0;
      }
    }
  }
  // Place a 0 at the end to prevent old remnants from leaking out
  if (ptr < newsize) {
    new[ptr] = 0;
  } else {
    print_error(NON_ENV_OVERFLOW);
    return 0;
  }
  return 1;
}

void print_error(int error_type) {
  switch (error_type) {
    case NON_ENV_OVERFLOW:
      fprintf(stderr, "Non-env expansion overflowed. ");
      break;
    case PID_OVERFLOW:
      fprintf(stderr, "PID expansion overflowed. ");
      break;
    case MATCHING_ENV_OVERFLOW:
      fprintf(stderr, "Reached end of line before finding matching '}'. ");
      break;
    case ENV_OVERFLOW:
      fprintf(stderr, "Environment variable expansion overflowed. ");
      break;
    case MATCHING_CMD_OVERFLOW:
      fprintf(stderr, "Reached end of line before finding matching ')'. ");
      break;
    case CMD_FORK_ERROR:
      fprintf(stderr, "Fork during command expansion failed. ");
      break;
    case CMD_EXP_OVERFLOW:
      fprintf(stderr, "Command expansion overflowed. ");
      break;
    case ARGC_OVERFLOW:
      fprintf(stderr, "Argument count expansion overflowed. ");
      break;
    case ARGN_OVERFLOW:
      fprintf(stderr, "Argument expansion overflowed. ");
      break;
    case LAST_EXIT_OVERFLOW:
      fprintf(stderr, "Last exit value expansion overflowed. ");
      break;
    case WILD_OVERFLOW:
      fprintf(stderr, "Wildcard expansion overflowed. ");
      break;
  }
  fprintf(stderr, "Line processing halted.\n");
}
