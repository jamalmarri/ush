#pragma once

int expand(char *orig, char *new, int newsize);

int check_for_builtin(char *argpointer, int argc);
void run_builtin(int builtin);