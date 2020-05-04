#pragma once

int expand(char *orig, char *new, int newsize);

int check_for_builtin(char **argpointers, int argc);

/* Global Variables */

int mainargc;
char **mainargv;
int shift_offset;