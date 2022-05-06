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

using namespace std;



class Mt
{
    friend std::ostream & operator << (std::ostream & ct ,const Mt &t) {
        return ct << "this Mt cout "<<&t ;
    }
public:

};

template <class t>
void print(t a)
{
    cout << a <<" ";
}

template <class ... cls>
void print(cls...agv);

template <class t,class ... cls>
void print(t a, cls...agv) {
    print(a);
    print(agv...);
    cout << endl;
}


int main()
{

    int* ar = new int[12];

    ar[12] = 19;

    std::tuple<string, int> t = { "121",11 };
    auto k = std::make_tuple<int, double>(11,1.0);

    auto kl = std::get<0>(k);
    print(1121,11.0,"asda",Mt(), kl);
    
    system("pause");

}