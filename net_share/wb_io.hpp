#pragma once
#include <windows.h>
#include "base_component.h"
#include <ioapiset.h>

class wb_io
{
protected:
	HANDLE				_cp=nullptr;
	wb_base_thread_pool _tp;
public:
	wb_io() {};
	virtual ~wb_io() {
		stop();
	};
	bool relate(HANDLE h, void* ComKey) {
		if (!_cp)
			return false;
		return ::CreateIoCompletionPort(h, _cp, reinterpret_cast<ULONG_PTR>(ComKey), 0) == _cp;
	}
	bool post(ULONG_PTR ComKey, DWORD Byte,LPOVERLAPPED pol) {
		return ::PostQueuedCompletionStatus(_cp, Byte, ComKey, pol) == TRUE;
	}
	template<class Fn,class...argvs>
	bool start(int tds, Fn f, argvs...ags) {
		stop();
		SYSTEM_INFO si = {};
		GetSystemInfo(&si);
		auto ithreads = si.dwNumberOfProcessors * 2 + 2;//默认启动线程数，release下使用默认cpu*2+2
		_cp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
		if (!_cp) {

			return false;
		}
		auto do_work = [=]() {

			while (1)
			{
				DWORD numberOfBytes = 0;
				ULONG_PTR ComKey = 0;
				LPOVERLAPPED ol = nullptr;
				try {
					//printf("线程执行中...\n");
					if (GetQueuedCompletionStatus(this->_cp, &numberOfBytes, &ComKey, &ol, INFINITE))
					{
						//printf("GetQueuedCompletionStatus1...\n");
						if (!f(numberOfBytes, ComKey, ol, ags...))
							break;
					}
					else
					{
						//auto err = GetLastError();
						//WLOG("GetQueuedCompletionStatus FALSE %d %d %p\n", err, numberOfBytes, ol);
						if (ol || numberOfBytes>0) {
							if (!f(numberOfBytes, ComKey, ol, ags...))
								break;
						}
						else
						{
							RLOG("IO 关闭，线程退出\n");
							break;
						}
					}
				}
				catch (...)
				{
					break;
				}
			}
			//线程退出
		};
		//_tp.start<decltype(do_work), false ,HANDLE>(3, do_work,_cp);
		_tp.start<decltype(do_work), false>(tds, do_work);
		return true;
	}
	void stop() {
		if (_cp)
			CloseHandle(_cp);
		_cp = nullptr;
		_tp.stop();
	}
};
