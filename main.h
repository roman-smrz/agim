#ifndef __MAIN_H__
#define __MAIN_H__


#include <stdbool.h>


struct command {
	char *name;
	bool (*run) (int, const char **);
};


extern int main_argc;
extern char **main_argv;


#endif
