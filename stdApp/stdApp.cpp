// stdApp.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include <windows.h>
#include <iostream>
#include <bitset>
#include <functional>
using namespace std;

void foo()noexcept
{
    //throw "111";
}

template <class T>
constexpr T w_spuare(T t)
{
    return t * t;
}

class C
{
    int* _p = nullptr;
public:
    C(int k,int l) {
        _p = new int[k];
        for (int  i=0 ;i<k;i++)
        {
            _p[i] = l * i;
        }
    }

    ~C()
    {
        if (_p)
            delete[]_p;
        _p = nullptr;
    }

    C& operator=(C&& c)noexcept
    {
        if (_p)
            delete[]_p;
        _p = c._p;
        c._p = nullptr;
        return *this;
    }

    C(C && c)noexcept
    {
        _p = c._p;
        c._p = nullptr;
    }

    int operator[](int k)
    {
        return _p[k];
    }
};

//lambda 测试

void lamdba_test()
{
    using int_fun = std::function<int(int, int)>;
    int_fun f = [](int x, int y) {

        int k();
        return x * y;
    };

    f(11, 12);
}

int main()
{

    cout<<std::bitset<16>(158);

    C c(8, 2);

    cout << c[3];
    C c1(std::move(c));
 
    std::cout << "Hello World!\n";
    
    C c2(1, 0);
    c2 = std::move(c1);


    foo();

    auto s = R"nc(a\
            b\nc()"
            )nc";

    auto _u8 = L"asdada";
    cout << _u8 << endl;
    
    system("pause");
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
