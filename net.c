/* Copyright (c) Roman Smrž 2011 <roman.smrz@seznam.cz>
 *
 * This file is part of agim, which is distributed under the term of GNU
 * General Public License version 3, see COPYING for details.
 */

/*
 * Handling of general network, implemented the net command
 */


#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netdb.h>


inline static int max(int x, int y) { return x > y ? x : y; }


/* Parses parameter into protocol family, address length and arrays of bytes of
 * address and mask. */
static void parse_param(char *param, sa_family_t *family,
		size_t *addr_len, unsigned char **addr, unsigned char **mask)
{
	// if there is a mask, divdide the parameter to address and mask part
	char *addr_str = param, *mask_str = NULL;
	for (char *c = param; *c; c++)
		if (*c == '/') {
			*c = '\0';
			mask_str = c+1;
			break;
		}


	// find address as first entry returned by getaddrinfo
	struct addrinfo *addr_info;

	int error = getaddrinfo(addr_str, NULL, NULL, &addr_info);
	if (error != 0) {
		fprintf(stderr, "net: %s: %s\n", addr_str, gai_strerror(error));
		exit(EXIT_FAILURE);
	}

	*family = addr_info->ai_addr->sa_family;
	if (*family == AF_INET) *addr_len = sizeof(struct in_addr);
	if (*family == AF_INET6) *addr_len = sizeof(struct in6_addr);

	*addr = malloc((*addr_len)*2);
	*mask = *addr + (*addr_len);

	if (*family == AF_INET)
		memcpy(*addr, &((struct sockaddr_in *) addr_info->ai_addr)->sin_addr, *addr_len);
	if (*family == AF_INET6)
		memcpy(*addr, ((struct sockaddr_in6 *) addr_info->ai_addr)->sin6_addr.s6_addr, *addr_len);

	freeaddrinfo(addr_info);

	// if mask was given, set the bytes
	if (mask_str) {
		char *mask_after;
		long mask_len = strtol(mask_str, &mask_after, 10);
		if (*mask_after) {
			fprintf(stderr, "net: \"%s\" is not valid mask size\n", mask_str);
			exit(EXIT_FAILURE);
		}
		memset(*mask, 0, *addr_len);
		for (unsigned int i = 0; i < *addr_len; i++, mask_len -= 8)
			(*mask)[i] = 0xff << max(8 - mask_len, 0);
	} else {
		memset(*mask, 0xff, *addr_len);
	}
}


bool net(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "net: requires exactly one parameter\n");
		exit(EXIT_FAILURE);
	}

	sa_family_t family;
	size_t addr_len;
	unsigned char *addr, *cur_addr, *mask;

	parse_param(argv[1], &family, &addr_len, &addr, &mask);


	struct ifaddrs *ifaddr;

	if (getifaddrs(&ifaddr) == -1) {
		perror("getifaddrs");
		exit(EXIT_FAILURE);
	}

	/* Returns true if the addresses from addr and cur_addr matches.
	 * Compares only the bits where mask is set */
	bool match_address() {
		for (size_t i = 0; i < addr_len; i++) {
			if ((addr[i] & mask[i]) != (cur_addr[i] & mask[i]))
				return false;
		}
		return true;
	}

	/* Walk through all the interfaces returned by getifaddrs and check if
	 * it matches address from parameter: */
	for (struct ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != family)
			continue;

		if (family == AF_INET)
			cur_addr = (unsigned char *) &((struct sockaddr_in *) ifa->ifa_addr)->sin_addr;
		if (family == AF_INET6)
			cur_addr = ((struct sockaddr_in6 *) ifa->ifa_addr)->sin6_addr.s6_addr;

		if (match_address()) {
			free(addr);
			freeifaddrs(ifaddr);
			return true;
		}
	}

	free(addr);
	freeifaddrs(ifaddr);
	return false;
}
