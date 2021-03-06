/* Copyright (c) Roman Smrž 2011 <roman.smrz@seznam.cz>
 *
 * This file is part of agim, which is distributed under the term of GNU
 * General Public License version 3, see COPYING for details.
 */

/*
 * Some simple logical combinators.
 */

#include "main.h"


bool agim_true(int argc, char **argv)
{
	(void) argc;
	(void) argv;
	return true;
}


bool agim_false(int argc, char **argv)
{
	(void) argc;
	(void) argv;
	return false;
}


bool any(int argc, char **argv)
{
	(void) argc;
	(void) argv;

	bool any = false;
	for (int i = 0; i < results_count; i++)
		any = any || results[i];
	return any;
}


bool all(int argc, char **argv)
{
	(void) argc;
	(void) argv;

	bool all = true;
	for (int i = 0; i < results_count; i++)
		all = all && results[i];
	return all;
}
