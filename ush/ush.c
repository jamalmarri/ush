/* CS 352 -- Micro Shell!
 *
 *   Sept 21, 2000,  Phil Nelson
 *   Modified April 8, 2001
 *   Modified January 6, 2003
 *   Modified January 8, 2017
 *
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>


/* Constants */

#define LINELEN 1024

/* Prototypes */

void processline (char *line);
char ** arg_parse (char *line, int *argcptr);

/* Shell main */

int
main (void)
{
    char   buffer [LINELEN];
    int    len;

    while (1) {

    /* prompt and get line */
    fprintf (stderr, "%% ");
    if (fgets (buffer, LINELEN, stdin) != buffer)
        break;

    /* Get rid of \n at end of buffer. */
    len = strlen(buffer);
    if (buffer[len-1] == '\n')
        buffer[len-1] = 0;

    /* Run it ... */
    processline (buffer);

    }

    if (!feof(stdin))
        perror ("read");

    return 0;       /* Also known as exit (0); */
}


void processline (char *line)
{
    pid_t  cpid;
    int    status;
    int argc; // Number of arguments for executed program
    char **argpointers = arg_parse(line, &argc);

    printf("%d\n", argc);

    // Only attempt to execute if input contained any arguments
    if (argc > 0) {
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
            /* execlp returned, wasn't successful */
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
    free(argpointers);
}


char ** arg_parse (char *line, int *argcptr)
{
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
                // If quote is found, find its partner
                if (line[ptr] == '"') {
                    ptr++;
                    while (line[ptr] != '"' && line[ptr] != 0) {
                        ptr++;
                    }
                    // Print error and return if EOS is reached before another quote is found
                    if (line[ptr] == 0) {
                        fprintf(stderror, "Odd number of quotes found in input.\n");
                        return NULL;
                    }
                }
                ptr++;
            }
        }
    }

    // Reserve extra space for NULL pointer at the end
    argc++;

    // Attempt to malloc for argpointers array
    char ** argpointers = malloc(sizeof(char *) * argc);
    if (argpointers == NULL) {
        fprintf(stderr, "Malloc of argument pointer array failed.\n");
        exit(1);
    }

    ptr = 0;
    int i = 0; // Current index of argpointers

    // Populate argpointers
    while (line[ptr] != 0) {
        if (line[ptr] == ' ') {
            // Skip leading/trailing spaces
            ptr++;
        } else {
            // Argument found
            argpointers[i] = &line[ptr];
            i++;
            // Find end of argument and replace trailing space with 0.
            while (line[ptr] != ' ' && line[ptr] != 0) {
                ptr++;
            }
            line[ptr] = 0;
            ptr++;
        }
    }
    // Set last element to NULL for execvp
    argpointers[argc - 1] = NULL;

    *argcptr = argc;
    return argpointers;
}
