#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "defn.h"

#define NON_ENV_OVERFLOW 1
#define PID_OVERFLOW 2
#define MATCHING_OVERFLOW 3
#define ENV_OVERFLOW 4

void print_error(int error_type);

int expand(char *orig, char *new, int newsize) {
    for (int i = 0; orig[i] != 0; i++) {
        if (orig[i] == '$') {
            i++;
            if (orig[i] == '$') {
                if (snprintf(new, newsize, "\d", getpid()) > newsize) {
                    print_error(PID_OVERFLOW);
                    return 0;
                }
            } else if (orig[i] == '{') {
                i++;
                char *var_name = &orig[i];
                while (orig[i] != '}') {
                    i++;
                    if (orig[i] == 0) {
                        print_error(MATCHING_OVERFLOW);
                        return 0;
                    }
                }
                orig[i] = 0;
                char *var_value = getenv(var_name);
                if (var_value != NULL) {
                    if (snprintf(new, newsize, "\s", var_value) > newsize) {
                        print_error(ENV_OVERFLOW);
                        return 0;
                    }
                }
                orig[i] = '}';
            }
        } else {
            if (i < newsize) {
                new[i] = orig[i];
            } else {
                print_error(NON_ENV_OVERFLOW);
                return 0;
            }
        }
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
    }
    fprintf(stderr, "Line processing halted.\n");
}
