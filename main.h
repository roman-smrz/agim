#ifndef __MAIN_H__
#define __MAIN_H__


#include <stdbool.h>


struct command {
	char *name;
	bool (*run) (int, char **);
};


extern int main_argc;
extern char **main_argv;

extern bool *results;
extern int results_count;


#endif
