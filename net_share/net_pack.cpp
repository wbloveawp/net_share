
#include "net_pack.h"
#include <winsock2.h>

/*
unsigned short ip_check_sum(unsigned short* a, int len)
{
	unsigned int sum = 0;
	while (len > 1) {
		sum += *a++;
		len -= 2;
	}
	if (len) {
		sum += *(unsigned char*)a;
	}
	while (sum >> 16) {
		sum = (sum >> 16) + (sum & 0xffff);
	}
	return (unsigned short)(~sum);
}
*/