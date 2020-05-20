/*
 * CSCI 347 Microshell
 * Jamal Marri
 * Spring Quarter 2020
 */

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "defn.h"

/* Constants */
#define LINELEN 200000

/* Prototypes */
int remove_comments(char *buffer);
char ** arg_parse(char *line, int *argcptr);
int check_for_quotes(const char *line, int *ptr);
void strip_quotes(char *arg);
void catch_signal(int signal);

/* Shell main */
int main(int argc, char **argv) {
    FILE *inputfile;
    int interactive; // "Boolean" representing if the shell is in interactive mode
    char buffer [LINELEN];
    int len;
    // Initialize global references to argc and argv
    mainargc = argc;
    mainargv = argv;
    // Register catch_signal as the action to be taken for SIGINT
    struct sigaction sa;
    sa.sa_handler = catch_signal;
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction");
    }
    if (argc > 1) {
        // Attempt to open inputted script file
        inputfile = fopen(mainargv[1], "r");
        interactive = 0;
    } else {
        // Just take standard input
        inputfile = stdin;
        interactive = 1;
    }
    if (inputfile == NULL) {
        perror("fopen");
        return 127;
    }
    while (1) {
        // Reset sigint global
        sigint_caught = 0;
        // Prompt and get line
        if (interactive) {
            fprintf (stderr, "%% ");
        }
        if (fgets (buffer, LINELEN, inputfile) != buffer) {
            break;
        }
        // Get rid of \n at end of buffer
        if (!remove_comments(buffer)) {
            len = strlen(buffer);
            if (buffer[len-1] == '\n') {
                buffer[len-1] = 0;
            }
        }
        // Run it...
        processline (buffer, 0, 1, WAIT | EXPAND);
    }
    if (!feof(inputfile)) {
        perror ("read");
    }
    // Also known as exit(0)
    return 0;
}

int remove_comments(char *buffer) {
    for (int i = 0; buffer[i] != 0; i++) {
        // If comment is found, remove it
        if (buffer[i] == '#' && buffer[i - 1] != '$') {
            buffer[i] = 0;
            return 1;
        }
    }
    return 0;
}

int processline(char *line, int infd, int outfd, int flags) {
    int argc;
    char **argpointers;
    pid_t cpid = 0;
    // Attempt to expand if flags say to do so
    if (flags & EXPAND) {
        char expanded_line[LINELEN];
        if (expand(line, expanded_line, LINELEN)) {
            argpointers = arg_parse(expanded_line, &argc);
        } else {
            return -1;
        }
    } else {
        // Otherwise, just parse the original line
        argpointers = arg_parse(line, &argc);
    }
    // Only proceed if any arguments were found
    if (argc > 0) {
        // No need to fork if the command is a shell builtin
        if (!check_for_builtin(argpointers, argc)) {
            // Attempt to fork the process
            cpid = fork();
            if (cpid < 0) {
                perror("fork");
                return -1;
            }
            // Check if this process is the new child process
            if (cpid == 0) {
                // Replace stdin with infd and then close infd
                if (infd != 0) {
                    if (dup2(infd, 0) < 0) {
                        perror("dup2");
                        return -1;
                    }
                    close infd;
                }
                // Replace stdout with outfd and then close outfd
                if (outfd != 1) {
                    if (dup2(outfd, 1) < 0) {
                        perror("dup2");
                        return -1;
                    }
                    close outfd;
                }
                // Attempt to execute the command
                execvp(argpointers[0], argpointers);
                // If this line is reached, there must have been an error
                perror("exec");
                fclose(stdin);
                exit(127);
            }
            // Wait on the child process if the flags say to do so
            if (flags & WAIT) {
                int status;
                if (wait(&status) < 0) {
                    perror("wait");
                    return -1;
                } else {
                    // Update last exit global value
                    if (WIFEXITED(status)) {
                        last_exit = WEXITSTATUS(status);
                    }
                    if (WIFSIGNALED(status)) {
                        // Print signal description
                        if (WTERMSIG(status) != SIGINT) {
                            printf("%s", strsignal(WTERMSIG(status)));
                            #ifdef WCOREDUMP
                            if (WCOREDUMP(status)) {
                                printf(" (core dumped)");
                            }
                            #endif
                            printf("\n");
                        }
                        last_exit = 128 + WTERMSIG(status);
                    }
                }
            } else {
                return cpid;
            }
        }
    }
    free(argpointers);
    return 0;
}

char ** arg_parse(char *line, int *argcptr) {
    int argc = 0;
    int ptr = 0; // "Pointer" for current position in line
    // Count arguments to determine value of argc
    while (line[ptr] != 0) {
        if (line[ptr] == ' ') {
            // Skip leading/trailing spaces
            ptr++;
        } else {
            // Argument found
            argc++;
            // Find end of argument
            while (line[ptr] != ' ' && line[ptr] != 0) {
                if (!check_for_quotes(line, &ptr)) {
                    *argcptr = 0;
                    return NULL;
                }
                ptr++;
            }
        }
    }
    // Return if no arguments were found
    if (argc < 1) {
        *argcptr = 0;
        return NULL;
    }
    // Attempt to malloc for argpointers array
    char ** argpointers = malloc(sizeof(char *) * (argc + 1));
    if (argpointers == NULL) {
        fprintf(stderr, "Malloc of argument pointer array failed.\n");
        *argcptr = 0;
        return NULL;
    }
    // Initialize indexing integers
    ptr = 0;
    int index = 0; // Current index of argpointers
    // Populate argpointers
    while (line[ptr] != 0 && index < argc) {
        if (line[ptr] == ' ') {
            // Skip leading/trailing spaces
            ptr++;
        } else {
            // Argument found
            argpointers[index] = &line[ptr];
            index++;
            // Find end of argument and replace trailing space with 0
            while (line[ptr] != ' ' && line[ptr] != 0) {
                if (!check_for_quotes(line, &ptr)) {
                    *argcptr = 0;
                    return NULL;
                }
                ptr++;
            }
            line[ptr] = 0;
            ptr++;
        }
    }
    // Set last element to NULL for execvp
    argpointers[argc] = NULL;
    // Remove quotes from all arguments
    for (index = 0; index < argc; index++) {
        strip_quotes(argpointers[index]);
    }
    // Set argc upstream
    *argcptr = argc;
    return argpointers;
}

int check_for_quotes(const char *line, int *ptr) {
    if (line[*ptr] == '"') {
        // If quote is found, find its partner
        int tmp_ptr = *ptr;
        tmp_ptr++;
        while (line[tmp_ptr] != '"' && line[tmp_ptr] != 0) {
            tmp_ptr++;
        }
        // Print error and return if EOS is reached before another quote is found
        if (line[tmp_ptr] == 0) {
            fprintf(stderr, "Odd number of quotes found in input.\n");
            return 0;
        }
        *ptr = tmp_ptr;
    }
    return 1;
}

void strip_quotes(char *arg) {
    int i = 0; // Current index of arg
    int offset = 0; // Current offset (number of quotes)
    while (arg[i] != 0) {
        while (arg[i + offset] == '"') {
            offset++;
        }
        arg[i] = arg[i + offset];
        i++;
    }
}

void catch_signal(int signal) {
    sigint_caught = 1;
}
