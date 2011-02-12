#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "main.h"


/*
 * Just concatenate the argument lists of this command and main program and
 * call exec.
 */
bool send(int argc, char **argv) {
	char **exec_argv =
		malloc((argc+main_argc-1) * sizeof(*exec_argv));

	int i = 0;
	for (int j = 1; j < argc; j++, i++)
		exec_argv[i] = argv[j];
	for (int j = 1; j < main_argc; j++, i++)
		exec_argv[i] = main_argv[j];
	exec_argv[i] = NULL;

	execvp(exec_argv[0], exec_argv);
	perror("send: exec failed");
	exit(EXIT_FAILURE);
}
