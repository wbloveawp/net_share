#include <Windows.h>
#include "net_card.h"
#include "net_base.h"

wb_net_card_file::wb_net_card_file() {
	_hf = INVALID_HANDLE_VALUE;
}

bool wb_net_card_file::openDriver(const char* driver_name) {
	_hf = CreateFileA(driver_name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
		FILE_FLAG_OVERLAPPED, nullptr);
	return _hf != INVALID_HANDLE_VALUE;
}
bool wb_net_card_file::openCard(LPWSTR card_name, const char* driver_name ) {
	if (_hf == INVALID_HANDLE_VALUE)
	{
		if (driver_name == nullptr)
			return false;
		if (!openDriver(driver_name))
			return false;
	}
	DWORD BytesReturned=0;
	int len = wcslen(card_name)*2;
	if (!DeviceIoControl(
		_hf,
		OUT_IOCTL_NDISPROT_OPEN_DEVICE,
		(VOID*)card_name,
		len,
		nullptr,
		0,
		&BytesReturned,
		nullptr))
	{
		return false;
	}
	else
	{
		memset(_name, 0, sizeof(_name) );
		_name_len = len;
		memcpy(_name, card_name, len);
		return true;
	}

}

VOID wb_net_card_file::close() {
	if (_hf == INVALID_HANDLE_VALUE)
		return;
	/*
	DWORD BytesReturned;
	DeviceIoControl(
		_hf,
		OUT_IOCTL_NDISPROT_CLOSE_DEVICE,
		(VOID*)_name,
		_name_len,
		nullptr,
		0,
		&BytesReturned,
		nullptr);
	*/
	CloseHandle(_hf);
	_hf = INVALID_HANDLE_VALUE;
}

bool wb_net_card_file::setFilter(PNET_IP_FILTER pFilter, int len)
{
	if (_hf == INVALID_HANDLE_VALUE)
		return FALSE;

	DWORD BytesReturned;
	if (!DeviceIoControl(
		_hf,
		OUT_IOCTL_NDISPROT_SET_HOST_SECCION,
		pFilter,
		len,
		NULL,
		0,
		&BytesReturned,
		NULL))
	{
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}


int get_local_ip() {
	char hostname[128];
	int ret = gethostname(hostname, sizeof(hostname));
	if (ret == -1) {
		return -1;
	}
	struct hostent* hent;
	hent = gethostbyname(hostname);
	if (NULL == hent) {
		return -1;
	}
	return ntohl(((struct in_addr*)hent->h_addr)->s_addr);
}