
#include <Windows.h>
#include "base_component.h"


bool wb_base_memory_pool::start(int block_size, int block_num) {

	stop();
	SYSTEM_INFO sysi = { 0 };
	::GetSystemInfo(&sysi);
	//m_PageSize = sysi.dwPageSize;//页面大小 系统固定的
	auto pool_size = block_size * block_num;// ((PoolSize + m_PageSize - 1) / m_PageSize)* m_PageSize;
	pool_size = ((pool_size + sysi.dwPageSize - 1) / sysi.dwPageSize) * sysi.dwPageSize;
	_mem_h = ::GlobalAlloc(GMEM_FIXED, pool_size);//申请
	if (!_mem_h)return false;
	_mem_p = ::GlobalLock(_mem_h);
	if (!_mem_p)
	{
		::GlobalUnlock(_mem_h);
		_mem_h = NULL;
		return false;
	}
	auto n = pool_size / block_size;
	_end_p = (BYTE*)_mem_h + ((block_num - 1) * block_size);
	for (auto i = 0; i < n; i++)
	{
		__int64 offset = i * (__int64)block_size;
		_lst.push_back((BYTE*)_mem_p + offset);
	}
#ifdef _DEBUG
	m_map.clear();
#endif // _DEBUG


	return true;
}


void wb_base_memory_pool::stop() {
	if (_mem_h)
	{
		if(_mem_p)::GlobalFree(_mem_p);
		_mem_p = nullptr;
		GlobalUnlock(_mem_h);
		_mem_h = NULL;
	}
	_lst.clear();
}

wb_auto_tm::wb_auto_tm(const char* msg)
	:_msg(msg)
{
	SYSTEMTIME st;
	GetSystemTime(&st);
	_tm = st.wMilliseconds;
}
wb_auto_tm::~wb_auto_tm()
{
	SYSTEMTIME st;
	GetSystemTime(&st);
	printf("%s耗时:%d\n", _msg.c_str(), st.wMilliseconds-_tm);
}

