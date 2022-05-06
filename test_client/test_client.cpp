// test_client.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <WinSock2.h>
#include <windows.h>
#include <iostream>
#include <thread>
#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"Winmm.lib")

using namespace std;


class auto_event {

public:
    auto_event() {
        WORD ver = MAKEWORD(2, 0);
        WSADATA data;
        if (WSAStartup(ver, &data) != 0)
            throw "启动网络库失败";
    }
    ~auto_event() {
        WSACleanup();
    }
};

SOCKET get_udp_socket() {
    SOCKET  FSocket = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, 0);
    if (FSocket == INVALID_SOCKET)
    {
        return INVALID_SOCKET;
    }
    sockaddr_in add = { 0 };
    add.sin_family = AF_INET;
    add.sin_port = ::htons(33698);
    add.sin_addr.S_un.S_addr = INADDR_ANY;
    if (SOCKET_ERROR == ::bind(FSocket, (sockaddr*)&add, sizeof(sockaddr_in)))
    {
        printf("bind err:%d\n", GetLastError());
        closesocket(FSocket);
        return INVALID_SOCKET;
    }
    return FSocket;
}



template <class  T>
class Tree
{


    struct Node
    {
        T       data;
        Node*   left;
        Node*   right;
    };

    Node  _node = {};
 
public:
    Tree();
    Tree(const T& t) {
        _node.data = t;
    }

    void insert(const T& t) {
        if (!_node.data)
        {
            _node.data = t;
            return;
        }
        Node* tn = &_node;
        for (;tn;)
        {
            if (t < tn->data)
            {
                if (!tn->left)
                {
                    tn->left = new Node();
                    tn->left->data = t;
                    return;
                }
                tn = tn->left;
            }
            else
            {
                if (!tn->right)
                {
                    tn->right = new Node();
                    tn->right->data = t;
                    return;
                }
                tn = tn->right;
            }
        }
    }
};

int main()
{
    
    Tree<int> tr(12);
    tr.insert(13);
    tr.insert(10);
    tr.insert(11);

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
