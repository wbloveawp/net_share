// test_server.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <WinSock2.h>
#include <windows.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <IPHlpApi.h>
#include <atlconv.h>
#include <map>
#pragma comment(lib,"Iphlpapi.lib") 
#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"Winmm.lib")

using namespace std;

constexpr u_short s_port = 33698;
constexpr u_short c_port = 33699;

constexpr u_short g_tcp_port = 33333;

atomic_bool b_ac = true;

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

auto_event g_evn;

SOCKET get_udp_socket(u_short pt) {
    SOCKET  FSocket = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, 0);
    if (FSocket == INVALID_SOCKET)
    {
        return INVALID_SOCKET;
    }
    sockaddr_in add = { 0 };
    add.sin_family = AF_INET;
    add.sin_port = ::htons(pt);
    add.sin_addr.S_un.S_addr = INADDR_ANY;
    if (SOCKET_ERROR == ::bind(FSocket, (sockaddr*)&add, sizeof(sockaddr_in)))
    {
        printf("bind err:%d\n", GetLastError());
        closesocket(FSocket);
        return INVALID_SOCKET;
    }
    return FSocket;
}

void udp_server(const atomic_bool & b_ac)
{
    SOCKET fs = get_udp_socket(s_port);
    if (INVALID_SOCKET != fs) {

        thread t = thread([&](SOCKET s) {

            do
            {
                WSABUF bf = {};
                char buf[4096] = {};
                bf.buf = buf;
                bf.len = sizeof(buf);
                DWORD dwBytes = 0;
                sockaddr_in	addr = {};
                int len = sizeof(addr);
                DWORD dwFlag = 0;

                printf("服务器进入等待收...\n");
                if (SOCKET_ERROR != WSARecvFrom(s, &bf, 1, &dwBytes, &dwFlag, (sockaddr*)&addr, &len, nullptr, nullptr))
                {
                    printf("WSARecvFrom:[%s:%d]%d字节,数据:%s\n", inet_ntoa(addr.sin_addr), (UINT)htons(addr.sin_port), dwBytes, buf);
                    //char sendbuf[4096] = {};
                    //sprintf_s(sendbuf, 4095, "我已收到你的数据:%s", buf);

                    //WSABUF sbf = {};
                    //sbf.buf = sendbuf;
                    bf.len = dwBytes;
                    DWORD dw_send_bytes = 0;
                    WSASendTo(s, &bf, 1, &dw_send_bytes, 0, (sockaddr*)&addr, len, nullptr, nullptr);

                    printf("WSASendTo:%d字节\n", dw_send_bytes);
                }
                else
                {
                    cout << "WSARecvFrom错误码：" << GetLastError() << endl;
                }

            } while (b_ac);
            cout << "服务器收线程退出...." << endl;
        },fs);
        t.detach();
    }
}

void udp_client()
{
    SOCKET fs = get_udp_socket(c_port);
#ifndef _DEBUG
#pragma message("编译ndebug!!!!!!!!!!!!!!!!!!") 
    char ip[32] = {"60.205.206.66"};
    char buf[4096] = "test_udp";
    /*
    printf("输入服务器地址:");
    cin >> ip;  
    cin.get();

    printf("\n输入发送的数据:");
    cin >> buf;
    cin.get();
    */

#else
    char ip[32] = { "127.0.0.1" };
    char buf[4096] = "test_udp";

#endif 
    memset(buf, '1', 1471);
    buf[1471] = '2';
    buf[1472] = '3';
    buf[1473] = '4';
    memset(buf + 1474, 'a', 1471);

    printf("输入发送字节数:");
    int send_size = 0;
    cin >> send_size;
    cin.get();
    char gt = 1;
    if (INVALID_SOCKET != fs) {
        while (gt != 0)
        {
            DWORD dwBytes = 0;
            sockaddr_in	addr = {};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(s_port);
            addr.sin_addr.S_un.S_addr = inet_addr(ip);
            int len = sizeof(addr);
            DWORD dwFlag = 0;
            WSABUF bf = { };
            bf.buf = buf;
            bf.len = send_size;
            if (SOCKET_ERROR == WSASendTo(fs, &bf, 1, &dwBytes, 0, (sockaddr*)&addr, len, nullptr, nullptr))
            {
                printf("WSASendTo失败:%d\n", GetLastError());
                break;
            }
            printf("发送:%d字节数\n", dwBytes);


            if(1)
            {
                WSABUF rbf = {};
                char rbuf[4096] = {};
                rbf.buf = rbuf;
                rbf.len = sizeof(rbuf);
                DWORD rdwBytes = 0;
                sockaddr_in	raddr = {};
                int rlen = sizeof(raddr);
                DWORD rdwFlag = 0;
                printf("等待服务器数据....\n");
                if (SOCKET_ERROR == WSARecvFrom(fs, &rbf, 1, &rdwBytes, &rdwFlag, (sockaddr*)&raddr, &rlen, nullptr, nullptr)) {
                    break;
                }
                printf("收到服务器数据%d:%s\n", rdwBytes, rbuf);
            }
            printf("输入0退出,其他继续:\n");
            //printf("收到服务器数据%d:%s\n", dwBytes, buf);
            cin.get(gt);// >> gt;
            cin.get();
        }
        closesocket(fs);
    }
}


SOCKET get_tcp_socket()
{
    return WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, 0);  
}


void tcp_server()
{
    SOCKET s = get_tcp_socket();
    if (s == INVALID_SOCKET)
    {
        printf("get_tcp_socket 失败:%d\n", GetLastError());
        return;
    }
    sockaddr_in add = { 0 };
    add.sin_family = AF_INET;
    add.sin_port = ::htons(g_tcp_port);
#ifdef _DEBUG
    add.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
#else
    add.sin_addr.S_un.S_addr = inet_addr("192.168.2.49");
#endif
    if (SOCKET_ERROR == ::bind(s, (sockaddr*)&add, sizeof(sockaddr_in)))
    {
        printf("tcp server bind err:%d\n", GetLastError());
        closesocket(s);
    }

    if (SOCKET_ERROR == ::listen(s, 10))
    {
        printf("tcp server listen err:%d\n", GetLastError());
        closesocket(s);
    }

    std::thread t = std::thread([=]() {

        do
        {
            sockaddr_in addr = { 0 };
            int addr_len = sizeof(sockaddr_in);
            SOCKET as = accept(s, (sockaddr*)&add, &addr_len);
            if (as == INVALID_SOCKET)break;
            std::thread recv_t = std::thread([=]() {
                char buf[8192+2500] = {};
                int ret = 0;
                do
                {
                    ret = recv(as, buf, 8192, 0);
                    printf("服务器recv返回:%d %s\n", ret, buf);

                    
                    if (ret) {
                        if (send(as, buf, 8192 + 2500, 0) <= 0) {
                            printf("服务器send失败:%d\n", GetLastError());
                            break;
                        }
                        else
                        {
                            //printf("我已回复数据:%d\n", ret);
                        }

                    }
                    
                } while (ret > 0);
                closesocket(as);
                });
            recv_t.detach();
        } while (b_ac);
        closesocket(s);
        });
    t.detach();
    system("pause");
    closesocket(s);
};

void tcp_client()
{
    SOCKET s = get_tcp_socket();
    if (s == INVALID_SOCKET)
    {
        printf("get_tcp_socket 失败:%d\n", GetLastError());
        return;
    }
    sockaddr_in add = { 0 };
    add.sin_family = AF_INET;
    add.sin_port = ::htons(g_tcp_port);
#ifdef _DEBUG
    add.sin_addr.S_un.S_addr = inet_addr("60.205.206.66");//127.0.0.1
#else
    add.sin_addr.S_un.S_addr = inet_addr("60.205.206.66");
#endif
    if (SOCKET_ERROR == connect(s, (sockaddr*)&add, sizeof(sockaddr_in)))
    {
        printf("tcp connect 失败:%d\n",GetLastError());
        return;
    }
    printf("连接成功!\n");
    
    //closesocket(s);
    
    char t = 1;
    INT64 _all_recv = 0;
    int send_times = 0;
    int len = 0;
    cin >> len;
    cin.get();
    printf("\n");
    do
    {


        char buf[8192+4096] = {};
        memset(buf, '1', len);
        int send_len = send(s, buf, len + 1, 0);
        if (send_len <= 0)
        {
            printf("send 失败:%d\n",GetLastError());
            break;
        }
       // printf("send 成功:%d %s\n", send_len , buf );
        //_all_send += send_len;
        char rbuf[8192] = {};
        int rt = recv(s, rbuf, 4096, MSG_PEEK);
        cout <<"收:" <<rt << " err="<< GetLastError() << endl;
        if (rt <= 0)break;
        _all_recv += rt;
        //printf("recv 成功:%d %s\n", rt ,rbuf );
       // cin >> t;
        //cin.get();
    } while (send_times-->0);
    cout << "收" << _all_recv / 1024 << "KB" << endl;
     closesocket(s);

     
}




template <class Ty>
class wb_share_prt
{
public:
    wb_share_prt() {};
    ~wb_share_prt()
    {
        if (use_count)
        {
            if (--(*use_count) <= 0)
            {
                //assert(*use_count == 0);
                delete p_ttr;
            }
        }
    }
    explicit wb_share_prt(Ty* p) {
        p_ttr = p;
        if (!use_count)
        {
            use_count = new int;
            *use_count = 1;
        }
        else
         ++(*use_count);
    };

    wb_share_prt(const wb_share_prt& p) {
        p_ttr = p.p_ttr;
        use_count = p.use_count;
        ++(*use_count);
    }

    wb_share_prt&  operator = (const wb_share_prt& p) {
        p_ttr = p.p_ttr;
        use_count = p.use_count;
        ++(*use_count);
        return *this;
    }

    operator Ty*() {

        return p_ttr;
    }

    Ty* operator->() const
    {
        return p_ttr;
    }
protected:

    Ty* p_ttr = nullptr;
    int* use_count = nullptr;
};


class Test
{
    int ak = 0;
public:
    Test() { printf("Test()\n"); };
    ~Test() { printf("~Test()\n"); };

    void Do() { ak++; printf("Test.Do\n"); };

    int get() { return ak; };
};

using t_map = std::map<int, wb_share_prt<Test>>;
using s_map = std::map<int, std::shared_ptr<Test>>;

void tfun(int) {

}
int main()
{
    void (*p)(int) = tfun;

    p(1);
    t_map tm;
    s_map sm;
    {

        wb_share_prt<Test> pt(new Test());
        pt->Do();
        tm[pt->get()] = pt;
    }
    system("pause");
    //tm.clear();
    {
        char name[256];

        int getNameRet = gethostname(name, sizeof(name));

        hostent* host = gethostbyname(name);

        if (NULL == host) {
            return 0;
        }

        in_addr* pAddr = (in_addr*)*host->h_addr_list;

        for (int i = 0; i < (strlen((char*)*host->h_addr_list) - strlen(host->h_name)) / 4 && pAddr; i++) {
            string addr = inet_ntoa(pAddr[i]);
            cout << addr.c_str() << endl;
        }
    }
    {
        PIP_ADAPTER_INFO pIpAdapterInfo = NULL;
        IP_ADAPTER_INFO IpAdapterInfo = { 0 };
        unsigned long stSize = sizeof(IP_ADAPTER_INFO);
        char* byte = NULL;
        int nRel = GetAdaptersInfo(&IpAdapterInfo, &stSize);
        if (ERROR_BUFFER_OVERFLOW == nRel)
        {
            byte = new char[stSize];
            pIpAdapterInfo = (PIP_ADAPTER_INFO)byte;
            nRel = GetAdaptersInfo(pIpAdapterInfo, &stSize);
            if (ERROR_SUCCESS != nRel)
            {
                delete[]byte;
                return false;
            }
        }
        else if (ERROR_SUCCESS == nRel)
        {
            pIpAdapterInfo = &IpAdapterInfo;
        }
        while (pIpAdapterInfo)
        {


            printf("%s  %s  %s\n", pIpAdapterInfo->AdapterName, pIpAdapterInfo->Description, pIpAdapterInfo->IpAddressList.IpAddress);


            pIpAdapterInfo = pIpAdapterInfo->Next;
        }
        if (byte)
            delete[]byte;
    }


    printf("0.退出\n1.udp server\n2.udp client\n3.tcp server\n4.tcp client\n");
    char get;
    cin >> get;
    cin.get();

    switch (get)
    {
    case '1': udp_server(b_ac); break;
    case '2': udp_client(); break;
    case '3': tcp_server(); break;
    case '4': tcp_client(); break;
    }
    system("pause");
    b_ac = false;




    this_thread::sleep_for(chrono::seconds(1));
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
