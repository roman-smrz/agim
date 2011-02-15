/* Copyright (c) Roman Smrž 2011 <roman.smrz@seznam.cz>
 *
 * This file is part of agim, which is distributed under the term of GNU
 * General Public License version 3, see COPYING for details.
 */

/*
 * Here is implemented the core of the interpreter (configuration file is used
 * basically as a script) – parsing file into commands and arguments and
 * running them. The main work is done in function parse_params and its
 * subroutines. Results from there are used in run_script to parse the whole
 * configuration file (script) and execute it.
 */


#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <ctype.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "main.h"


void serve_children();


/* List of available commands */

bool agim_send (int, char **);
bool copy (int, char **);
bool net (int, char **);

// libiw from wireless-tools used for essid command is available only on Linux
#ifdef __linux__
bool essid (int, char **);
#endif

bool agim_true(int, char **);
bool agim_false(int, char **);
bool any(int, char **);
bool all(int, char **);


struct command commands[] = {
	{ "send", agim_send },
	{ "copy", copy },
	{ "net", net },
#ifdef __linux__
	{ "essid", essid },
#endif

	{ "true", agim_true },
	{ "false", agim_false },
	{ "any", any },
	{ "all", all },

	{ NULL, NULL }
};



/* Actual parsing of parameters. Main work is done in subroutines defined in
 * the body of this function, whose results are put together at the end in a
 * simple loop */
static char **parse_params(const char *data, int *pos, int length)
{
	static char **list = NULL;
	static int list_len = 0, list_i = 0;

	static char *buf = NULL;
	static int buf_len = 0, buf_i;

	list_i = 0; buf_i = 0;

	/* Adds a parameter to the current buffer (enlarging it if necessary).
	 * If the buffer is moved as a result of realloc, updates pointers in
	 * list of parsed parameters */
	inline void add_char(char c)
	{
		if (buf_i >= buf_len) {
			char *old_buf = buf;
			buf_len = buf_len * 2 + 1;
			buf = realloc(buf, buf_len);
			if (old_buf != buf)
				for (int i = 0; i < list_i; i++)
					list[i] += buf - old_buf;
		}
		buf[buf_i++] = c;
	}

	/* Adds item to the list of parsed parameters. Would be the current end
	 * of buffer or NULL to mark the end */
	inline void add_param(char *param)
	{
		if (list_i >= list_len) {
			list_len = list_len * 2 + 1;
			list = realloc(list, sizeof(*list)*list_len);
		}
		list[list_i++] = param;
	}

	/* Parses the name of variable which is to be expanded and the name of
	 * which starts after the $ sign at the current position. Takes care of
	 * possible { and } delimiters, if those are not present, name is taken
	 * to be the longest sequence of alphanumeric or underscore characters.
	 * The value of variable is placed in the var_value. Currently, only
	 * variable names of up to 255 characters are supported. */
	const char *var_value = "";
	void read_var()
	{
		char name[256];
		char type = 0;
		int i = 0;
		for ((*pos)++; i < 255 && *pos < length; (*pos)++) {
			char c = data[*pos];
			if (!type) {
				if (c == '{') {
					type = '{';
					continue;
				}
				if (isalnum(c) || c == '_')
					type = ' ';
			}

			// Nothing that could be a variable name was found, so
			// just return the $ itself:
			if (!type) {
				var_value = "$";
				return;
			}

			if (isalnum(c) || c == '_')
				name[i++] = c;
			else {
				if (!(type == '{' && c == '}'))
					(*pos)--;
				break;
			}
		}
		name[i] = 0;

		var_value = getenv(name);
		if (!var_value) var_value = "";
	}

	int quot = 0;
	bool last_space = false;

	/* This function returns the next character to be included in
	 * parameters, it interprets special characters, for each sequence of
	 * non-escaped white-space characters returns exactly one -2 and
	 * returns -1 when processing of parameters for current command should
	 * stop. */
	int next_char()
	{
		char c;
		if (*var_value) {
			c = *(var_value++);
			if (quot) return c;
		} else {
			if (++(*pos) >= length) return -1;

			c = data[*pos];
			last_space = last_space && isspace(c);

			if (c == '\'' || c == '"') {
				if (quot == 0) quot = c;
				else if (quot == c) quot = 0;
				else return c;
				return next_char();
			}

			if (!quot || quot == '"') {
				if (c == '\\' && (*pos) < length-1)
					return data[++(*pos)];
				if (c == '$') {
					read_var();
					return next_char();
				}
			}

			if (quot) return c;

			if (c == '\n' || c == '{' || c == '}' || c == '#')
				return -1;
		}

		if (isspace(c)) {
			if (last_space) return next_char();
			else {
				last_space = true;
				return -2;
			}
		}

		return c;
	}

	/* next_char() advances the position prior to reading character, so we
	 * have to step a one back first: */
	(*pos)--;

	char c = ' ';
	while (isspace(c)) c = next_char();

	while (c != -1) {
		add_param(buf+buf_i);
		for (; c >= 0; c = next_char())
			add_char(c);
		add_char(0);
		if (c != -1) c = next_char();
	}
	add_param(NULL);
	return list;
}


/* History of results since last beginning or end of a command block; used by
 * some commands like 'any' and 'all'. */

bool *results = NULL;
int results_count = 0, results_size = 0;

static bool add_result(bool result)
{
	if (results_count >= results_size) {
		results_size = results_size * 2 + 1;
		results = realloc(results, results_size * sizeof(*results));
	}
	return results[results_count++] = result;
}


/* Runs command given by the parameters, first parameter is the command name. */
static bool run_cmd(char **params)
{
	int count = 0;
	while (params[count]) count++;

	for (struct command *cmd = commands; cmd->name; cmd++) {
		if (strcmp(params[0], cmd->name) != 0)
			continue;

		return add_result(cmd->run(count, params));
	}

	fprintf(stderr, "Unknown command: %s\n", params[0]);
	exit(EXIT_FAILURE);
}


/* Runs the script, uses parse_params to parse parameters, divides the program
 * into block and decides which parts to execute, depending on results of
 * individual commands */
static void run_script(const char *data, int length)
{
	int pos = 0;

	void run_script_(bool run) {
		bool run_subcmds = run;
		while (1) {
			while (pos < length && isspace(data[pos])) pos++;
			if (pos >= length) break;

			if (data[pos] == '#') {
				while (pos < length && data[pos] != '\n') pos++;
				continue;
			}

			if (data[pos] == '{') {
				pos++;
				results_count = 0;
				run_script_(run_subcmds);
				continue;
			}

			if (data[pos] == '}') {
				pos++;
				results_count = 0;
				break;
			}

			char **params = parse_params(data, &pos, length);
			if (!params) return;

			if (run) run_subcmds = run_cmd(params);
		}
	}
	run_script_(true);
}


/* The rest of command-line arguments; to be used in the send command */

int main_argc;
char **main_argv;

int main(int argc, char **argv)
{
	int fd;
	struct stat st;
	char *conf_file, *conf_data;

	if (argc > 1 && !strncmp(argv[1], "-C", 2)) {
		if (strlen(argv[1]) == 2) {
			if (argc < 3) {
				fprintf(stderr, "Parameter -C requires name of "
					"configuration file as an argument\n"
					"usage: agim [-C config-file] [MTA arguments ...]\n"
				       );
				exit(1);
			}
			conf_file = argv[2];
			main_argc = argc-3;
			main_argv = argv+3;
		} else {
			conf_file = argv[1]+2;
			main_argc = argc-2;
			main_argv = argv+2;
		}
	} else {
		char *home = getenv("HOME");
		if (!home) {
			fprintf(stderr, "Environment variable HOME not set\n");
			exit(1);
		}

		conf_file = malloc(strlen(home) + 9);
		strcpy(conf_file, home);
		strcat(conf_file, "/.agimrc");

		main_argc = argc-1;
		main_argv = argv+1;
	}

	if ((fd = open(conf_file, O_RDONLY)) == -1) {
		char error[128];
		snprintf(error, sizeof(error), "Reading config file %s", conf_file);
		perror(error);
		exit(1);
	}

	if (fstat(fd, &st) == -1) {
		perror("Stat of configuration file");
		exit(1);
	}


	conf_data = mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (conf_data == MAP_FAILED) {
		perror("Mapping config file into memory");
		exit(1);
	}

	run_script(conf_data, st.st_size);
	serve_children();
	return 0;
}
