/*
 * CSCI 347 Microshell
 * Jamal Marri
 * Spring Quarter 2020
 */

#pragma once

/* Global Constants */
#define WAIT 1
#define NOWAIT 0
#define EXPAND 2
#define NOEXPAND 0

/* Global Prototypes */
int processline(char *line, int outfd, int flags);
int expand(char *orig, char *new, int newsize);
int check_for_builtin(char **argpointers, int argc);
void strmode(mode_t mode, char *p);

/* Global Variables */
int mainargc;
char **mainargv;
int shift_offset;
int last_exit;
int sigint_caught;
