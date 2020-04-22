#include <string.h>

#include "defn.h"

// Prototypes
void exit_shell(char **argpointers, int argc);
void envset(char **argpointers, int argc);
void envunset(char **argpointers, int argc);
void cd(char **argpointers, int argc);

typedef void (*funcptr)(char **, int argc);

struct builtin {
    char *name;
    funcptr function;
};

static struct builtin builtins[] = {{"exit", exit_shell},
                             {"envset", envset},
                             {"envunset", envunset},
                             {"cd", cd}};

int check_for_builtin(char **argpointers, int argc) {
    int num_builtins = (int) (sizeof(builtins) / sizeof(builtins[0]));
    for (int i = 0; i < num_builtins; i++) {
        if (!strcmp(argpointers[0], builtins[i].name)) {
            (*builtins[i].function)(argpointers, argc);
            return 1;
        }
    }
    return 0;
}

void exit_shell(char **argpointers, int argc) {
    if (argc < 2) {
        exit(0);
    } else {
        exit(atoi(*(argpointers[1])));
    }
}

void envset(char **argpointers, int argc){
}

void envunset(char **argpointers, int argc) {
}

void cd(char **argpointers, int argc) {
}
