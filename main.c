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

bool send (int, const char **);
bool copy (int, const char **);
bool net (int, const char **);
bool essid (int, const char **);

struct command commands[] = {
	{ "send", send },
	{ "copy", copy },
	{ "net", net },
	{ "essid", essid },
	{ NULL, NULL }
};



static const char **parse_params(const char *data, int *pos, int length) {
	static const char **list = NULL;
	static int list_len = 0, list_i = 0;

	static char *buf = NULL;
	static int buf_len = 0, buf_i;

	list_i = 0; buf_i = 0;

	inline void add_char(char c) {
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

	inline void add_param(char *param) {
		if (list_i >= list_len) {
			list_len = list_len * 2 + 1;
			list = realloc(list, sizeof(*list)*list_len);
		}
		list[list_i++] = param;
	}

	int quot = 0;
	bool last_space = false;
	(*pos)--;

	const char *var_value = "";

	void read_var() {
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

	/*
	 * This function returns the next character to be included in
	 * parameters, it interprets special characters, for each sequence of
	 * white-space characters returns exactly one space and returns 0 when
	 * processing of parameters for current command should stop.
	 */
	char next_char() {
		char c;
		if (*var_value) {
			c = *(var_value++);
		} else {
			if (++(*pos) >= length) return 0;

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
				return 0;
		}

		if (isspace(c)) {
			if (last_space) return next_char();
			else {
				last_space = true;
				return ' ';
			}
		}

		return c;
	}

	char c = ' ';
	while (isspace(c)) c = next_char();

	while (c) {
		add_param(buf+buf_i);
		for (; c && c != ' '; c = next_char())
			add_char(c);
		add_char(0);
		if (c) c = next_char();
	}
	add_param(NULL);
	return list;
}


static bool run_cmd(const char **params) {
	int count = 0;
	while (params[count]) count++;

	//fprintf(stderr, "Processing command: %s\n", params[0]);
	for (struct command *cmd = commands; cmd->name; cmd++) {
		if (strcmp(params[0], cmd->name) != 0)
			continue;

		return cmd->run(count, params);
	}

	fprintf(stderr, "Unknown command: %s\n", params[0]);
	exit(EXIT_FAILURE);
}



static void run_script(const char *data, int length) {
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
				run_script_(run_subcmds);
				continue;
			}

			if (data[pos] == '}') {
				pos++;
				break;
			}

			const char **params = parse_params(data, &pos, length);
			if (!params) return;

			if (run) run_subcmds = run_cmd(params);
		}
	}
	run_script_(true);
}



int main_argc;
char **main_argv;

int main(int argc, char **argv)
{
	int fd;
	struct stat st;
	const char *conf_data;

	if (argc < 2) {
		fprintf(stderr, "Script name not given\n");
		exit(1);
	}

	if ((fd = open(argv[1], O_RDONLY)) == -1) {
		perror("Reading configuration file");
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

	main_argc = argc-2;
	main_argv = argv+2;

	run_script(conf_data, st.st_size);
	serve_children();
	return 0;
}
