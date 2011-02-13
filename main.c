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


bool send (int, const char **);
bool net (int, const char **);
bool essid (int, const char **);

struct command commands[] = {
	{ "send", send },
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

	while (isspace(data[*pos])) (*pos)++;
	while (1) {
		if (data[*pos] == '\n' || data[*pos] == '{' || data[*pos] == '#')
			break;

		add_param(buf+buf_i);

		int quot = -1;
		for (; *pos < length; (*pos)++) {
			char c = data[*pos];
			if (c == quot)
				quot = -1;
			else if (c == '\'' || c == '"')
				quot = c;
			else if (quot == -1 && (isspace(c) || c == '{' || c == '#'))
				break;
			else add_char(c);
		}
		while (isspace(data[*pos]) && data[*pos] != '\n') (*pos)++;
		add_char(0);
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

	if ((fd = open("conf", O_RDONLY)) == -1) {
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

	main_argc = argc;
	main_argv = argv;

	run_script(conf_data, st.st_size);
	return 0;
}
