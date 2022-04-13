#include <iostream>
#include "XYTimer.h"

XYTimerAutoEvnInit TimerAutoInit;

HANDLE XYTimer::m_hTimerQueue = NULL;

XYTimer::XYTimer()
{
	m_hTimer = NULL;
	m_Active = FALSE;
}
XYTimer::~XYTimer()
{
	stop_timer();
}
BOOL XYTimer::start_timer(WAITORTIMERCALLBACK time_proc, LPVOID lparam, DWORD dueTime, DWORD perid)
{
	
	return m_Active = CreateTimerQueueTimer(&m_hTimer, m_hTimerQueue,
		time_proc, lparam, dueTime, perid, WT_EXECUTEDEFAULT);
}

void XYTimer::stop_timer()
{
	if (m_hTimer)
	{
		DeleteTimerQueueTimer(m_hTimerQueue, m_hTimer, INVALID_HANDLE_VALUE);
	}
	m_hTimer = NULL;
	m_Active = FALSE;
}


BOOL XYTimer::Init()
{
	m_hTimerQueue = CreateTimerQueue();//创建定时器队列
	if (NULL == m_hTimerQueue)
	{
		wchar_t exception_msg[50] = { 0 };
		swprintf_s(exception_msg, 49, L"创建定时器队列失败,程序崩溃 错误代码：%d", GetLastError());
		throw exception_msg;
	}
	return TRUE;
}

VOID XYTimer::UnInit()
{
	if (m_hTimerQueue)
		DeleteTimerQueueEx(m_hTimerQueue, NULL);
	m_hTimerQueue = NULL;
}
