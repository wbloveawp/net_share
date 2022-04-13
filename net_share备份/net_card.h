#pragma once
#include <ioapiset.h>
#include "net_base.h"
class wb_net_card_file
{
	HANDLE	_hf;

	WCHAR	_name[MAX_PATH] = {};
	DWORD	_name_len =0 ;
public:
	wb_net_card_file();
	~wb_net_card_file() {
		close();
	};
	bool Read(char* buffer, int len, LPOVERLAPPED overlopped) {
		return (ReadFile(_hf, buffer, len, nullptr, overlopped) == TRUE || GetLastError() == ERROR_IO_PENDING);
	}
	bool Write(const char* buffer, int len, LPOVERLAPPED overlopped) {
		return WriteFile(_hf, buffer, len, nullptr, overlopped) == TRUE || GetLastError() == ERROR_IO_PENDING ;
	}
	explicit operator HANDLE() {
		return _hf;
	}
	VOID close();
	bool setFilter(PNET_IP_FILTER pFilter, int len);
	bool openDriver(const char* driver_name);
	bool openCard(LPWSTR card_name, const char* driver_name = nullptr);
};

extern int get_local_ip();