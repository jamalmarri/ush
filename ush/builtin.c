#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

typedef void (*funcptr)(char **, int argc);

// Used for clean function redirection
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

int check_for_builtin(char **argpointers, int argc) {
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
    if (argc < 3) {
        fprintf(stderr, "Not enough arguments for envset NAME VALUE.");
    } else {
        if (setenv(argpointers[1], argpointers[2], 1)) {
            perror("setenv");
        }
    }
}

void envunset(char **argpointers, int argc) {
    if (argc < 2) {
        fprintf(stderr, "Not enough arguments for envunset NAME.");
    } else {
        if (unsetenv(argpointers[1])) {
            perror("unsetenv");
        }
    }
}

void cd(char **argpointers, int argc) {
    if (argc < 2) {
        if (chdir(getenv("HOME"))) {
            fprintf(stderr, "Changing directory to home failed!\n");
        }
    } else {
        if (chdir(argpointers[1])) {
            perror("chdir");
        }
    }
}

void shift(char **argpointers, int argc) {
    int n;
    if (argc < 2) {
        n = 1;
    } else {
        n = atoi(argpointers[1]);
    }
    if (n < 0) {
        fprintf(stderr, "Invalid amount to shift.\n");
    } else if (mainargc - n < 1) {
        fprintf(stderr, "Not enough arguments to shift %d.\n", n);
    } else {
        shift_offset = n;
    }
}

void unshift(char **argpointers, int argc) {
    if (argc < 2) {
        shift_offset = 0;
    } else {
        int unshift_amount = atoi(argpointers[1]);
        if (unshift_amount < 0) {
            fprintf(stderr, "Invalid amount to unshift.\n");
        } else if (unshift_amount > shift_offset) {
            fprintf(stderr, "Only shifted %d right now. Can't unshift.\n", shift_offset);
        } else {
            shift_offset -= unshift_amount;
        }
    }
}

void sstat(char **argpointers, int argc) {
    if (argc < 2) {
        fprintf(stderr, "Not enough arguments for sstat FILE [FILE...]");
    } else {
        for (int i = 1; i < argc; i++) {
            struct stat *buf;
            if (stat(argpointers[i], buf)) {
                perror("stat");
                continue;
            }
            fprintf(stderr, "%s ", argpointers[i]);
            struct passwd *userinfo = getpwuid(buf->st_uid);
            if (userinfo == NULL) {
                fprintf(stderr, "%d ", buf->st_uid);
            } else {
                fprintf(stderr, "%s ", userinfo->pw_name);
            }
            struct group *groupinfo = getgrgid(buf->st_gid);
            if (groupinfo == NULL) {
                fprintf(stderr, "%d ", buf->st_gid);
            } else {
                fprintf(stderr, "%s ", groupinfo->gr_name);
            }
            char mode[12];
            strmode(buf->st_mode, mode);
            fprintf(stderr, "%s ", mode);
            fprintf(stderr, "%d %d %s\n", buf->st_nlink, buf->st_size, buf->st_mtime);
        }
    }
}
