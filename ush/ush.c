/*
 * CSCI 347 Microshell
 * Jamal Marri
 * Spring Quarter 2020
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "defn.h"

/* Constants */

#define LINELEN 1024

/* Prototypes */

int remove_comments(char *buffer);
void processline (char *line);
char ** arg_parse (char *line, int *argcptr);
int check_for_quotes (const char *line, int *ptr);
void strip_quotes (char *arg);

/* Shell main */

int main (int argc, char **argv) {
    FILE *inputfile;
    int interactive;
    char buffer [LINELEN];
    int len;
    
    mainargc = argc;
    mainargv = argv;

    if (argc > 1) {
        inputfile = fopen(mainargv[1], "r");
        interactive = 0;
    } else {
        inputfile = stdin;
        interactive = 1;
    }

    while (1) {
        if (interactive) {
            /* prompt and get line */
            fprintf (stderr, "%% ");
        }
        
        if (fgets (buffer, LINELEN, inputfile) != buffer) {
            break;
        }

        if (!remove_comments(buffer)) {
            /* Get rid of \n at end of buffer. */
            len = strlen(buffer);
            if (buffer[len-1] == '\n') {
                buffer[len-1] = 0;
            }
        }

        /* Run it ... */
        processline (buffer);
    }

    if (!feof(inputfile)) {
        perror ("read");
    }

    return 0;       /* Also known as exit (0); */
}

int remove_comments(char *buffer) {
    for (int i = 0; buffer[i] != 0; i++) {
        if (buffer[i] == '#' && buffer[i - 1] != '$') {
            buffer[i] = 0;
            return 1;
        }
    }
    return 0;
}

void processline (char *line) {
    pid_t cpid;
    int status;
    char expanded_line[LINELEN];

    if (expand(line, expanded_line, LINELEN)) {
        int argc; // Number of arguments for executed program
        char **argpointers = arg_parse(expanded_line, &argc);

        // Only attempt to execute if input contained any arguments
        if (argc > 0) {
            if (!check_for_builtin(argpointers, argc)) {
                /* Start a new process to do the job. */
                cpid = fork();
                if (cpid < 0) {
                    /* Fork wasn't successful */
                    perror ("fork");
                    return;
                }

                /* Check for who we are! */
                if (cpid == 0) {
                    /* We are the child! */
                    execvp(argpointers[0], argpointers);
                    /* execvp returned, wasn't successful */
                    perror ("exec");
                    fclose(stdin);  // avoid a linux stdio bug
                    exit (127);
                }

                /* Have the parent wait for child to complete */
                if (wait (&status) < 0) {
                    /* Wait wasn't successful */
                    perror ("wait");
                }
            }
        }
        free(argpointers);
    }
}

char ** arg_parse (char *line, int *argcptr) {
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

    ptr = 0;
    int index = 0; // Current index of argpointers

    // Populate argpointers
    while (line[ptr] != 0 && index < argc - 1) {
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

    *argcptr = argc;
    return argpointers;
}

int check_for_quotes (const char *line, int *ptr) {
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

void strip_quotes (char *arg) {
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
