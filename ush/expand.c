#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "defn.h"

#define NON_ENV_OVERFLOW 1
#define PID_OVERFLOW 2
#define MATCHING_OVERFLOW 3
#define ENV_OVERFLOW 4
#define ARGC_OVERFLOW 5
#define ARGN_OVERFLOW 6
#define WILD_OVERFLOW 7

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
                int chars_printed = snprintf(&new[ptr], newsize - ptr, "%d", getpid());
                if (chars_printed > newsize - ptr) {
                    print_error(PID_OVERFLOW);
                    return 0;
                }
                ptr += chars_printed;
            } else if (orig[i] == '{') {
                // Try to parse an environment variable
                i++;
                char *var_name = &orig[i];
                while (orig[i] != '}') {
                    i++;
                    if (orig[i] == 0) {
                        print_error(MATCHING_OVERFLOW);
                        return 0;
                    }
                }
                // Use var_name as a substring
                orig[i] = 0;
                char *var_value = getenv(var_name);
                if (var_value != NULL) {
                    // Attempt to expand environment variable
                    int chars_printed = snprintf(&new[ptr], newsize - ptr, "%s", var_value);
                    if (chars_printed > newsize - ptr) {
                        print_error(ENV_OVERFLOW);
                        return 0;
                    }
                    ptr += chars_printed;
                }
                // Clean up after ourselves
                orig[i] = '}';
            } else if (orig[i] == '#') {
                // Apply shift offset
                int argc = mainargc - shift_offset;
                // If running a script, ush isn't argument 0
                if (argc > 1) {
                    // Decrement argc for accurate count
                    argc--;
                }
                // Attempt to expand number of arguments
                int chars_printed = snprintf(&new[ptr], newsize - ptr, "%d", argc);
                if (chars_printed > newsize - ptr) {
                    print_error(ARGC_OVERFLOW);
                    return 0;
                }
                ptr += chars_printed;
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
                    int chars_printed = snprintf(&new[ptr], newsize - ptr, "%s", argument);
                    if (chars_printed > newsize - ptr) {
                        print_error(ARGN_OVERFLOW);
                        return 0;
                    }
                    ptr += chars_printed;
                } else {
                    // Apply shift offset
                    argnumber += shift_offset;
                    if (argnumber >= minarg && argnumber < mainargc) {
                        // If argnumber is within bounds, attempt to expand it
                        char *argument = mainargv[argnumber];
                        int chars_printed = snprintf(&new[ptr], newsize - ptr, "%s", argument);
                        if (chars_printed > newsize - ptr) {
                            print_error(ARGN_OVERFLOW);
                            return 0;
                        }
                        ptr += chars_printed;
                    }
                }
                // Skip remaining original argument
                while (isdigit(orig[i])) {
                    i++;
                }
                // Step back one to prevent overwriting next character
                i--;
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
                        int chars_printed = snprintf(&new[ptr], newsize - ptr, "%s ", entname);
                        if (chars_printed > newsize - ptr) {
                            print_error(WILD_OVERFLOW);
                            return 0;
                        }
                        ptr += chars_printed;
                    }
                }
            } else {
                // Pattern matching *
                int pat_chars = 0;
                while (orig[i + pat_chars + 1] != 0) {
                    if (orig[i + pat_chars + 1] != ' ' && orig[i + pat_chars + 1] != '"') {
                        // Find end of pattern, throw error and return if a '/' is found
                        if (orig[i + pat_chars + 1] == '/') {
                            fprintf(stderr, "Wildcard pattern cannot contain '/'.\n");
                            return 0;
                        }
                        pat_chars++;
                    } else {
                        break;
                    }
                }
                // Declare and initialize pattern array
                char pattern[pat_chars];
                for (int j = 0; j < pat_chars; j++) {
                    pattern[j] = orig[i + j + 1];
                }
                while ((direntry = readdir(cur_dir))) {
                    // Find all valid entry names (doesn't start with '.')
                    char *entname = direntry->d_name;
                    if (entname[0] != '.') {
                        // Check if pattern matches
                        int pattern_pos = strlen(entname) - pat_chars;
                        if (!strncmp(&entname[pattern_pos], pattern, pat_chars)) {
                            entries_found++;
                            // Attempt to expand entry name
                            int chars_printed = snprintf(&new[ptr], newsize - ptr, "%s ", entname);
                            if (chars_printed > newsize - ptr) {
                                print_error(WILD_OVERFLOW);
                                return 0;
                            }
                            ptr += chars_printed;
                        }
                    }
                }
                if (entries_found) {
                    i += pat_chars;
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
            fprintf(stderr, "Non-env expansion overflowed.\n");
            break;
        case PID_OVERFLOW:
            fprintf(stderr, "PID expansion overflowed.\n");
            break;
        case MATCHING_OVERFLOW:
            fprintf(stderr, "Reached end of line before finding matching '}'.\n");
            break;
        case ENV_OVERFLOW:
            fprintf(stderr, "Environment variable expansion overflowed.\n");
            break;
        case ARGC_OVERFLOW:
            fprintf(stderr, "Argument count expansion overflowed.\n");
            break;
        case ARGN_OVERFLOW:
            fprintf(stderr, "Argument expansion overflowed.\n");
            break;
        case WILD_OVERFLOW:
            fprintf(stderr, "Wildcard expansion overflowed.\n");
            break;
    }
    fprintf(stderr, "Line processing halted.\n");
}
