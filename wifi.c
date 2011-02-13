#include <stdbool.h>
#include <iwlib.h>


bool essid(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "essid: requires exactly one parameter\n");
		exit(EXIT_FAILURE);
	}

	bool result = 0;
	char *args[2] = { argv[1], (char*) &result };
	int skfd = iw_sockets_open();

	int check_iface(int skfd, char *ifname, char *args[], int count) {
		count = count;
		wireless_config info;

		iw_get_basic_config(skfd, ifname, &info);
		*args[1] = *args[1] || !strcmp(args[0], info.essid);
		return 0;
	}
	iw_enum_devices(skfd, check_iface, args, 0);

	return result;
}
