
#include "evn_init.h"
#include <Windows.h>
#include <stdio.h>
#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"Winmm.lib")




wb_auto_init_net gAutoInitNet;

wb_auto_init_net::wb_auto_init_net()
{
    timeBeginPeriod(1);
    WORD ver = MAKEWORD(2, 0);
    WSADATA data;
    if (WSAStartup(ver, &data) != 0)
        throw "Æô¶¯ÍøÂç¿âÊ§°Ü";
}

wb_auto_init_net::~wb_auto_init_net()
{
    ::WSACleanup();
    timeEndPeriod(1);
}