#include "defn.h"

// Prototypes
void exit(int value);
void envset(char *name, char *value);
void envunset(char *name);
void cd(char *directory);

typedef void (*funcptr)(char **);

static funcptr builtins[] = {exit,envset,envunset,cd};

int check_for_builtin(char *argpointer, int argc) {
    return 0;
}

void run_builtin(int builtin, char **argpointers) {
    (*builtins[builtin])(argpointers);
}

void exit(char **argpointers) {
}

void envset(char **argpointers){
}

void envunset(char **argpointers) {
}

void cd(char **argpointers) {
}
