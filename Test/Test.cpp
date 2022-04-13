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

    auto a = sizeof(A);

    auto b = sizeof(B);
    T t;
    std::cout << t;
    Fun(11, 12.0,"我擦","12313");
    std::cin.get();
}