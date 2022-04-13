
#ifndef XYTIMER_H
#define XYTIMER_H


#include <Windows.h>

class XYTimer
{
	HANDLE			m_hTimer;
	BOOL			m_Active;

	static HANDLE	m_hTimerQueue;
public:

	static BOOL		Init();
	static VOID		UnInit();

	XYTimer();
	~XYTimer();

	BOOL start_timer(WAITORTIMERCALLBACK time_proc, LPVOID lparam,DWORD dueTime, DWORD perid);

	void stop_timer();

	BOOL IsRuning() { return m_Active; }
};

class XYTimerAutoEvnInit
{
public:
	XYTimerAutoEvnInit() { XYTimer::Init(); };
	~XYTimerAutoEvnInit() { XYTimer::UnInit(); };
};

extern XYTimerAutoEvnInit TimerAutoInit;
#endif // !XYTIMER_H