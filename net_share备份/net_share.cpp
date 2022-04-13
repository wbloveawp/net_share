// net_share.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "evn_init.h"
#include "main.h"
#include <algorithm>
using namespace std;


class MyClass
{
    int ak = 0;
public:
    MyClass() {
        printf("MyClass\n");
    }
    virtual ~MyClass() {
        printf("~MyClass\n");
    }

    virtual void fun() {
        printf("MyClass::fun()\n");
    }

    int get() { return ak; }
    int set(int k) { return ak = k; }

    void* operator new(size_t size, wb_base_memory_pool& mp) { return mp.get_mem(); }
    void operator delete(void* p, wb_base_memory_pool& mp) {
        ((MyClass*)p)->~MyClass();
        mp.recover_mem(p);
    }
};

class Text_ex :public MyClass
{
public:
    Text_ex() {
        printf("Text_ex\n");
    }
    virtual ~Text_ex() {
        printf("~Text_ex\n");
    }

    virtual void fun() {
        printf("Text_ex::fun()\n");
    }
};


void test_fun(wb_share_ptr<MyClass> sp) {
    sp->fun();
}

#define MY_STRUCT(x) ((MyStruct*)x)
struct MyStruct
{
    wb_share_ptr<MyClass> sp;

    void* operator new (size_t size, wb_base_memory_pool& mp) {
        return mp.get_mem();
    }

    void operator delete (void* p, wb_base_memory_pool& mp) {
        MY_STRUCT(p)->~MyStruct();
        mp.recover_mem(p);
    }
};

int main()
{
    if (0)
    {
        wb_base_memory_pool _mp;
        wb_base_memory_pool _sp;

        _sp.start(sizeof(MyStruct), 100);
        _mp.start(sizeof(Text_ex), 1000);

        {
            auto ms = new (_sp) MyStruct();
            auto sp = wb_share_ptr<Text_ex>(new (_mp) Text_ex(), &_mp);
            //test_fun(sp);
            MyStruct::operator delete(ms, _sp);
            system("pause");
        }
        system("pause");
    }
    mainer m;
    m.start();
    cin.get();
    m.stop();

    system("pause");
}
