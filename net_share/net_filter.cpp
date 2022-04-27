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
			case udp_io_oper_type::e_send:this->DoOnSend(pl->buf, numberOfBytes, pl);break;
			case udp_io_oper_type::e_recv:this->DoOnRecv(pl->buf, numberOfBytes, pl); break;
			case udp_io_oper_type::e_send_no_copy:this->DoOnSend_No_Copy(pl->wbuf.buf, numberOfBytes, pl); break;
			}
		}
		return true;
	};
	auto ret = false;
	do
	{
		if (!__super::start(thds))
		{
			WLOG("_mlp.start 失败:%d  %s %d\n", GetLastError(), __FILE__, __LINE__);
			break;
		}
		if (!_mlp.start(sizeof(wb_udp_link), 1024*10))
		{
			WLOG("_mlp.start 失败:%d  %s %d\n",GetLastError() , __FILE__,__LINE__);
			break;
		}
		if(!_mop.start(sizeof(WB_OVERLAPPED_UDP), 1024*10)) 
		{
			WLOG("_mop.start 失败:%d  %s %d\n", GetLastError(), __FILE__, __LINE__);
			break;
		}
		if (!_io.start<decltype(io_event)>(thds, io_event))
		{
			WLOG("_io.start 失败:%d  %s %d\n", GetLastError(), __FILE__, __LINE__);
			break;
		}
		ret = true;
	} while (0);
	_active = ret;
	_check_t = std::thread([=]() {
		while (this->_active)
		{
			std::this_thread::sleep_for(std::chrono::seconds(2));
			//continue;
			this->_lock.lock();
			auto lks = _links;
			this->_lock.unlock();

			//WLOG("UDP当前连接数:%d  .........\n", lks.size());
			for (auto &i : lks) {
				//WLOG("连接%p:引用数:%d\n" , i.second.get() , *i.second.use_cout() );
				if (i.second->is_timeout()) {
					//WLOG("连接%p:超时\n", i.second.get());
					i.second->close();
				}
			}
		}

		});
	return ret;
}

void wb_udp_filter::stop() {
	//遍历关闭连接
	_active = false;
	//std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	if (_check_t.joinable()) {
		_check_t.join();
	}
	_io.stop();

	_lock.lock();
	//for (auto i : _links)
		//i.second->close();
	_links.clear();
	_lock.unlock();

	__super::stop();
	_mlp.stop();
	_mop.stop();
}

wb_udp_filter::auto_udp_link wb_udp_filter::get_link(const ustrt_net_base_info* pif, const ETH_PACK* pg)
{
	auto_udp_link plink ;
	_lock.lock();
	auto fd = _links.find(*pif);
	if (fd != _links.end())
		plink = fd->second;
	if (nullptr == plink)
	{
		plink = auto_udp_link(new (_mlp) wb_udp_link(pg, pif),&_mlp);
#if 0
		//if (0x42cecd3c == pif->ip_info.dst_ip)
		{
			in_addr dia = {};
			dia.S_un.S_addr = pif->ip_info.dst_ip;
			in_addr sia = {};
			sia.S_un.S_addr = pif->ip_info.src_ip;

			std::string s_dip = inet_ntoa(dia);
			std::string s_sip = inet_ntoa(sia);
			WLOG("新uid连接upd包[%p]:%s   %s  \n", plink.get(), s_dip.c_str(), s_sip.c_str());
		}

#endif 
		bool bt = false;
		if (plink) 
		{
			do {
				//this->OnNewLink(plink);
				if (!plink->create_socket())
				{
					WLOG("udp create_socket 失败:%d\n", GetLastError());
					break;
				}
				auto skt = (SOCKET)*plink.get();
				if (!_io.relate(HANDLE(skt), nullptr))
				{
					WLOG("udp relate 失败:%d\n", GetLastError());
					break;
				}

				if (!post_recv(plink,&plink, nullptr))
				{
					WLOG("udp post_recv 失败:%d\n", GetLastError());
					break;
				}
					
				bt = true;
			} while (0);
			if (bt) {
				//新UDP连接
				_links[*pif] = plink;
			}
			else
			{
				//OnCloseLink(plink);
				plink = auto_udp_link(nullptr,nullptr);
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
	if (UDP_BROADCAST_IP == uDestIp || pif->ip_info.dst_ip == 0)return;//丢弃广播包
	if ((uDestIp & 0xF0000000) == MULTICAST_IP_MIN) return;//丢弃组播包
#ifdef _DEBUG   //调试模式下暂时屏蔽dns(8.8.8.8和114.114.114.114)的数据包
	//if (0x42cecd3c != pif->ip_info.dst_ip)return;
#endif 
	//由于分片的后续包，没有udp头不具备端口信息，所以无法确认包是属于哪个连接的
	//是否是分片包
	_read_packs++;
	auto fs = ntohs(pk->ip_h.sFragFlags);
	char MF = (fs >> 13) & 0x01;
	auto FO = (fs & 0x1FFF) / (1480 / 8);//分片的索引值
	auto udp_len = get_udp_pack_date_len(pk);
	if (FO > 0 || (FO==0 && udp_len> c_udp_data_len_max))//MF=1 或 FO>0  所以使用并运算
	{
		//分片处理
		FRAME_KEY fk = { pk->ip_h.sPackId,pif->ip_info.dst_ip,pif->ip_info.src_ip };
		auto ud = get_data_pack();
		if (FO == 0)
		{
			//首片，含有udp头
			ud->MF_Idx = 0;
			ud->usLen = get_udp_pack_date_len(pk);//udp整包长度
			ud->usPackLen = get_ip_pack_len(pk) - (c_ip_head_len + c_udp_head_len);
			memcpy(ud->data, get_udp_pack_date(pk), ud->usPackLen);
		}
		else
		{
			//后续分片，不含udp头
			ud->MF_Idx = FO;
			ud->usLen = 0;//非首片，长度必须初始化
			ud->usPackLen = get_ip_pack_len(pk) - c_ip_head_len;
			memcpy(ud->data, get_ip_pack_date(pk), ud->usPackLen);
		}
		frame_info* lst = nullptr;
		_f_lock.lock();
		auto find = _fd.find(fk);
		if (find != _fd.end())
			lst = find->second;
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
		//看分片信息
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
		int ud_all_len = ud->usLen;//ud整包长度
		int ud_tmp_len = ud->usPackLen;//ud当前所有分片的长度
		std::vector<wb_data_pack*> dp_lst;// 拷贝数据
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
		//可以重组了
		auto nbi = lst->nbi;
		lst->fl.clear();
		lst->lk.unlock();
		delete lst;
		_f_lock.lock();
		_fd.erase(fk);
		_f_lock.unlock();
		auto lk = get_link(&nbi, pk);
		if (!lk)return;
		//按分片索引排序
		dp_lst.push_back(ud);
		std::sort(dp_lst.begin(), dp_lst.end(), [=](const wb_data_pack* d1, const wb_data_pack* d2)->bool {
			return d1->MF_Idx < d2->MF_Idx;
			});

		if (ud_all_len > UDP_RECV_SIZE)//大包
		{
			char* send_buf = new char[ud_all_len];
			int add_len = 0;
			for (auto& i : dp_lst) {
				memcpy(send_buf + add_len, i->data, i->usPackLen);
				add_len += i->usPackLen;
				back_data_pack(i);//资源归还
			}
			assert(add_len == ud_all_len);
			WLOG("post_send_no_copy:%p %d\n", send_buf, ud_all_len);
			if (!post_send_no_copy(lk,&lk, send_buf, ud_all_len))
				delete[]send_buf;
		}
		else
		{
			char send_buf[UDP_RECV_SIZE];
			int add_len = 0;
			for (auto& i : dp_lst) {
				memcpy(send_buf + add_len, i->data, i->usPackLen);
				add_len += i->usPackLen;
				back_data_pack(i);//资源归还
			}
			post_send(lk,&lk, send_buf, ud_all_len);
		}
	}
	else
	{
		auto lk = get_link(pif, pk);
		if (lk) {
			lk->OnRead(this,&lk, pk, len);
		}
	}
}

bool wb_udp_filter::post_recv(wb_link_interface* plink, void* const lp_link, LPOVERLAPPED pol)
{
	WB_OVERLAPPED_UDP* pu = (WB_OVERLAPPED_UDP*)pol;
	char del_flag = 0;
	if (!pu)
	{
		pu = (WB_OVERLAPPED_UDP*)_mop.get_mem();
		del_flag = 1;
	}

	if (pu)
	{
		pu->wbuf.buf = pu->buf;
		pu->wbuf.len = UDP_RECV_SIZE;
		pu->ot = udp_io_oper_type::e_recv;
		pu->p_lk = *((auto_udp_link*)lp_link);
		pu->addr_len = sizeof(sockaddr_in);
		if (!plink->Recv(&pu->wbuf, pu->dwFlag, (sockaddr*)&pu->addr, &pu->addr_len, &pu->ol)) {
			if(del_flag)
				WB_OVERLAPPED_UDP::operator delete(pu, _mop);// _mop.recover_mem(pu);
			return false;
		}
	}
	return true;
}

bool wb_udp_filter::post_send(wb_link_interface* plink, void* const lp_link, const char* buf, int len)
{
	WB_OVERLAPPED_UDP* pu = (WB_OVERLAPPED_UDP * )_mop.get_mem();
	if (pu)
	{
		pu->wbuf.buf = pu->buf;
		pu->wbuf.len = len;
		memcpy(pu->buf, buf, len);
		pu->ot = udp_io_oper_type::e_send;
		pu->p_lk = *((auto_udp_link*)lp_link);
		if (!plink->Send(&pu->wbuf, &pu->ol))
		{
			WB_OVERLAPPED_UDP::operator delete(pu, _mop);//_mop.recover_mem(pu);
			return false;
		}
	}
	return true;
}
bool wb_udp_filter::post_send_no_copy(wb_link_interface* plink, void* const lp_link, char* buf, int len)
{
	WB_OVERLAPPED_UDP* pu = (WB_OVERLAPPED_UDP*)_mop.get_mem();
	if (pu)
	{
		pu->wbuf.buf = buf;
		pu->wbuf.len = len;
		pu->ot = udp_io_oper_type::e_send_no_copy;
		pu->p_lk = *((auto_udp_link*)lp_link);
		if (!plink->Send(&pu->wbuf, &pu->ol))
		{
			WB_OVERLAPPED_UDP::operator delete(pu, _mop);//_mop.recover_mem(pu);
			return false;
		}
	}
	return true;
}

//icmp
//读写网卡使用

struct icmp_info
{
	IP_HEADER		ip_h;
	ICMP_HEADER		icmp_h;
};

bool wb_icmp_filter::start(int thds)
{
	stop();
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
			case udp_io_oper_type::e_send:this->DoOnSend(pl->buf, numberOfBytes, pl); break;
			case udp_io_oper_type::e_recv:this->DoOnRecv(pl->buf, numberOfBytes, pl); break;
			}
		}
		return true;
	};

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
			if (!post_recv(nullptr,nullptr, nullptr))return false ;
		}
		ret = true;
	} while (0);
	assert(ret);
	std::thread t = std::thread([=]() {
		while (_r_sock != INVALID_SOCKET)
		{
			this->_lock.lock();
			auto lks =  this->_links;
			this->_lock.unlock();
			for (auto & i : lks)
			{
				if (i.second->is_timeout()) {
					WLOG("ICMP 超时:%p\n", i.second.get());
					this->_lock.lock();
					this->_links.erase(i.first);
					this->_lock.unlock();
				}
			}
			this->_ic_lock.lock();
			auto d_lks = this->_d_links;
			this->_ic_lock.unlock();
			auto now_tm = time(0);
			for (auto& i : d_lks)
			{
				if (now_tm - i.first.tm>3) {
					WLOG("ICMP 包超时:%p [%I64d  %d  %d]\n", i.second.get(),i.first.tm,i.first.detail.usSequence ,i.first.detail.usID);
					this->_ic_lock.lock();
					this->_d_links.erase(i.first);
					this->_ic_lock.unlock();
				}
			}
			std::this_thread::sleep_for(std::chrono::seconds(3));
		}
		});
	t.detach();
	return ret;
}

void wb_icmp_filter::stop()
{
	closesocket(_r_sock);
	_r_sock = INVALID_SOCKET;
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	_io.stop();
	_mop.stop();
	_mlp.stop();
}

void wb_icmp_filter::OnRead(const ETH_PACK* pg, int len)
{
	if (pg->icmp_h.cType != 8)return;//非ping命令不处理，端口不可达也不在处理范围内
	ustrt_net_base_info pif = *(ustrt_net_base_info*)&pg->ip_h.uiSourIp;
	//WLOG("icmp:%llx, ip_len=%d pid=%d  usID = %d   usSeq=%d  data=%s   \n", 
			//pif.big_info.ip, (int)ntohs(pg->ip_h.sTotalLenOfPack), (INT)ntohs(pg->ip_h.sPackId), (UINT)pg->icmp_h.usID,(UINT)ntohs(pg->icmp_h.usSequence),pg->icmp_h.cData);
	auto_icmp_link plink ;
	ICMP_LINK_KEY lk_key = { pg->icmp_h.usID ,pif.big_info.ip };
	_lock.lock();
	auto fd = _links.find(lk_key);
	if (fd != _links.end())
		plink = fd->second;
	if (!plink)
	{
		plink = auto_icmp_link(new (_mlp) wb_icmp_link(pg) ,&_mlp);
		if (plink) {
			_links[lk_key] = plink;
		}
	}
	_lock.unlock();
	if (plink)
	{
		plink->OnRead(this,&plink, pg, len);
	}
}

void wb_icmp_filter::DoOnRecv(const char* buffer, int len, WB_OVERLAPPED_ICMP* pol) {

	PICMP_DATA pc = (PICMP_DATA)buffer;
	ICMP_PACK_KEY icmp_key = { 0,pc->ip_h.uiSourIp,pc->icmp_h.usSequence};
	icmp_key.detail.usID = pc->icmp_h.usID;
	auto_icmp_link pk;
	_lock.lock();
	auto fd = _d_links.find(icmp_key);
	if (fd != _d_links.end())
		pk = fd->second;// _d_links[icmp_key];
	_d_links.erase(icmp_key);
	_lock.unlock();
	if(pk)
		pk->OnRecv(this,&pk, buffer, len, &pol->ol);
	post_recv(nullptr,nullptr, &pol->ol);
}

bool wb_icmp_filter::post_recv(wb_link_interface* plink, void* const lp_link, LPOVERLAPPED pol)
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
	//pu->p_lk = (wb_icmp_link*)plink;
	pu->addr_len = sizeof(sockaddr_in);
	if (SOCKET_ERROR == WSARecvFrom(_r_sock, &pu->wbuf, 1, nullptr, &pu->dwFlag, (sockaddr*)&pu->addr, &pu->addr_len, &pu->ol, nullptr)
		&& GetLastError() != ERROR_IO_PENDING)
	{
		return false;
	}
	return true;
}
bool wb_icmp_filter::post_send_ex(wb_link_interface* plink, void* const lp_link, const char* buf, int len, void* pd)
{
	WB_OVERLAPPED_ICMP* pu = new (_mop) WB_OVERLAPPED_ICMP;
	if (pu)
	{
		pu->wbuf.buf = pu->buf;
		pu->wbuf.len = len;
		memcpy(pu->buf, buf, len);
		pu->ot = udp_io_oper_type::e_send;
		pu->p_lk = *((auto_icmp_link*)lp_link);
		_ic_lock.lock();
		_d_links[*(ICMP_PACK_KEY*)pd] = pu->p_lk; //投递前就及时添加
		_ic_lock.unlock();

		if (SOCKET_ERROR == WSASendTo(_r_sock, &pu->wbuf, 1, nullptr, 0, pu->p_lk->get_addr(), sizeof(sockaddr_in), &pu->ol, nullptr)
			&& GetLastError() != ERROR_IO_PENDING)
		{
			_ic_lock.lock();
			_d_links.erase(*(ICMP_PACK_KEY*)pd);//如果失败再删除
			_ic_lock.unlock();
			WB_OVERLAPPED_ICMP::operator delete(pu, _mop);
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
		case tcp_operator::e_send:this->DoOnSend(pl->buf, numberOfBytes, pl); break;
		case tcp_operator::e_recv:this->DoOnRecv(pl->buf, numberOfBytes, pl); break;
		case tcp_operator::e_send_no_copy:this->DoOnSend_No_Copy(pl->buf, numberOfBytes, pl); break;
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
	_check_thread = std::thread([=]() {
		while (this->_active)
		{
			this->_lock.lock();
			auto que = this->_links;
			this->_lock.unlock();
			for (auto i : que)
			{
				if (i.second->is_timeout())
				{
					WLOG("tcp:%p 超时关闭:%d \n", i.second.get(), i.second->Ref());
					i.second->close();
				}
			}
			std::this_thread::sleep_for(std::chrono::seconds(3));
		}
	});
	//t.detach();
	return _active;
}

void wb_tcp_filter::close_link(auto_tcp_link& cl,char cFlag)
{
	auto nbi = (const ustrt_net_base_info&)*cl.get();
		
	_lock.lock();
	auto it = _links.erase(nbi);
	_lock.unlock();
	//WLOG("tcp close:%p   \n",cl.get());
	if(it>0)
		OnCloseLink(cl);
	//执行断开
	((wb_tcp_link*)cl.get())->DoClose(this, cFlag);
}

void wb_tcp_filter::DoOnConnect(wb_tcp_link* cl, LPOVERLAPPED pol)
{
	_mcp.recover_mem(pol);
	auto nbi = (const ustrt_net_base_info&)*cl;
	auto ret = cl->connect();
	if (ret) 
	{
		auto_tcp_link tcp_link(cl, &_mlp);
		bool b_ok = false;
		OnNewLink(cl);
		assert(cl->get_event());
		do
		{
			if (!_io.relate((HANDLE)((SOCKET)*cl), nullptr))
			{
				assert(0);
				break;
			}
			assert(cl->get_event());
			if (!post_recv(cl, &tcp_link, nullptr))
			{
				assert(0);
				break;
			}
			b_ok = true;
		} while (0);
		if(!b_ok)
		{
			OnCloseLink(tcp_link);
			assert(0);
			cl->close();
		}
		else
		{
			_lock.lock();
			assert(_links.find(nbi) == _links.end());
			_links[nbi] = tcp_link;
			_lock.unlock();
			assert(cl->get_event());
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

}
void wb_tcp_filter::OnRead(const ETH_PACK* pg, int len)
{
	// cFlag位  URG ACK PSH RST SYN FIN
	//if (pg->ip_h.uiDestIp != 0x42cecd3c)return;//调试阶段其他ip的tcp不处理
	const ustrt_net_base_info & nbi = *(ustrt_net_base_info*)&pg->ip_h.uiSourIp;
	auto t_flag = TCP_FLAG(pg->tcp_h.cFlag);
	_read_packs++;
	if (t_flag == TCP_SYN1)//建立新连接,如果存在旧的连接，先关闭旧连接
	{
		add_connect_link(pg, nbi);
	}
	else
	{
		auto plink = get_link(nbi);
		if (!plink)
		{
			return;
			if (!TCP_FLAG_RST(t_flag) && !TCP_FLAG_FIN(t_flag))
			{
				write_rst_pack(pg);		
			}
			else
			{
			}
			return;
		}
		if (TCP_FLAG_RST(t_flag) || TCP_FLAG_FIN(t_flag))//关闭连接
		{
			close_link(plink, t_flag);
		}
		else
		{
#ifdef _DEBUG
			if (((wb_tcp_link*)plink.get())->get_close_status() == 0) {
				assert(plink->Ref() >= 3);
			}
#endif 
			plink->OnRead(this, &plink, pg, len);
		}
	}
}

void wb_tcp_filter::DoOnRecv(const char* buffer, int len, WB_TCP_OVERLAPPED* pol) {
	//auto err = GetLastError();
	if (len > 0)
	{
		auto err = WSAGetLastError();
		if (997 == GetLastError())
		{
			int kl = 0;
		}
		
		_recv_bytes+=len ;
		assert(pol->p_lk->Ref() >= 1);
		if(pol->p_lk->OnRecv(this,&pol->p_lk, buffer, len, &pol->ol))
			post_recv(pol->p_lk.get(), &pol->p_lk, &pol->ol);
	}
	else
	{
		//WLOG("连接断开:%p  %d \n", pol->p_lk.get(),GetLastError());
		close_link(pol->p_lk,0);
		WB_TCP_OVERLAPPED::operator delete(pol, _mop);  //_mop.recover_mem(pol);
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
		if (c_link)
			_con_link[nbi] = c_link;
	}
	_con_lock.unlock();
	if(!b_exists){
	
		
		if (!c_link) {
			RLOG("TCP连接池资源不足....................\n");
			return nullptr;
		}
		if (!c_link->create_socket())
		{
			WLOG("连接：%p create socket 失败:%d\n", c_link,GetLastError());
			_con_lock.lock();
			_con_link.erase(nbi);
			_con_lock.unlock();
			wb_tcp_link::operator delete (c_link, _mlp);
			return nullptr;
		}
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

wb_tcp_filter::auto_tcp_link wb_tcp_filter::get_link(const ustrt_net_base_info& nbi)
{
	AUTO_LOCK(_lock);
	auto fd = _links.find(nbi);
	if (fd != _links.end())
	{
		assert(fd->second->Ref()>= 2);
		return fd->second;
	}
	return auto_tcp_link();
}

bool wb_tcp_filter::post_send(wb_link_interface* plink, void* const lp_link, const char* buf, int len) {

	auto pl = new(_mop) WB_TCP_OVERLAPPED;// _mop.get_mem();
	if (pl) {

		pl->op = tcp_operator::e_send;
		pl->wbuf.buf = pl->buf;
		pl->wbuf.len = len;
		memcpy(pl->buf, buf, len);
		pl->p_lk = *((auto_tcp_link*)lp_link);
		if (!plink->Send(&pl->wbuf, &pl->ol)) {
			close_link(pl->p_lk,0);
			WB_TCP_OVERLAPPED::operator delete(pl, _mop);// _mop.recover_mem(pl);
			return false;
		}
	}
	return true;
}

bool wb_tcp_filter::post_send_no_copy(wb_link_interface* plink, void* const lp_link, char* buf, int len)
{
	auto pl = new(_mop) WB_TCP_OVERLAPPED;// _mop.get_mem();
	if (pl) {

		pl->op = tcp_operator::e_send_no_copy;
		pl->wbuf.buf = buf;
		pl->no_copy = buf;
		pl->wbuf.len = len;
		pl->p_lk = *((auto_tcp_link*)lp_link);
		if (!plink->Send(&pl->wbuf, &pl->ol)) {
			close_link(pl->p_lk, 0);
			WB_TCP_OVERLAPPED::operator delete(pl, _mop);// _mop.recover_mem(pl);
			return false;
		}
	}
	return true;
}

bool wb_tcp_filter::post_recv(wb_link_interface* plink, void* const lp_link, LPOVERLAPPED pol) 
{
	auto pl = (WB_TCP_OVERLAPPED*)pol;
	if (!pl)
		pl = new(_mop) WB_TCP_OVERLAPPED;
	assert(pl);
	if (pl) {

		pl->op = tcp_operator::e_recv;
		pl->wbuf.len = TCP_RECV_SIZE;
		pl->wbuf.buf = pl->buf;
		pl->p_lk = *((auto_tcp_link*)lp_link);
		if (!plink->Recv(&pl->wbuf, pl->dwFlag, nullptr, nullptr, &pl->ol))
		{
			close_link(pl->p_lk,0);
			WB_TCP_OVERLAPPED::operator delete(pl, _mop);// _mop.recover_mem(pl);
			return false;
		}
	}
	return true;
};
