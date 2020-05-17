#pragma once

int expand(char *orig, char *new, int newsize);

int check_for_builtin(char **argpointers, int argc);

void strmode(mode_t mode, char *p);

/* Global Variables */

int mainargc;
char **mainargv;
int shift_offset;
int last_exit;