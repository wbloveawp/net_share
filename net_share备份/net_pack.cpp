
#include "net_pack.h"
#include <winsock2.h>

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

/*
wb_eth_pack::wb_eth_pack(){

	_pack = (ETH_PACK*)_buffer;
	//_date_len = ntohs(p_pack->ip_h.sTotalLenOfPack) - g_ip_head_len;
}
*/