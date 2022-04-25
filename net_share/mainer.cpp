
#include "mainer.h"
#include <IPHlpApi.h>
#include <atlconv.h>
#pragma comment(lib,"Iphlpapi.lib") 


mainer::mainer(wb_machine_interface* mi){
	_udp = new wb_udp_filter(this);
	_tcp = new wb_tcp_filter(this,mi);
	_icmp = new wb_icmp_filter(this);
	if (!_udp || !_tcp || !_icmp)
		throw "mainer ��ʼ��ʧ��";
}

mainer::~mainer() {
	stop();
	delete _udp;
	delete _tcp;
	delete _icmp;
}

bool mainer::start() {
	SYSTEM_INFO si = {};
	GetSystemInfo(&si);
	auto ithreads =  si.dwNumberOfProcessors * 2 + 2;//Ĭ�������߳�����release��ʹ��Ĭ��cpu*2+2
	WLOG("ithreads=%d\n", ithreads);
	//ithreads = 1;
	//�������ݶ�ȡ
	auto net_card_work = [=](DWORD numberOfBytes, ULONG_PTR ComKey, LPOVERLAPPED pol)->bool {
		WB_OVERLAPPED* pl = (WB_OVERLAPPED*)pol;
		if (numberOfBytes <= 0 || nullptr == pol)
		{
			if (pl) {
				WLOG("����pol:%p\n", pol);
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
		return true;//ֻ�᷵��true
	};
	_nc_io.start<decltype(net_card_work)>(ithreads,net_card_work);//����io
	bool _ok = true;
	do
	{	//��ȡ��ǰip
		auto i_ip = get_local_ip();
		in_addr ia = {};
		ia.S_un.S_addr = ntohl(i_ip);
		_szIP = inet_ntoa(ia);
		char buf[1024] = {};
		char szMask[24] = {};
		NET_IP_FILTER nif = {};
		//nif.uiNetMasks = inet_addr("255.255.0.0");//Ĭ������
		//nif.uiNetSeccion = inet_addr("192.168.0.0");//Ĭ�������
		if (!(_ok = get_net_card_name(_szIP.c_str(), buf, sizeof(buf), szMask,sizeof(szMask))))
		{
			RLOG("get_net_card_name ʧ��:%d [%s %d]\n", GetLastError(),__FILE__,__LINE__);
			break;
		}
		_szCardName = buf;
		::sprintf_s(buf, sizeof(buf) - 1, "\\DEVICE\\%s", _szCardName.c_str());
		nif.uiNetMasks = inet_addr(szMask);//ʹ�ø�ip���ڵ�����ӿڵ�����
		nif.uiNetSeccion = inet_addr(_szIP.c_str())& nif.uiNetMasks;//ʹ�ø�ip���ڵ�����ӿڵ������

		RLOG("����:1%%...");
		if (!(_ok = _ncf.openDriver(NDIS_DRIVER_NAME))) {//����������
			RLOG("_ncf.openDriver ʧ��:%d\n", GetLastError());
			break;
		}

		RLOG("����:10%%...");
		if (!(_ok = _ncf.setFilter(&nif, sizeof(nif))))//���ù������������ͱ�������һ���������������Ÿ�������ת��
		{
			RLOG("_ncf.setFilter ʧ��:%d\n", GetLastError());
			break;
		}
		RLOG("����:20%%...");
		USES_CONVERSION;
		if (!(_ok = _ncf.openCard(A2W(buf), NDIS_DRIVER_NAME)))//����������(�ļ�)
		{
			RLOG(" _ncf.openCard ʧ��:%d [%s %d]\n", GetLastError(), __FILE__, __LINE__);
			break;
		}
		RLOG("����:40%%...");
		if (!(_ok = _nc_io.relate((HANDLE)_ncf, this)))//��������(�ļ�)��io��ɶ˿ڹ���
		{
			RLOG(" _nc_io.relate ʧ��:%d [%s %d]\n", GetLastError(), __FILE__, __LINE__);
			break;
		}
		RLOG("����:50%%...");
		if (!_udp->start(ithreads))
		{
			RLOG(" _udp->start ʧ��:%d [%s %d]\n", GetLastError(), __FILE__, __LINE__);
			break;
		}
		RLOG("����:60%%...");
		if (!_tcp->start(ithreads))
		{
			RLOG(" _tcp->start ʧ��:%d [%s %d]\n", GetLastError(), __FILE__, __LINE__);
			break;
		}
		RLOG("����:70%%...");
		if (!_icmp->start(ithreads))
		{
			RLOG(" _tcp->start ʧ��:%d [%s %d]\n", GetLastError(), __FILE__, __LINE__);
			break;
		}
		RLOG("����:80%%...");
		if (!_mp.start(sizeof(WB_OVERLAPPED), 8024))
		{
			RLOG("_mp.start ʧ��:%d [%s %d]\n", GetLastError(), __FILE__, __LINE__);
			break;
		}
		RLOG("����:90%%...");
		while (_default_reads < ithreads+2)
		{
			WB_OVERLAPPED* p = new (_mp.get_mem()) WB_OVERLAPPED;
			memset(p, 0, sizeof(WB_OVERLAPPED));
			p->ot = io_oper_type::e_read;
			//WLOG("Ͷ��ol:%p\n",p);
			if(!(_ok = _ncf.Read(p->buf, MAX_MAC_LEN, &p->ol)))
				break;
			_default_reads++;
		}
		RLOG("����:100%%...\n");
		RLOG("����IP=%s  Mask=%s  uMask=%x uIP=%x\n",_szIP.c_str(), szMask, nif.uiNetMasks, nif.uiNetSeccion);
		
	} while (0);
	if (!_ok)
	{
		RLOG("����ʧ��:%d\n",GetLastError());
		stop();
	}
	else
	{
		RLOG("�����ɹ�!\n");
	}	
	return _ok;
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

void mainer::post_write(wb_filter_event* p_nce, wb_link_interface* plink, const ETH_PACK* pk, int len, int data_len)
{
	WB_OVERLAPPED* pl = new(_mp.get_mem()) WB_OVERLAPPED();
	if (pl) {
		pl->ot = io_oper_type::e_write;
		pl->p_nce = p_nce;
		pl->p_lk = plink;
		memcpy(pl->buf, pk, len);
		
		if (writes >= 390)
		{
			_lock.lock();
			_write_list.push_back(pl);
			pl->len = len;
			_lock.unlock();
			return;
		}
		if (!_ncf.Write(pl->buf, len, &pl->ol))
		{
			//RLOG("_ncf ʧ��:%d   %d\n", GetLastError(), writes.load());
			_mp.recover_mem(pl);
		}
		else
			++writes;
	}
}

void mainer::DoOnWrite(DWORD numberOfBytes, WB_OVERLAPPED* pol) {
	--writes;
	_write_packs++;
	_write_bytes += numberOfBytes;
	if (pol->p_nce)
		pol->p_nce->OnWrite(pol->p_lk, (ETH_PACK*)pol->buf, numberOfBytes);
	_mp.recover_mem(pol);
	//�Ƿ����Ͷ��write
	_lock.lock();
	if (_write_list.size() > 0) {
		auto pl = *_write_list.begin();
		if (_ncf.Write(pl->buf, pl->len, &pl->ol))
		{
			_write_list.pop_front();
		}
	}
	_lock.unlock();
}

//io�¼�
void mainer::DoOnRead(DWORD numberOfBytes, WB_OVERLAPPED* pl) {
	//numberOfBytes>0 && pl!=null �������ѱ�֤ 
	//�ײ������ѹ��˵�����Э��������,
	//1.���ݰ����ͷ��ɸ���Ӧ�Ĺ�����->OnRead()
	ETH_PACK* pk = (ETH_PACK*)pl->buf;
	_read_packs++;
	_read_bytes += numberOfBytes;
	switch (pk->ip_h.cTypeOfProtocol) {
	case PRO_TCP: _tcp->OnRead(pk, numberOfBytes); break;
	case PRO_UDP: _udp->OnRead(pk, numberOfBytes); break;
	case PRO_ICMP:_icmp->OnRead(pk, numberOfBytes); break;
	}
	//2.�ٴ�ѭ��ʹ�ô˰��ڴ�����Ͷ��
	if (!_ncf.Read(pl->buf, MAX_MAC_LEN, &pl->ol)) {
		WLOG("_ncf.Read����Ͷ��ʧ��%d\n", GetLastError());
		_default_reads--;
		//stop();
	}
}
//github�ύ����
bool mainer::get_net_card_name(const char* ip, char* buffer, int buf_len, char* mask, int mask_len)
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
			delete []byte;
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

			//::sprintf_s(buffer, buf_len - 1, "\\DEVICE\\%s", pIpAdapterInfo->AdapterName);
			::sprintf_s(buffer, buf_len - 1,"%s", pIpAdapterInfo->AdapterName);
			memcpy(mask,pIpAdapterInfo->IpAddressList.IpMask.String,sizeof(pIpAdapterInfo->IpAddressList.IpMask.String));
			//memcpy(buffer, pIpAdapterInfo->AdapterName, strlen(pIpAdapterInfo->AdapterName));
			break;
		}
		pIpAdapterInfo = pIpAdapterInfo->Next;
	}
	if (byte)
		delete []byte;
	return true;
}