#include "net_filter.h"
#include <iostream>
#include <algorithm>

bool wb_udp_filter::start(int thds) {
	auto io_event = [=](DWORD numberOfBytes, ULONG_PTR ComKey, LPOVERLAPPED pol)->bool {
		WB_OVERLAPPED_UDP* pl = (WB_OVERLAPPED_UDP*)pol;
		if (pl)
		{
			switch (pl->ot)
			{
			case udp_io_oper_type::e_send:this->DoOnSend((wb_udp_link*)ComKey, pl->buf, numberOfBytes, pl);break;
			case udp_io_oper_type::e_recv:this->DoOnRecv((wb_udp_link*)ComKey, pl->buf, numberOfBytes, pl); break;
			case udp_io_oper_type::e_send_no_copy:this->DoOnSend_No_Copy((wb_udp_link*)ComKey, pl->wbuf.buf, numberOfBytes, pol); break;
			}
		}
		return true;
	};
	auto ret = false;
	do
	{
		if (!__super::start(thds))
		{
			WLOG("_mlp.start ʧ��:%d  %s %d\n", GetLastError(), __FILE__, __LINE__);
			break;
		}
		if (!_mlp.start(sizeof(wb_udp_link), 1024*10))
		{
			WLOG("_mlp.start ʧ��:%d  %s %d\n",GetLastError() , __FILE__,__LINE__);
			break;
		}
		if(!_mop.start(sizeof(WB_OVERLAPPED_UDP), 1024*10)) 
		{
			WLOG("_mop.start ʧ��:%d  %s %d\n", GetLastError(), __FILE__, __LINE__);
			break;
		}
		if (!_io.start<decltype(io_event)>(thds, io_event))
		{
			WLOG("_io.start ʧ��:%d  %s %d\n", GetLastError(), __FILE__, __LINE__);
			break;
		}
		ret = true;
	} while (0);
	_active = ret;
	_check_t = std::thread([=]() {
		while (this->_active)
		{
			std::this_thread::sleep_for(std::chrono::seconds(2));
			this->_lock.lock();
			auto lks = _links;
			this->_lock.unlock();
			this->_close_lk.lock();
			auto clm = this->_close_lst;
			this->_close_lst.clear();
#ifdef _DEBUG
			this->_close_map.clear();
#endif // _DEBUG
			this->_close_lk.unlock();

			WLOG("UDP��ǰ������:%d  ���ڹر���:%d  .........\n", lks.size(), clm.size());
			for (auto &i : lks) {
				if (i.second->is_timeout()) {
					//WLOG("%p ��ʱ�ر�....\n",i.second);
					i.second->close();
				}
			}
			for (auto &i : clm) {
				wb_udp_link::operator delete(i,this->_mlp);
			}
		}

		});
	return ret;
}

void wb_udp_filter::stop() {
	//�����ر�����
	_active = false;
	_lock.lock();
	for (auto i: _links)
		i.second->close();
	_lock.unlock();

	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	if (_check_t.joinable()) {
		_check_t.join();
	}
	_io.stop();
	__super::stop();
	_mlp.stop();
	_mop.stop();
}

wb_udp_link* wb_udp_filter::get_link(const ustrt_net_base_info* pif, const ETH_PACK* pg)
{
	wb_udp_link* plink = nullptr;
	_lock.lock();
	auto fd = _links.find(*pif);
	if (fd != _links.end())
		plink = fd->second;
	if (nullptr == plink)
	{
		plink = new (_mlp) wb_udp_link(pg, pif);
#if 0
		//if (0x42cecd3c == pif->ip_info.dst_ip)
		{
			in_addr dia = {};
			dia.S_un.S_addr = pif->ip_info.dst_ip;
			in_addr sia = {};
			sia.S_un.S_addr = pif->ip_info.src_ip;

			std::string s_dip = inet_ntoa(dia);
			std::string s_sip = inet_ntoa(sia);
			WLOG("��uid����upd��[%p]:%s   %s  \n", plink, s_dip.c_str(), s_sip.c_str());
		}

#endif 
		bool bt = false;
		if (plink) {
			do {
				if (!plink->create_socket())
				{
					WLOG("udp create_socket ʧ��:%d\n", GetLastError());
					break;
				}
				if (!_io.relate(HANDLE((SOCKET)*plink), plink))
				{
					WLOG("udp relate ʧ��:%d\n", GetLastError());
					break;
				}
				if (!post_recv(plink, nullptr))
				{
					WLOG("udp post_recv ʧ��:%d\n", GetLastError());
					break;
				}
					
				bt = true;
			} while (0);
			if (bt) {
				_links[*pif] = plink;
			}
			else
			{
				wb_udp_link::operator delete(plink, _mlp);
				plink = nullptr;
			}
		}
	}
	_lock.unlock();
	return plink;
}

void wb_udp_filter::OnRead(const ETH_PACK* pk, int len)
{
	const ustrt_net_base_info* pif = (ustrt_net_base_info*)&pk->ip_h.uiSourIp;// ETH_PACK_TO_net_base_info(*pg);
	UINT uDestIp = UDP_BROADCAST_IP & pif->ip_info.dst_ip;
	if (UDP_BROADCAST_IP == uDestIp || pif->ip_info.dst_ip == 0)return;//�����㲥��
	if ((uDestIp & 0xF0000000) == MULTICAST_IP_MIN) return;//�����鲥��
#ifdef _DEBUG   //����ģʽ����ʱ����dns(8.8.8.8��114.114.114.114)�����ݰ�
	//if (0x42cecd3c != pif->ip_info.dst_ip)return;
#endif 
	//���ڷ�Ƭ�ĺ�������û��udpͷ���߱��˿���Ϣ�������޷�ȷ�ϰ��������ĸ����ӵ�
	//�Ƿ��Ƿ�Ƭ��
	auto fs = ntohs(pk->ip_h.sFragFlags);
	char MF = (fs >> 13) & 0x01;
	auto FO = (fs & 0x1FFF) / (1480 / 8);//��Ƭ������ֵ
	auto udp_len = get_udp_pack_date_len(pk);
	if (FO > 0 || (FO==0 && udp_len> c_udp_data_len_max))//MF=1 �� FO>0  ����ʹ�ò�����
	{
		//��Ƭ����
		FRAME_KEY fk = { pk->ip_h.sPackId,pif->ip_info.dst_ip,pif->ip_info.src_ip };
		auto ud = get_data_pack();
		if (FO == 0)
		{
			//��Ƭ������udpͷ
			ud->MF_Idx = 0;
			ud->usLen = get_udp_pack_date_len(pk);//udp��������
			ud->usPackLen = get_ip_pack_len(pk) - (c_ip_head_len + c_udp_head_len);
			memcpy(ud->data, get_udp_pack_date(pk), ud->usPackLen);
		}
		else
		{
			//������Ƭ������udpͷ
			ud->MF_Idx = FO;
			ud->usLen = 0;//����Ƭ�����ȱ����ʼ��
			ud->usPackLen = get_ip_pack_len(pk) - c_ip_head_len;
			memcpy(ud->data, get_ip_pack_date(pk), ud->usPackLen);
		}
		frame_info* lst = nullptr;
		_f_lock.lock();
		auto find = _fd.find(fk);
		if (find != _fd.end())
			lst = _fd[fk];
		if (!lst)
		{
			lst = new frame_info();
			if (lst)
			{
				_fd[fk] = lst;
			}
		}
		_f_lock.unlock();
		if (!lst)return;
		//����Ƭ��Ϣ
		lst->lk.lock();
		if (FO == 0)
		{
			lst->nbi = *pif;
		}
		if (lst->fl.size() < 1) {
			lst->fl.push_back(ud);
			lst->lk.unlock();
			return;
		}
		int ud_all_len = ud->usLen;//ud��������
		int ud_tmp_len = ud->usPackLen;//ud��ǰ���з�Ƭ�ĳ���
		std::vector<wb_data_pack*> dp_lst;// ��������
		for (auto& i : lst->fl)
		{
			ud_tmp_len += i->usPackLen;
			if (i->usLen > 0) {
				assert(ud_all_len == 0);
				ud_all_len = i->usLen;
			}
			dp_lst.push_back(i);
		}
		if (ud_all_len <= 0 || ud_all_len != ud_tmp_len)
		{
			lst->fl.push_back(ud);
			lst->lk.unlock();
			return ;
		}
		//����������
		auto nbi = lst->nbi;
		lst->fl.clear();
		lst->lk.unlock();
		delete lst;
		_f_lock.lock();
		_fd.erase(fk);
		_f_lock.unlock();
		auto lk = get_link(&nbi, pk);
		if (!lk)return;
		//����Ƭ��������
		dp_lst.push_back(ud);
		std::sort(dp_lst.begin(), dp_lst.end(), [=](const wb_data_pack* d1, const wb_data_pack* d2)->bool {
			return d1->MF_Idx < d2->MF_Idx;
			});

		if (ud_all_len > UDP_RECV_SIZE)//���
		{
			char* send_buf = new char[ud_all_len];
			int add_len = 0;
			for (auto& i : dp_lst) {
				memcpy(send_buf + add_len, i->data, i->usPackLen);
				add_len += i->usPackLen;
				back_data_pack(i);//��Դ�黹
			}
			assert(add_len == ud_all_len);
			WLOG("post_send_no_copy:%p %d\n", send_buf, ud_all_len);
			if (!post_send_no_copy(lk, send_buf, ud_all_len))
				delete[]send_buf;
		}
		else
		{
			char send_buf[UDP_RECV_SIZE];
			int add_len = 0;
			for (auto& i : dp_lst) {
				memcpy(send_buf + add_len, i->data, i->usPackLen);
				add_len += i->usPackLen;
				back_data_pack(i);//��Դ�黹
			}
			post_send(lk, send_buf, ud_all_len);
		}
	}
	else
	{
		auto lk = get_link(pif, pk);
		if (lk) {
			lk->OnRead(this, pk, len);
		}
	}
}

bool wb_udp_filter::post_recv(wb_link_interface* plink, LPOVERLAPPED pol)
{
	WB_OVERLAPPED_UDP* pu = (WB_OVERLAPPED_UDP*)pol;
	if(!pu)
		pu = (WB_OVERLAPPED_UDP*)_mop.get_mem();
	if (pu)
	{
		pu->wbuf.buf = pu->buf;
		pu->wbuf.len = UDP_RECV_SIZE;
		pu->ot = udp_io_oper_type::e_recv;
		pu->p_lk = (wb_udp_link*)plink;
		pu->addr_len = sizeof(sockaddr_in);
		if (!plink->Recv(&pu->wbuf, pu->dwFlag, (sockaddr*)&pu->addr, &pu->addr_len, &pu->ol)) {
			//WLOG("wb_udp_filter::post_recv close_link %p\n", plink);
			//close_link(pu->p_lk);
			_mop.recover_mem(pu);
			return false;
		}
	}
	return true;
}

bool wb_udp_filter::post_send(wb_link_interface* plink, const char* buf, int len)
{
	WB_OVERLAPPED_UDP* pu = (WB_OVERLAPPED_UDP * )_mop.get_mem();
	if (pu)
	{
		pu->wbuf.buf = pu->buf;
		pu->wbuf.len = len;
		memcpy(pu->buf, buf, len);
		pu->ot = udp_io_oper_type::e_send;
		pu->p_lk =(wb_udp_link *) plink;
		if (!plink->Send(&pu->wbuf, &pu->ol))
		{
			_mop.recover_mem(pu);
			return false;
		}
	}
	return true;
}
bool wb_udp_filter::post_send_no_copy(wb_link_interface* plink, char* buf, int len)
{
	WB_OVERLAPPED_UDP* pu = (WB_OVERLAPPED_UDP*)_mop.get_mem();
	if (pu)
	{
		pu->wbuf.buf = buf;
		pu->wbuf.len = len;
		pu->ot = udp_io_oper_type::e_send_no_copy;
		pu->p_lk = (wb_udp_link*)plink;
		if (!plink->Send(&pu->wbuf, &pu->ol))
		{
			_mop.recover_mem(pu);
			return false;
		}
	}
	return true;
}

//icmp
//��д����ʹ��
struct icmp_info
{
	IP_HEADER		ip_h;
	ICMP_HEADER		icmp_h;
};
struct WB_OVERLAPPED_ICMP
{
	OVERLAPPED			ol;
	udp_io_oper_type	ot;
	WSABUF				wbuf;
	char				buf[ICMP_RECV_SIZE];
	DWORD				dwFlag;
	sockaddr_in			addr;
	int					addr_len;
	wb_icmp_link*		p_lk;
};

bool wb_icmp_filter::start(int thds)
{
	thds = 5;
	_r_sock = WSASocket(AF_INET, SOCK_RAW, IPPROTO_ICMP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	assert(_r_sock != INVALID_SOCKET);
	if (INVALID_SOCKET == _r_sock)return false;
	auto io_event = [=](DWORD numberOfBytes, ULONG_PTR ComKey, LPOVERLAPPED pol)->bool {
		WB_OVERLAPPED_ICMP* pl = (WB_OVERLAPPED_ICMP*)pol;
		if (pl)
		{
			switch (pl->ot)
			{
			case udp_io_oper_type::e_send:this->DoOnSend(pl->p_lk, pl->buf, numberOfBytes, pol); break;
			case udp_io_oper_type::e_recv:this->DoOnRecv(pl->p_lk, pl->buf, numberOfBytes, pol); break;
			}
		}
		return true;
	};
	stop();
	auto ret = false;
	do
	{	
		if (!_mlp.start(sizeof(wb_icmp_link), 256))break;
		if (!_mop.start(sizeof(WB_OVERLAPPED_ICMP), 2048))break;
		if (!_io.start<decltype(io_event)>(3, io_event))break;
		sockaddr_in addr = {};
		addr.sin_family = AF_INET;
		addr.sin_port = 0;
		addr.sin_addr.S_un.S_addr = INADDR_ANY;
		if (SOCKET_ERROR == bind(_r_sock, (sockaddr*)&addr, sizeof(sockaddr_in))) break;
		if (!_io.relate(HANDLE(_r_sock), this))break;
		while (thds--)
		{
			if (!post_recv(nullptr, nullptr))return false ;
		}
		ret = true;
	} while (0);
	assert(ret);
	return ret;
}

void wb_icmp_filter::stop()
{
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	_io.stop();
	_mop.stop();
	_mlp.stop();
}

void wb_icmp_filter::OnRead(const ETH_PACK* pg, int len)
{
	if (pg->icmp_h.cType != 8)return;//��ping��������˿ڲ��ɴ�Ҳ���ڴ���Χ��
	ustrt_net_base_info pif = *(ustrt_net_base_info*)&pg->ip_h.uiSourIp;
	wb_icmp_link* plink = nullptr;
	_lock.lock();
	auto fd = _links.find(pif.big_info.ip);
	if (fd != _links.end())
		plink = fd->second;
	if (nullptr == plink)
	{
		plink = new (_mlp) wb_icmp_link(pg);
		if (plink) {
			_links[pif.big_info.ip] = plink;
		}
	}
	_lock.unlock();
	if (plink)
	{
		_ic_lock.lock();
		_d_links.erase((const ICMP_KEY &) *plink);//ɾ��ǰ����
		_ic_lock.unlock();
		plink->OnRead(this, pg, len);
	}

}

void wb_icmp_filter::DoOnRecv(wb_icmp_link* plink, const char* buffer, int len, LPOVERLAPPED pol) {

	PICMP_DATA pc = (PICMP_DATA)buffer;
	ICMP_KEY icmp_key = { pc->ip_h.uiSourIp,pc->icmp_h.usSequence };
	wb_icmp_link* pk = nullptr;
	_lock.lock();
	auto fd = _d_links.find(icmp_key);
	if (fd != _d_links.end())
		pk = _d_links[icmp_key];
	_d_links.erase(icmp_key);
	_lock.unlock();
	if(pk)
		pk->OnRecv(this, buffer, len, pol);
	post_recv(nullptr, pol);
}

bool wb_icmp_filter::post_recv(wb_link_interface* plink, LPOVERLAPPED pol)
{
	WB_OVERLAPPED_ICMP* pu = (WB_OVERLAPPED_ICMP*)pol;
	if (!pu)
	{
		pu = (WB_OVERLAPPED_ICMP*)_mop.get_mem();
	}
	memset(pu, 0, sizeof(WB_OVERLAPPED_ICMP));
	pu->dwFlag = MSG_PARTIAL;
	pu->wbuf.buf = pu->buf;
	pu->wbuf.len = ICMP_RECV_SIZE;
	pu->ot = udp_io_oper_type::e_recv;
	pu->p_lk = (wb_icmp_link*)plink;
	pu->addr_len = sizeof(sockaddr_in);
	if (SOCKET_ERROR == WSARecvFrom(_r_sock, &pu->wbuf, 1, nullptr, &pu->dwFlag, (sockaddr*)&pu->addr, &pu->addr_len, &pu->ol, nullptr)
		&& GetLastError() != ERROR_IO_PENDING)
	{
		return false;
	}
	return true;
}

bool wb_icmp_filter::post_send(wb_link_interface* plink, const char* buf, int len) 
{
	WB_OVERLAPPED_ICMP* pu = (WB_OVERLAPPED_ICMP*)_mop.get_mem();
	memset(pu, 0, sizeof(WB_OVERLAPPED_ICMP));
	if (pu)
	{
		pu->wbuf.buf = pu->buf;
		pu->wbuf.len = len;
		memcpy(pu->buf, buf, len);
		pu->ot = udp_io_oper_type::e_send;
		pu->p_lk = (wb_icmp_link*)plink;
		if (SOCKET_ERROR == WSASendTo(_r_sock, &pu->wbuf, 1, nullptr, 0, pu->p_lk->get_addr(), sizeof(sockaddr_in), &pu->ol, nullptr)
			&& GetLastError() != ERROR_IO_PENDING)
		{
			return false;
		}
		return true;
	}
	
	return true;
}

//tcp========================================================================================

bool wb_tcp_filter::start(int thds)
{
	auto io_connecting = [=](DWORD numberOfBytes, ULONG_PTR ComKey, LPOVERLAPPED pol) ->bool {
		this->DoOnConnect((wb_tcp_link*)ComKey, pol);
		return true;
	};
	auto io_net_work = [=](DWORD numberOfBytes, ULONG_PTR ComKey, LPOVERLAPPED pol) ->bool {
		auto pl = (WB_TCP_OVERLAPPED*)pol;
		switch (pl->op) {
		case tcp_operator::e_send:this->DoOnSend((wb_tcp_link*)ComKey, pl->buf, numberOfBytes, pl); break;
		case tcp_operator::e_recv:this->DoOnRecv((wb_tcp_link*)ComKey, pl->buf, numberOfBytes, pl); break;
		}
		return true;
	};
	_active = false;
	do
	{
		if (!_mlp.start(sizeof(wb_tcp_link), 1024 * 5))break;
		if (!_mop.start(sizeof(WB_TCP_OVERLAPPED), 1024 * 10))break;
		if (!_mcp.start(sizeof(OVERLAPPED), 4096))break;
		if (!_mdp.start(sizeof(wb_data_pack_ex), 40960))break;
		if (!_conn_io.start<decltype(io_connecting)>(50, io_connecting))break;
		if (!_io.start<decltype(io_net_work)>(3, io_net_work))break;
		_active = true;
	} while (0);
	//return _active;
	std::thread t = std::thread([=]() {

		while (this->_active)
		{
			std::this_thread::sleep_for(std::chrono::seconds(5));
			this->_lock.lock();
			auto que = this->_links;
			this->_lock.unlock();
			_fin_lock.lock();
			auto clo_que = _fin_links;
			_fin_lock.unlock();
			RLOG("��ǰ������:%d  ���ڹر���:%d\n", que.size(), clo_que.size());
			for (auto i : que)
			{
				assert(i.second);
				if (i.second->is_timeout())
					i.second->close();

			}
			auto now_tm = time(0);
			for (auto i : clo_que)
			{
				if (now_tm - i.second->close_tm() > 2) {
					_fin_lock.lock();
					_fin_links.erase((const ustrt_net_base_info&)*i.second);
					_fin_lock.unlock();
					wb_tcp_link::operator delete(i.second, _mlp);
				}
			}
		}
	});
	t.detach();

	return _active;
}

void wb_tcp_filter::close_link(wb_tcp_link* cl,char cFlag)
{
	auto nbi = (const ustrt_net_base_info&)*cl;
	_lock.lock();
	_links.erase(nbi);
	_lock.unlock();
	//ִ�жϿ�

	cl->DoClose(this, cFlag);
	{
		_fin_lock.lock();
		_fin_links[nbi] = cl;
		_fin_lock.unlock();
	}	

}

void wb_tcp_filter::DoOnConnect(wb_tcp_link* cl, LPOVERLAPPED pol)
{
	_mcp.recover_mem(pol);
	auto nbi = (const ustrt_net_base_info&)*cl;
	auto ret = cl->connect();
	if (ret) {

		bool b_ok = false;
		do
		{
			if (!_io.relate((HANDLE)((SOCKET)*cl), cl))break;
			if (!post_recv(cl, nullptr))break;
			b_ok = true;

		} while (0);
		if(!b_ok)
		{
			cl->close();
			wb_tcp_link::operator delete(cl, _mlp);
		}
		else
		{
			_lock.lock();
			assert(cl);
			_links[nbi] = cl;
			_lock.unlock();
			cl->OnConnected(this);
		}
	}
	else
	{
		cl->close();
		wb_tcp_link::operator delete(cl, _mlp);
	}
	_con_lock.lock();
	_con_link.erase(nbi);
	_con_lock.unlock();
}

void wb_tcp_filter::write_rst_pack(const ETH_PACK* pg)
{
	return;
	/*
	//1.tcpͷ,4λ�ײ�����
	ETH_PACK pk = {};
	memcpy(pk.DstAddr, pg->SrcAddr, MAC_ADDR_LEN);
	memcpy(pk.SrcAddr, pg->DstAddr, MAC_ADDR_LEN);
	pk.EthType = IP_FLAG;
	//2.ipͷ
	pk.ip_h.cVersionAndHeaderLen = IP_VER_LEN;
	//_m_ip_h.cTypeOfService = 0;
	//_ip_h->sTotalLenOfPack = xx ���ʱ��д;
	//_m_ip_h.sPackId = 0;//�����ʱ�����ֵ����
	//_m_ip_h.sFragFlags = 0;//��Ƭ
	pk.ip_h.cTTL = DEFAULT_TTL;
	pk.ip_h.cTypeOfProtocol = PRO_TCP;
	pk.ip_h.uiDestIp = pg->ip_h.uiSourIp;// nbi.ip_info.src_ip;// pBni->m_SrcIP;
	pk.ip_h.uiSourIp = pg->ip_h.uiDestIp;// nbi.ip_info.dst_ip;// pBni->m_DestI
	*/
}
void wb_tcp_filter::OnRead(const ETH_PACK* pg, int len)
{
	// cFlagλ  URG ACK PSH RST SYN FIN
	//if (pg->ip_h.uiDestIp != 0x42cecd3c)return;//���Խ׶�����ip��tcp������
	ustrt_net_base_info & nbi = *(ustrt_net_base_info*)&pg->ip_h.uiSourIp;
	auto t_flag = TCP_FLAG(pg->tcp_h.cFlag);
	if (t_flag == TCP_SYN1)//����������,������ھɵ����ӣ��ȹرվ�����
	{
		add_connect_link(pg, nbi);
	}
	else
	{
		auto plink = get_link(nbi);
		if (!plink)
		{
			if (!TCP_FLAG_RST(t_flag) && !TCP_FLAG_FIN(t_flag))
			{
				write_rst_pack(pg);		
			}
			else
			{
				/*
				_fin_lock.lock();
				auto ffd = _fin_links.find(nbi);
				if(ffd!= _fin_links.end())
					plink = ffd->second;
				_fin_lock.unlock();
				if (plink)
					close_link(plink, TCP_FLAG_FIN(t_flag));
				*/
			}
			return;
		}
		if (TCP_FLAG_RST(t_flag) || TCP_FLAG_FIN(t_flag))//�ر�����
		{
			//WLOG("tcp close link %p %s  %d\n", plink,__FILE__,__LINE__);
			close_link(plink, t_flag);
		}

		else 
			plink->OnRead(this, pg, len);//���������ʱ��S�˶Ͽ������ӣ�������ִ�н������
	}
}

void wb_tcp_filter::DoOnRecv(wb_tcp_link* plink, const char* buffer, int len, WB_TCP_OVERLAPPED* pol) {
	if (len > 0)
	{
		plink->OnRecv(this, buffer, len, &pol->ol);
		post_recv(plink, &pol->ol);
	}
	else
	{
		//WLOG("tcp close link %p %s  %d\n", plink, __FILE__, __LINE__);
		close_link(plink,0);
		_mop.recover_mem(pol);
	}
};


wb_tcp_link* wb_tcp_filter::add_connect_link(const ETH_PACK* pg ,const ustrt_net_base_info& nbi)
{
	wb_tcp_link* c_link = nullptr;
	auto b_exists = false;
	_con_lock.lock();
	auto fd = _con_link.find(nbi);
	if (fd != _con_link.end())
	{
		b_exists = true;
		c_link = fd->second;
	}
	else
	{
		c_link = new (_mlp) wb_tcp_link(pg);
	}
	_con_lock.unlock();
	if(!b_exists){
	
		
		if (!c_link) {
			RLOG("TCP���ӳ���Դ����....................\n");
			return nullptr;
		}
		if (!c_link->create_socket())
		{
			WLOG("���ӣ�%p create socket ʧ��:%d\n", c_link,GetLastError());
			wb_tcp_link::operator delete (c_link, _mlp);
			return nullptr;
		}
		_con_lock.lock();
		_con_link[nbi] = c_link;
		_con_lock.unlock();
		if (!_conn_io.post((ULONG_PTR)c_link, 0, (OVERLAPPED*)_mcp.get_mem()))
		{
			_con_lock.lock();
			_con_link.erase(nbi);
			_con_lock.unlock();
			wb_tcp_link::operator delete (c_link, _mlp);
			return nullptr;
		}
	}
	
	return c_link;
}

wb_tcp_link* wb_tcp_filter::get_link(const ustrt_net_base_info& nbi)
{
	wb_tcp_link* tcp_link = nullptr;
	_lock.lock();
	auto fd = _links.find(nbi);
	if(fd!= _links.end())
		tcp_link = fd->second;
	_lock.unlock();
	return tcp_link;
}

bool wb_tcp_filter::post_send(wb_link_interface* plink, const char* buf, int len) {

	auto pl = (WB_TCP_OVERLAPPED*)_mop.get_mem();
	if (pl) {

		pl->op = tcp_operator::e_send;
		pl->wbuf.buf = pl->buf;
		pl->wbuf.len = len;
		memcpy(pl->buf, buf, len);
		pl->p_lk = (wb_tcp_link*)plink;

		if (!plink->Send(&pl->wbuf, &pl->ol)) {
			//close_link(pl->p_lk);
			_mop.recover_mem(pl);
			return false;
		}
	}
	return true;
}

bool wb_tcp_filter::post_recv(wb_link_interface* plink, LPOVERLAPPED pol) {

	auto pl = (WB_TCP_OVERLAPPED*)pol;
	if (!pl)
		pl = (WB_TCP_OVERLAPPED*)_mop.get_mem();
	if (pl) {

		pl->op = tcp_operator::e_recv;
		pl->wbuf.buf = pl->buf;
		pl->wbuf.len = TCP_RECV_SIZE;
		pl->p_lk = (wb_tcp_link*)plink;
		if (!plink->Recv(&pl->wbuf, pl->dwFlag, nullptr, nullptr, &pl->ol))
		{
			//WLOG("tcp close link %p %s  %d\n", plink, __FILE__, __LINE__);
			close_link(pl->p_lk,0);
			_mop.recover_mem(pl);
			return false;
		}
	}
	return true;
};