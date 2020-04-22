#include "defn.h"

// Prototypes
void exit_shell(char **argpointers);
void envset(char **argpointers);
void envunset(char **argpointers);
void cd(char **argpointers);

typedef void (*funcptr)(char **);

static funcptr builtins[] = {exit_shell,envset,envunset,cd};

int check_for_builtin(char *argpointer, int argc) {
    return 0;
}

void run_builtin(int builtin, char **argpointers) {
    (*builtins[builtin])(argpointers);
}

void exit_shell(char **argpointers) {
}

void envset(char **argpointers){
}

void envunset(char **argpointers) {
}

void cd(char **argpointers) {
}
