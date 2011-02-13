#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netdb.h>


static void parse_param(const char *param, sa_family_t *family,
		size_t *addr_len, unsigned char **addr)
{
	struct addrinfo *addr_info;

	int error = getaddrinfo(param, NULL, NULL, &addr_info);
	if (error != 0) {
		fprintf(stderr, "net: getaddrinfo: %s\n", gai_strerror(error));
		exit(EXIT_FAILURE);
	}

	*family = addr_info->ai_addr->sa_family;
	if (*family == AF_INET) *addr_len = sizeof(struct in_addr);
	if (*family == AF_INET6) *addr_len = sizeof(struct in6_addr);

	*addr = malloc(*addr_len);

	if (*family == AF_INET)
		memcpy(*addr, &((struct sockaddr_in *) addr_info->ai_addr)->sin_addr, *addr_len);
	if (*family == AF_INET6)
		memcpy(*addr, ((struct sockaddr_in6 *) addr_info->ai_addr)->sin6_addr.s6_addr, *addr_len);

	freeaddrinfo(addr_info);
}


bool net(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "net: requires exactly one parameter\n");
		exit(EXIT_FAILURE);
	}

	sa_family_t family;
	size_t addr_len;
	unsigned char *addr, *cur_addr;

	parse_param(argv[1], &family, &addr_len, &addr);


	struct ifaddrs *ifaddr;

	if (getifaddrs(&ifaddr) == -1) {
		perror("getifaddrs");
		exit(EXIT_FAILURE);
	}

	bool match_address() {
		for (size_t i = 0; i < addr_len; i++) {
			if (addr[i] != cur_addr[i])
				return false;
		}
		return true;
	}

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
