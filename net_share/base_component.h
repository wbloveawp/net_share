#pragma once
#include <thread>
#include <atomic>
#include <list>
#include <mutex>
#include <vector>
#include <basetsd.h>
#include <string>
#include <assert.h>
#include <map>
#include <type_traits>
#include <cstdint>
class wb_lock
{
	CRITICAL_SECTION _cs;
public:
	wb_lock() { InitializeCriticalSection(&_cs); }
	~wb_lock() { DeleteCriticalSection(&_cs); }

	void lock() { EnterCriticalSection(&_cs); }
	void unlock() { LeaveCriticalSection(&_cs); };
};

class wb_AutoLock
{
	wb_lock& FLock;
public:
	wb_AutoLock(wb_lock& Lock)
		:FLock(Lock)
	{
		FLock.lock();
	}
	~wb_AutoLock()
	{
		FLock.unlock();
	}
};

#define AUTO_LOCK(lock) wb_AutoLock Lock(lock)
/*
* �����̳߳�
*/

class wb_base_thread_pool {

	using thread_vec = std::vector<std::thread>;
	thread_vec _tv;
public:
	/*
	* is_cite:true,cls���������ô��ݣ�����ֵ����
	*/
	template<class Fn , bool is_cite, class...cls>
	void start(int threads, Fn f, cls...arvs)
	{
		stop();
		for (int i = 0; i < threads; i++)
		{
			if (is_cite)
				_tv.push_back(std::thread(
					[&]() {
						f(arvs...);
					}));
			else
				_tv.push_back(std::thread(
				[=]() {
					f(arvs...);
				}));
		}
	}

	void stop() {
		for (auto t=_tv.begin();t!=_tv.end();t++)
		{
			(*t).join();
		}
		_tv.clear();
	}
};

/*
* �����ڴ��,�̰߳�ȫ
*/

class wb_base_memory_pool {

	std::list<void*>	_lst;
	wb_lock				_lock;
	void*				_mem_p=nullptr;//�ڴ��׵�ַ
	void*				_end_p = nullptr;//�ڴ�β��ַ
	HANDLE				_mem_h=NULL;//�ڴ����
#ifdef _DEBUG
	using mem_map = std::map<ULONG_PTR, void*>; //�����Դ�ظ��ͷŵ�����
	mem_map m_map;
#endif
public:
	//wb_base_memory_pool();// = delete;
	~wb_base_memory_pool() {
		stop();
	};
	bool start(int block_size, int block_num);
	void stop();

	void* get_mem() {
		void* p = nullptr;
		_lock.lock();
		if (!_lst.empty())
		{
			p = *(_lst.begin());
#ifdef _DEBUG
			assert(m_map.find((ULONG_PTR)p) == m_map.end());
			m_map[(ULONG_PTR)p] = p;
#endif
			_lst.pop_front();
		}
		else
		{
			assert(0);
			printf("��Դ����\n");
		}
		_lock.unlock();
		return p;
	}

	void recover_mem(void* p) {

		assert(p >= _mem_p && p<=_end_p);
		_lock.lock();
#ifdef _DEBUG
		assert(m_map.find((ULONG_PTR)p) != m_map.end());
		m_map.erase((ULONG_PTR)p);
#endif
		_lst.push_front(p);
		_lock.unlock();

	}

	int size() {
		_lock.lock();
		auto size = _lst.size();
		_lock.unlock();
		return size;
	}
};

//�Զ���ʱ���
class wb_auto_tm
{
	WORD _tm;
	std::string _msg;
public:
	wb_auto_tm(const char*);
	~wb_auto_tm();
};

//���ڴ�ص�����ָ��
template <class Ty>
class wb_share_ptr
{
	using elment_type = Ty;

	elment_type* _ptr = nullptr;
	wb_base_memory_pool* _mp=nullptr;
public:
	wb_share_ptr() {};
	~wb_share_ptr() {
		if (_ptr && _ptr->DelRef()==0)
			elment_type::operator delete (_ptr, *_mp);
	
		_ptr = nullptr;
		_mp = nullptr;
	}
	explicit wb_share_ptr(Ty* pt, wb_base_memory_pool* mp) {
		if(pt)
			pt->AddRef();
		_mp = mp;
		_ptr = pt;
	}

	wb_share_ptr(const wb_share_ptr& sp) {
		if(sp._ptr)
			sp._ptr->AddRef();
		_mp = sp._mp;
		_ptr = sp._ptr;
	}
	
	wb_share_ptr& operator = (const wb_share_ptr& p) {
		if (this == &p)return *this;
		if(p._ptr)
			p._ptr->AddRef();
		if (_ptr && _ptr->DelRef() == 0)
			elment_type::operator delete (_ptr, *_mp);
		_mp = p._mp;
		_ptr = p._ptr;
		return *this;
	}

	operator Ty* () {
		return _ptr;
	}

	Ty* operator->() {
		return _ptr;
	}

	Ty* get() {
		return _ptr;
	};
};