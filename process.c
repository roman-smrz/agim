/* Copyright (c) Roman Smrž 2011 <roman.smrz@seznam.cz>
 *
 * This file is part of agim, which is distributed under the term of GNU
 * General Public License version 3, see COPYING for details.
 */

/*
 * Here is implemented the work with processes, namely the send and copy
 * commands along with associated routines. That includes ensuring that the
 * message from standard input is properly passed to each of the children,
 * should there be any.
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

#include "main.h"


static struct {
	pid_t pid;
	int pipe;
} *children = NULL;
static int chld_len = 0, chld_i = 0;


/* Forks a child process and creates a pipe from parent (saved it the children
 * structure) to the child's standard input. Takes care of freeing information
 * about children in the newly created process. */
static pid_t add_child()
{
	int pipefd[2];
	if (pipe(pipefd) < 0) {
		perror("add_child: pipe");
		exit(EXIT_FAILURE);
	}

	pid_t pid = fork();
	if (pid < 0) {
		perror("add_child: fork");
		exit(EXIT_FAILURE);
	}

	if (pid > 0) {
		if (chld_i >= chld_len) {
			chld_len = chld_len * 2 + 1;
			children = realloc(children, sizeof(*children)*chld_len);
		}
		close(pipefd[0]);
		children[chld_i].pid = pid;
		children[chld_i].pipe = pipefd[1];
		chld_i++;
	} else {
		for (int i = 0; i < chld_i; i++)
			close(children[i].pipe);
		free(children);
		children = NULL;
		chld_len = chld_i = 0;

		dup2(pipefd[0], STDIN_FILENO);
		close(pipefd[0]);
		close(pipefd[1]);
	}

	return pid;
}


/* Copy standard input of calling process to standard input of all the child
 * processes using prepared pipes. Wait for their termination and terminate
 * this process with the first non-zero status of a child process or with 0 if
 * no error status is encountered. */
void serve_children()
{
	char buf[4096];
	size_t count = 0;
	while ((count = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
		for (int i = 0; i < chld_i; i++) {
			size_t written, diff;
			for (written = 0; written < count; written += diff) {
				diff = write(children[i].pipe, buf+written, count-written);
				if (diff == -1U) {
					perror("serve_children: write");
					exit(EXIT_FAILURE);
				}
			}
		}
	}

	if (count == -1U) {
		perror("serve_children: read");
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < chld_i; i++) {
		close(children[i].pipe);

		int status;
		waitpid(children[i].pid, &status, 0);
		if (WEXITSTATUS(status))
			exit(WEXITSTATUS(status));
	}

	exit(EXIT_SUCCESS);
}


/* Just concatenate the argument lists of this command and main program and
 * call exec. */
bool agim_send(int argc, char **argv)
{
	char **exec_argv =
		malloc((argc+main_argc) * sizeof(*exec_argv));

	int i = 0;
	for (int j = 1; j < argc; j++, i++)
		exec_argv[i] = argv[j];
	for (int j = 0; j < main_argc; j++, i++)
		exec_argv[i] = main_argv[j];
	exec_argv[i] = NULL;

	// if any children were created, we must fork again and keep the parent
	// process to copy the input for everyone:
	if (children && add_child())
		serve_children();

	execvp(exec_argv[0], exec_argv);
	perror("send: exec failed");
	exit(EXIT_FAILURE);
}


/* Fork the process and ensures that both parent and child get copy of the
 * standard input. Returns true for child, false for parent. */
bool copy(int argc, char **argv)
{
	argv = argv;
	if (argc != 1) {
		fprintf(stderr, "copy: does not support any arguments\n");
		exit(EXIT_FAILURE);
	}

	return !add_child();
}
