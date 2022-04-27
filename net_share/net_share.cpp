// net_share.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "evn_init.h"
#include "mainer.h"

#include <algorithm>
using namespace std;


template <class Ty>
class my_share_ptr
{
	using elment_type = Ty;

	elment_type* _ptr = nullptr;
	int* _use_count = nullptr;
	wb_base_memory_pool* _mp = nullptr;
public:
	my_share_ptr() {};
	~my_share_ptr() {
		if (_use_count)
		{
			if (-- * (_use_count) == 0) {
				elment_type::operator delete (_ptr, *_mp);
			}
		}

		_ptr = nullptr;
		_use_count = nullptr;
		_mp = nullptr;
	}
	explicit my_share_ptr(Ty* pt, wb_base_memory_pool* mp) {
		_mp = mp;
		_ptr = pt;
		_use_count = new int;
		*_use_count = 1;
		//pt->set(_use_count);
	}

	my_share_ptr(const my_share_ptr& sp) {
		_mp = sp._mp;
		_ptr = sp._ptr;
		_use_count = sp._use_count;
		if (_use_count)
			++(*_use_count);
	}

	my_share_ptr& operator = (const my_share_ptr& p) {
		if (this == &p)return *this;
		if (_use_count) {
			if (--(*_use_count) == 0)
			{
				assert(0);
				elment_type::operator delete (_ptr, *_mp);
				delete _use_count;
			}
		}
		_mp = p._mp;
		_ptr = p._ptr;
		_use_count = p._use_count;
		if (_use_count)
			++(*_use_count);
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

	int* use_cout() {
		return _use_count;
	}
};


template <class Ty>
class auto_share_ptr
{
	using elment_type = Ty;

	elment_type* _ptr = nullptr;
	wb_base_memory_pool* _mp = nullptr;
public:
	auto_share_ptr() {};
	~auto_share_ptr() {
		if (_ptr && _ptr->DelRef()==0)
		{
			elment_type::operator delete (_ptr, *_mp);
		}

		_ptr = nullptr;
		_mp = nullptr;
	}
	explicit auto_share_ptr(Ty* pt, wb_base_memory_pool* mp) {
		_mp = mp;
		_ptr = pt;
		_ptr->AddRef();
	}

	auto_share_ptr(const auto_share_ptr& sp) {
		_mp = sp._mp;
		_ptr = sp._ptr;
		if (_ptr)_ptr->AddRef();
	}

	auto_share_ptr& operator = (const auto_share_ptr& p) {
		if (this == &p)return *this;
		if (_ptr && _ptr->DelRef()==0) {

			elment_type::operator delete (_ptr, *_mp);
		}
		_mp = p._mp;
		_ptr = p._ptr;
		if (_ptr)_ptr->AddRef();
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


class MyClass
{
	int kl = 0;
	std::atomic<int> _ref = 0;
public:
	MyClass() {}
	~MyClass() { WLOG("~MyClass\n"); };

	int Ref() { return _ref.load(); }
	int AddRef() { return ++_ref; }
	int DelRef() { return --_ref; }
	void* operator new(size_t size, wb_base_memory_pool& mp) { return mp.get_mem(); }
	void operator delete(void* p, wb_base_memory_pool& mp) { 
		((MyClass*)p)->~MyClass();
		mp.recover_mem(p);
	}
};

using share_ptr = my_share_ptr<MyClass>;
using my_que = std::map<int, share_ptr>;


class wb_mac_event : public wb_machine_event
{
	virtual void OnRead(const wb_link_interface* lk, int len) {};
	virtual void OnWrite(const wb_link_interface* lk, int len) {};

	virtual void OnSend(const wb_link_interface* lk, int len) {};
	virtual void OnRecv(const wb_link_interface* lkc, int len) {};
};

wb_mac_event mac_e;

class wb_mac_interface :public wb_machine_interface
{
	virtual void AddLink(wb_link_interface* lk) {
		WLOG("%p set_event %p \n" , lk , &mac_e);
		lk->set_event(&mac_e);
	};
	virtual void DelLink(const wb_link_interface* lk) {};
};

int main()
{
	if(0)
	{	//只能指针测试
		wb_base_memory_pool _mp;
		_mp.start(sizeof(MyClass), 100);
		std::map<int, auto_share_ptr<MyClass>> _que;
		_que[0] = auto_share_ptr<MyClass>(new(_mp) MyClass(), &_mp);
		_que[1] = auto_share_ptr<MyClass>(new(_mp) MyClass(), &_mp);
		_que[2] = auto_share_ptr<MyClass>(new(_mp) MyClass(), &_mp);
		auto fn = [&]() {
			int i = 2000000;
			while (i--)
			{
				auto p = _que[rand() % 3];
				assert( p->Ref()>= 2);
			}
			WLOG("线程完成!\n");
		};

		for (int i = 0; i < 6; i++)
		{
			std::thread t(fn);
			t.detach();
		}
		system("pause");
	}
	{
		//特殊网络包测试	
	}
	system("pause");
	wb_mac_interface mac_i;
	mainer* m = new mainer(&mac_i);
	m->start();
    cin.get();
    m->stop();

    system("pause");
}
