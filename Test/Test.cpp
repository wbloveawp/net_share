// Test.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//


#include "XYTimer.h"
#include <iostream>
#include <thread>



template <class T>
void Fun(const T& a)
{
    std::cout << a <<std::endl;
}

template <class T,class ...args>
void Fun(const T& a, const args&...ags)
{
    std::cout << a << ",";
    Fun(ags...);
}

class T
{
    
    friend std::ostream& operator << (std::ostream& cut,const T &t)
    {
        return cut << "this is T ..." << std::endl;
    }
public:
};

template <class T1 ,class T2 >
auto funn(T1 a, T2 b)->decltype(a + b) {
    return a + b;
}


struct A
{
    unsigned long long ll;
    char c;
};

struct B :public A
{
    char cc;
    int kl;
};


int
p_fun(
    __in    int kl,
    __out   int& pl

)
{
    pl = 12;
    return 12;
}

int main()
{

    std::atomic<INT64> recv_byte = 0;

    std::thread t = std::thread([&]() {
        for (size_t i = 0; i < 10000000; i++)
        {
            recv_byte+=5;
        }
       
        });

    std::thread t1 = std::thread([&]() {
        for (size_t i = 0; i < 10000000; i++)
        {
            recv_byte+=5;
        }
        });
    std::cout << "start" << std::endl;
    t1.detach();
    t.detach();
    for (size_t i = 0; i < 10000000; i++)
    {
        recv_byte += 5;
    }
    system("pause");
    std::cout << recv_byte << std::endl;
    std::cin.get();
}