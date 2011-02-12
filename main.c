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


struct command commands[] = {
	{ NULL, NULL }
};



const char **parse_params(const char *data, int *pos, int length) {
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
		if (data[*pos] == '\n' || data[*pos] == '{')
			break;

		add_param(buf+buf_i);

		int quot = -1;
		for (; *pos < length; (*pos)++) {
			char c = data[*pos];
			if (c == quot)
				quot = -1;
			else if (c == '\'' || c == '"')
				quot = c;
			else if (quot == -1 && (isspace(c) || c == '{'))
				break;
			else add_char(c);
		}
		while (isspace(data[*pos]) && data[*pos] != '\n') (*pos)++;
		add_char(0);
	}
	add_param(NULL);
	return list;
}


struct program *compile_cmd(const char **params) {
	static struct program *buf = NULL;
	static int buf_len = 0, buf_i = 0;

	int count = 0;
	while (params[count]) count++;

	//fprintf(stderr, "Processing command: %s\n", params[0]);
	for (struct command *cmd = commands; cmd->name; cmd++) {
		if (strcmp(params[0], cmd->name) != 0)
			continue;

		if (buf_i >= buf_len) {
			// Command nodes are preserved for the whole time this
			// program is running, so we do not need to keep
			// references to the used ones.

			buf_i = 0;
			buf_len = buf_len * 2 + 1;
			buf = malloc(sizeof(*buf)*buf_len);
		}

		buf[buf_i].func = 0;
		buf[buf_i].param = 0;
		buf[buf_i].sub = 0;
		buf[buf_i].next = 0;

		//fprintf(stderr, "Compiling command: %s\n", params[0]);
		if (!cmd->compile(count, params, buf+buf_i)) {
			fprintf(stderr, "Compiling failed: %s\n", params[0]);
			return NULL;
		}
		//fprintf(stderr, "OK\n");
		return buf+(buf_i++);
	}
	fprintf(stderr, "Unknown command: %s\n", params[0]);
	return NULL;
}



struct program *parse_config(const char *data, int length) {
	int pos = 0;

	struct program *parse_config_(const char *data, int length, int *pos) {
		struct program *result = NULL, *current = NULL;
		while (1) {
			while (*pos < length && isspace(data[*pos])) (*pos)++;
			if (*pos >= length) break;

			if (data[*pos] == '{') {
				(*pos)++;
				current->sub = parse_config_(data, length, pos);
				continue;
			}

			if (data[*pos] == '}') {
				(*pos)++;
				break;
			}

			const char **params = parse_params(data, pos, length);
			if (!params) return NULL;

			struct program *new = compile_cmd(params);
			if (!new) {
				fprintf(stderr, "Compilation failed.\n");
				return NULL;
			}

			if (current) current->next = new;
			current = new;
			if (!result) result = new;
		}
		return result;
	}
	return parse_config_(data, length, &pos);
}


void run(struct program *program) {
	if (program->func(program->param) && program->sub)
		run(program->sub);
	if (program->next)
		run(program->next);
}


int main_argc;
const char **main_argv;

int main(int argc, char const* argv[])
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

	struct program *program = parse_config(conf_data, st.st_size);
	if (program) run(program);


	return 0;
}
