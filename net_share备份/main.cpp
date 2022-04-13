
#include "main.h"
#include <IPHlpApi.h>
#include <atlconv.h>
#pragma comment(lib,"Iphlpapi.lib") 


mainer::mainer() {
	_udp = new wb_udp_filter(this);
	_tcp = new wb_tcp_filter(this);
	_icmp = new wb_icmp_filter(this);
	if (!_udp || !_tcp || !_icmp)
		throw "mainer 初始化失败";
}
mainer::~mainer() {
	delete _udp;
	delete _tcp;
	delete _icmp;
}

#ifdef _DEBUG
#define WB_DEFAULT_READS 60
#else
#define WB_DEFAULT_READS 60
#endif

void mainer::start() {
	//stop();
	SYSTEM_INFO si = {};
	GetSystemInfo(&si);
	auto ithreads = si.dwNumberOfProcessors * 2 + 2;//默认启动线程数，release下使用默认cpu*2+2
	WLOG("ithreads=%d\n", ithreads);
	//ithreads = 1;
	//网卡数据读取
	auto net_card_work = [=](DWORD numberOfBytes, ULONG_PTR ComKey, LPOVERLAPPED pol)->bool {
		WB_OVERLAPPED* pl = (WB_OVERLAPPED*)pol;
		if (numberOfBytes <= 0 )
		{
			if (pl) {
				WLOG("回收pol:%p\n", pol);
				_default_reads--;
				_mp.recover_mem(pl);
			}
			return true;
		}

		switch (pl->ot)
		{
		case io_oper_type::e_read: this->DoOnRead(numberOfBytes, pl); break;
		case io_oper_type::e_write:this->DoOnWrite(numberOfBytes, pl); break;
		}
		return true;//只会返回true
	};
	_nc_io.start<decltype(net_card_work)>(ithreads,net_card_work);//网卡io
	bool _ok = true;
	do
	{	//获取当前ip
		auto i_ip = get_local_ip();
		in_addr ia = {};
		ia.S_un.S_addr = ntohl(i_ip);
		std::string my_ip = inet_ntoa(ia);
		if (!(_ok = _ncf.openDriver(NDIS_DRIVER_NAME))) {
			RLOG("_ncf.openDriver 失败:%d\n",GetLastError());
			break;
		}

		NET_IP_FILTER nif = {};
		nif.uiNetMasks = inet_addr("255.255.0.0") ;
		nif.uiNetSeccion = inet_addr("192.168.0.0");

		printf("本机%d  %d IP=%s\n", nif.uiNetMasks, nif.uiNetSeccion, my_ip.c_str());
		if (!(_ok = _ncf.setFilter(&nif, sizeof(nif))))
		{
			RLOG("_ncf.setFilter 失败:%d\n", GetLastError());
			break;
		}
		
		char buf[1024] = {};
		if (!(_ok = get_net_card_name(my_ip.c_str(), buf, 1024)))
		{
			RLOG("get_net_card_name 失败:%d [%s %d]\n", GetLastError(),__FILE__,__LINE__);
			break;
		}
		USES_CONVERSION;
		if (!(_ok = _ncf.openCard(A2W(buf), NDIS_DRIVER_NAME)))
		{
			RLOG(" _ncf.openCard 失败:%d [%s %d]\n", GetLastError(), __FILE__, __LINE__);
			break;
		}
		if (!(_ok = _nc_io.relate((HANDLE)_ncf, this)))
		{
			RLOG(" _nc_io.relate 失败:%d [%s %d]\n", GetLastError(), __FILE__, __LINE__);
			break;
		}
		if (!_udp->start(ithreads))
		{
			RLOG(" _udp->start 失败:%d [%s %d]\n", GetLastError(), __FILE__, __LINE__);
			break;
		}
		if (!_tcp->start(ithreads))
		{
			RLOG(" _tcp->start 失败:%d [%s %d]\n", GetLastError(), __FILE__, __LINE__);
			break;
		}
		
		if (!_icmp->start(ithreads))
		{
			RLOG(" _tcp->start 失败:%d [%s %d]\n", GetLastError(), __FILE__, __LINE__);
			break;
		}
		
		if (!_mp.start(sizeof(WB_OVERLAPPED), 8024))
		{
			RLOG("_mp.start 失败:%d [%s %d]\n", GetLastError(), __FILE__, __LINE__);
			break;
		}
		while (_default_reads < ithreads)
		{
			WB_OVERLAPPED* p = new (_mp.get_mem()) WB_OVERLAPPED;
			memset(p, 0, sizeof(WB_OVERLAPPED));
			p->ot = io_oper_type::e_read;
			//WLOG("投递ol:%p\n",p);
			if(!(_ok = _ncf.Read(p->buf, MAX_MAC_LEN, &p->ol)))
				break;
			_default_reads++;
		}
	} while (0);
	if (!_ok)
	{
		RLOG("启动失败:%d\n",GetLastError());
		stop();
	}
	else
		RLOG("启动成功!\n");
		
}

void mainer::stop() {
	_ncf.close();
	while (_default_reads > 0) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	WLOG("mainer stop...\n");
	_nc_io.stop();
	_udp->stop();
	_tcp->stop();
	_icmp->stop();
	_mp.stop();
}

std::atomic_int writes = 0;
void mainer::post_write(wb_filter_event* p_nce, wb_link_interface* plink, const ETH_PACK* pk, int len)
{
	WB_OVERLAPPED* pl = new(_mp.get_mem()) WB_OVERLAPPED();
	if (pl) {

		//memset(pl, 0, sizeof(WB_OVERLAPPED));
		pl->ot = io_oper_type::e_write;
		pl->p_nce = p_nce;
		pl->p_lk = plink;
		memcpy(pl->buf, pk, len);
		while (writes >= 390)
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		if (!_ncf.Write(pl->buf, len, &pl->ol)) {
			RLOG("_ncf 失败:%d   %d\n", GetLastError(), writes.load());
			//stop();
			_mp.recover_mem(pl);
		}
		else
			++writes;
	}
}

void mainer::DoOnWrite(DWORD numberOfBytes, WB_OVERLAPPED* pol) {
	--writes;
	if (pol->p_nce)
		pol->p_nce->OnWrite(pol->p_lk, (ETH_PACK*)pol->buf, numberOfBytes);
	_mp.recover_mem(pol);
}

//io事件
void mainer::DoOnRead(DWORD numberOfBytes, WB_OVERLAPPED* pl) {
	//1.根据包类型分派给对应的过滤器->OnRead()
	ETH_PACK* pk = (ETH_PACK*)pl->buf;
	switch (pk->ip_h.cTypeOfProtocol) {
	case PRO_TCP: _tcp->OnRead((ETH_PACK*)pl->buf, numberOfBytes); break;
	case PRO_UDP: _udp->OnRead((ETH_PACK*)pl->buf, numberOfBytes); break;
	case PRO_ICMP:_icmp->OnRead((ETH_PACK*)pl->buf, numberOfBytes); break;
	}
	//2.再次循环使用此包内存重新投递
	if (!_ncf.Read(pl->buf, MAX_MAC_LEN, &pl->ol)) {
		WLOG("_ncf.Read重新投递失败%d\n", GetLastError());
		_default_reads--;
		stop();
	}
}

bool mainer::get_net_card_name(const char* ip, char* buffer, int buf_len)
{
	if (!buffer || buf_len < 0)
		return false;

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
		if (strcmp(pIpAdapterInfo->IpAddressList.IpAddress.String, ip) == 0)
		{
			if (buf_len < strlen(pIpAdapterInfo->AdapterName))
			{
				return false;
			}

			::sprintf_s(buffer, buf_len - 1, "\\DEVICE\\%s", pIpAdapterInfo->AdapterName);
			//memcpy(buffer, pIpAdapterInfo->AdapterName, strlen(pIpAdapterInfo->AdapterName));
			break;
		}
		pIpAdapterInfo = pIpAdapterInfo->Next;
	}
	if (byte)
		delete[]byte;
	return true;
}

/*
void mainer::DoOnConnecting(WB_OVERLAPPED_N* pl)
{
	ustrt_net_base_info& nbi = (ustrt_net_base_info&)(*pl->pl);

	sockaddr_in add = { 0 };
	add.sin_family = AF_INET;
	add.sin_port = nbi.ip_info.dst_port;
	add.sin_addr.S_un.S_addr = nbi.ip_info.dst_ip;
	SOCKET s = (SOCKET)(*pl->pl);
	ULONG arg = 1;
	int ret = ::ioctlsocket(s, FIONBIO, &arg);
	if (NO_ERROR != ret)
	{
		((wb_tcp_filter*)pl->pf)->OnConnected(this, pl->pl, ret);
		return ;
	}
	ret = connect(s, (sockaddr*)&add, sizeof(add));
	if (SOCKET_ERROR != ret)
	{
		((wb_tcp_filter*)pl->pf)->OnConnected(this, pl->pl, 0);
		return ;
	}

	TIMEVAL	to;
	fd_set	fsW;

	FD_ZERO(&fsW);
	FD_SET(s, &fsW);
	to.tv_sec = 2;
	to.tv_usec = 0;

	ret = select(0, NULL, &fsW, NULL, &to);
	if (ret > 0)
	{
		// 改回阻塞模式:
		arg = 0;
		ioctlsocket(s, FIONBIO, &arg);
		((wb_tcp_filter*)pl->pf)->OnConnected(this, pl->pl, 0);
	}
	else
	{
		((wb_tcp_filter*)pl->pf)->OnConnected(this, pl->pl, GetLastError());
	}
}
*/
