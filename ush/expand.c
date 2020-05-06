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
                // Attempt to expand number of arguments
                int argc = mainargc - shift_offset;
                if (argc > 1) {
                    argc--;
                }
                int chars_printed = snprintf(&new[ptr], newsize - ptr, "%d", argc);
                if (chars_printed > newsize - ptr) {
                    print_error(ARGC_OVERFLOW);
                    return 0;
                }
                ptr += chars_printed;
            } else if (isdigit(orig[i])) {
                int minarg = 0;
                int argnumber = atoi(&orig[i]);
                if (mainargc > 1) {
                    argnumber++;
                    minarg = 1;
                }
                if (argnumber == minarg) {
                    char *argument = mainargv[argnumber];
                    int chars_printed = snprintf(&new[ptr], newsize - ptr, "%s", argument);
                    if (chars_printed > newsize - ptr) {
                        print_error(ARGN_OVERFLOW);
                        return 0;
                    }
                    ptr += chars_printed;
                } else {
                    argnumber += shift_offset;
                    if (argnumber >= minarg && argnumber <= mainargc - shift_offset) {
                        char *argument = mainargv[argnumber];
                        int chars_printed = snprintf(&new[ptr], newsize - ptr, "%s", argument);
                        if (chars_printed > newsize - ptr) {
                            print_error(ARGN_OVERFLOW);
                            return 0;
                        }
                        ptr += chars_printed;
                    }
                }
                while (isdigit(orig[i])) {
                    i++;
                }
            } else {
                // We got ahead of ourselves
                // Back it up and copy the $
                i--;
                new[ptr] = orig[i];
                ptr++;
            }
        } else if (orig[i] == '*' && (orig[i - 1] == ' ' || orig[i - 1] == '"')) {
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
                    char *entname = direntry->d_name;
                    if (entname[0] != '.') {
                        entries_found++;
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
                        if (orig[i + pat_chars + 1] == '/') {
                            fprintf(stderr, "Wildcard pattern cannot contain '/'.\n");
                            return 0;
                        }
                        pat_chars++;
                    } else {
                        break;
                    }
                }
                char pattern[pat_chars];
                for (int j = 0; j < pat_chars; j++) {
                    pattern[j] = orig[i + j + 1];
                }
                while ((direntry = readdir(cur_dir))) {
                    char *entname = direntry->d_name;
                    if (entname[0] != '.') {
                        int pattern_pos = strlen(entname) - pat_chars;
                        if (!strncmp(&entname[pattern_pos], pattern, 1)) {
                            entries_found++;
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
                }
            }
            if (entries_found) {
                ptr--;
            }
            if (closedir(cur_dir)) {
                perror("closedir");
            }
        } else if (orig[i] == '*' && orig[i - 1] == '\\') {
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
