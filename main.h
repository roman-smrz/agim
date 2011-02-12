#ifndef __MAIN_H__
#define __MAIN_H__


#include <stdbool.h>


struct command;
struct program;


struct command {
	char *name;
	bool (*compile) (int, const char **, struct program *);
};


struct program {
	bool (*func) (void*);
	void *param;
	struct program *sub;
	struct program *next;
};


extern int main_argc;
extern const char **main_argv;


#endif
