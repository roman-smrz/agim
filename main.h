/* Copyright (c) Roman Smrž 2011 <roman.smrz@seznam.cz>
 *
 * This file is part of agim, which is distributed under the term of GNU
 * General Public License version 3, see COPYING for details.
 */

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
