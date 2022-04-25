#pragma once

#include "net_link.h"
#include "wb_io.hpp"
#include <map>
//抽象基本过滤器
class wb_net_filter :public wb_filter_interface, public wb_filter_event
{
protected:
	using auto_link_ptr		= wb_share_ptr<wb_net_link>;
	wb_net_card_interface*	_p_nci = nullptr;
	wb_io					_io;
	wb_base_memory_pool		_mdp;

	std::atomic<INT64>		_recv_bytes=0;
	std::atomic<INT64>		_send_bytes=0;

	wb_link_event*			_event;
public:
	wb_net_filter(wb_net_card_interface* pnci, wb_link_event* ev=nullptr) :_p_nci(pnci), _event(ev){};
	virtual ~wb_net_filter() {};

	virtual bool start(int thds) {
		return _mdp.start(sizeof(wb_data_pack), 10240);
	};
	virtual void stop() {
		_mdp.stop();
	};

	virtual wb_data_pack* get_data_pack() { return (wb_data_pack*)_mdp.get_mem(); };
	void back_data_pack(wb_data_pack* p) { _mdp.recover_mem(p); };

	virtual void post_write(wb_link_interface* plink, const ETH_PACK* pk, int len,int data_len=0) { _p_nci->post_write(this, plink, pk, len, data_len); };
	//virtual void post_write(wb_link_interface* plink, const ETH_PACK* pk, const int& len) { _p_nci->post_write(this, plink, pk, len); };//
	virtual bool post_send_no_copy(wb_link_interface* plink, void* const lp_link, char* buf, int len) { return true; }
	virtual bool post_send_ex(wb_link_interface* plink, void* const lp_link, const char* buf, int len, void* pd) { return true; }
	virtual void OnWrite(wb_link_interface* plink, const ETH_PACK* pg, int len) {
		plink->OnWrite(this, pg, len);
	};

	virtual wb_data_pack_ex* get_data_pack_ex() { return nullptr; };//获取一个数据包结构
	virtual void back_data_pack_ex(wb_data_pack_ex*) {};//归还数据包结构

	void OnNewLink(wb_link_interface* lk) {
		if (_event)
			_event->OnNewLink(lk);
	};

	void OnCloseLink(wb_link_interface* lk) {
		if (_event)
			_event->OnCloseLink(lk);
	};
};

enum class udp_io_oper_type {

	e_recv,
	e_send,
	e_send_no_copy
};

//udp过滤器
class wb_udp_filter :public wb_net_filter
{
protected:
	using auto_udp_link = auto_link_ptr;// wb_share_ptr<wb_udp_link>;
	//读写网卡使用
	struct WB_OVERLAPPED_UDP
	{
		OVERLAPPED			ol;
		udp_io_oper_type	ot;
		WSABUF				wbuf;
		char				buf[UDP_RECV_SIZE];
		DWORD				dwFlag;
		sockaddr_in			addr;
		int					addr_len;
		auto_udp_link		p_lk;

		void* operator new (size_t size, wb_base_memory_pool& mp) { return mp.get_mem(); }
		void operator delete(void* p, wb_base_memory_pool& mp) {
			((WB_OVERLAPPED_UDP*)p)->~WB_OVERLAPPED_UDP();
			mp.recover_mem(p);
		}
	};
	using frame_list = std::list<wb_data_pack*>;
	struct frame_info
	{
		ustrt_net_base_info	nbi;
		wb_lock				lk;
		frame_list			fl;
	};
	using frame_data = std::map<FRAME_KEY, frame_info*>;

	using link_map = std::map<ustrt_net_base_info, auto_udp_link>;

	wb_base_memory_pool		_mlp;//连接池
	wb_base_memory_pool		_mop;//重叠池
	link_map				_links;
	wb_lock					_lock;

	std::thread				_check_t;
	std::atomic_bool		_active = false;
	//分片缓存数据
	frame_data				_fd;
	wb_lock					_f_lock;
public:
	wb_udp_filter(wb_net_card_interface* pnci, wb_link_event* ev = nullptr) :wb_net_filter(pnci,ev) {}

	virtual bool start(int thds);
	virtual void stop();
	auto_udp_link get_link(const ustrt_net_base_info* pif, const ETH_PACK* pg);
	void close_link(wb_net_link* plink) {
		assert(plink);
		auto nbi = (const ustrt_net_base_info&)(*plink);
		plink->close();
		_lock.lock();
		_links.erase(nbi);
		_lock.unlock();
	}

	//udp网卡包读取事件
	virtual void DoOnRecv(const char* buffer, int len, WB_OVERLAPPED_UDP* pol) {
		//WLOG("UDP recv数据:%d  %s\n",len,buffer);
		do
		{
			if (len <= 0)break;
			if (!pol->p_lk->OnRecv(this, &pol->p_lk, buffer, len, &pol->ol)) {
				break;
			}
			if (!post_recv(pol->p_lk, &pol->p_lk, &pol->ol))
			{
				break;
			}
			return;
		} while (0);
		close_link(pol->p_lk);
		WB_OVERLAPPED_UDP::operator delete(pol, _mop);// _mop.recover_mem(pol);
	}

	virtual void DoOnSend(const char* buffer, int len, WB_OVERLAPPED_UDP* pol) {
		
		if (len > 0)
		{
			if (pol->wbuf.len > len)
			{
				pol->wbuf.buf = pol->buf + len;
				pol->wbuf.len -= len;
				if (!pol->p_lk->Send(&pol->wbuf, &pol->ol))
					close_link(pol->p_lk);
				else
					return;
			}
			else
			{
				pol->p_lk->OnSend(this,&pol->p_lk, buffer, len, &pol->ol);
			}
		}
		else
		{
			close_link(pol->p_lk);
		}
		WB_OVERLAPPED_UDP::operator delete(pol, _mop); //_mop.recover_mem(pol);
	}
	virtual void DoOnSend_No_Copy(char* buffer, int len, WB_OVERLAPPED_UDP* pol) {
		WLOG("delete buffer:%p  %d\n", buffer, len);
		delete[]buffer;
		WB_OVERLAPPED_UDP::operator delete(pol, _mop);  //_mop.recover_mem(pol);
	}
	virtual void OnRead(const ETH_PACK* pg, int len);

	virtual bool post_recv(wb_link_interface* plink, void* const lp_link, LPOVERLAPPED pol);
	virtual bool post_send(wb_link_interface* plink, void* const lp_link, const char* buf, int len);
	virtual bool post_send_no_copy(wb_link_interface* plink, void* const lp_link, char* buf, int len);
};


class wb_icmp_filter : public wb_net_filter
{
	using auto_icmp_link = wb_share_ptr<wb_icmp_link>;
	struct WB_OVERLAPPED_ICMP
	{
		OVERLAPPED			ol;
		udp_io_oper_type	ot;
		WSABUF				wbuf;
		char				buf[ICMP_RECV_SIZE];
		DWORD				dwFlag;
		sockaddr_in			addr;
		int					addr_len;
		auto_icmp_link		p_lk;

		void* operator	new (size_t size, wb_base_memory_pool& mp) { return mp.get_mem(); }
		void operator	delete (void* p, wb_base_memory_pool& mp) { 
			((WB_OVERLAPPED_ICMP*)p)->~WB_OVERLAPPED_ICMP();
			mp.recover_mem(p);
		}
	};


	using link_map = std::map<ICMP_LINK_KEY, auto_icmp_link>;
	using icmp_map = std::map<ICMP_PACK_KEY, auto_icmp_link>;
	wb_base_memory_pool		_mop;//重叠池
	wb_base_memory_pool		_mlp;//连接池
	icmp_map				_d_links;
	link_map				_links;
	SOCKET					_r_sock = INVALID_SOCKET;
	wb_lock					_lock;
	wb_lock					_ic_lock;
public:

	wb_icmp_filter(wb_net_card_interface* pnci, wb_link_event* ev = nullptr) :wb_net_filter(pnci,ev) {}

	virtual bool start(int thds);
	virtual void stop();
	
	//IO事件
	virtual void DoOnRecv(const char* buffer, int len, WB_OVERLAPPED_ICMP* pol);
	virtual void DoOnSend(const char* buffer, int len, WB_OVERLAPPED_ICMP* pol) {
		pol->p_lk->OnSend(this, &pol->p_lk,buffer, len, &pol->ol);
		WB_OVERLAPPED_ICMP::operator delete(pol, _mop);// _mop.recover_mem(pol);
	}
	//网卡事件
	virtual void OnRead(const ETH_PACK* pg, int len);

	virtual bool post_recv(wb_link_interface* plink, void* const lp_link, LPOVERLAPPED pol);
	virtual bool post_send(wb_link_interface* plink, void* const lp_link, const char* buf, int len) { return true; };
	virtual bool post_send_ex(wb_link_interface* plink, void* const lp_link, const char* buf, int len, void* pd);
};

//连接overloaped
class wb_tcp_filter :public wb_net_filter
{
	//friend wb_tcp_link;
	using auto_tcp_link = auto_link_ptr;// wb_share_ptr<wb_tcp_link>;
	enum class tcp_operator {
		e_send,
		e_recv,
		e_recv_del,
		e_send_no_copy
	};

	struct WB_TCP_OVERLAPPED
	{
		OVERLAPPED		ol;
		tcp_operator	op;
		WSABUF			wbuf;
		char			buf[TCP_RECV_SIZE];
		DWORD			dwFlag;
		char* no_copy;
		auto_tcp_link	p_lk;

		void* operator	new (size_t size, wb_base_memory_pool& mp) { return mp.get_mem(); }
		void operator	delete (void* p, wb_base_memory_pool& mp) {
			((WB_TCP_OVERLAPPED*)p)->~WB_TCP_OVERLAPPED();
			mp.recover_mem(p);
		}
	};

	using link_map = std::map<ustrt_net_base_info, auto_tcp_link>;

	wb_base_memory_pool _mlp;
	wb_base_memory_pool _mop;
	wb_base_memory_pool _mcp;

	wb_io				_conn_io;//连接io 执行connect的io

	wb_lock				_con_lock;
	std::map<ustrt_net_base_info, wb_tcp_link*>	_con_link;//正在连接的连接

	wb_lock				_lock;
	link_map			_links;//正常的连接

	std::atomic_bool	_active = false;

	std::thread			_check_thread;
public:
	wb_tcp_filter(wb_net_card_interface* pnci, wb_link_event* ev = nullptr) :wb_net_filter(pnci,ev) {}

	virtual bool start(int thds);
	virtual void stop() {
		_active = false;		
		_conn_io.stop();
		_io.stop();
		if (_check_thread.joinable())
			_check_thread.join();
		_lock.lock();
		_links.clear();
		_lock.unlock();
		_mlp.stop();
		_mop.stop();
		_mcp.stop();
		_mdp.stop();
	}

	void close_link(auto_tcp_link& cl,char cFlag);
	void DoOnConnect(wb_tcp_link* plink, LPOVERLAPPED pol);
	void DoOnRecv(const char* buffer, int len, WB_TCP_OVERLAPPED* pol);
	void DoOnSend(const char* buffer, int len, WB_TCP_OVERLAPPED* pol) {
		assert(pol->p_lk.get());
		if (len > 0)
		{
			if (pol->wbuf.len > len) {
				pol->wbuf.buf = pol->no_copy + len;
				pol->wbuf.len -= len;
				if (!pol->p_lk->Send(&pol->wbuf, &pol->ol))
				{
					WB_TCP_OVERLAPPED::operator delete(pol, _mop);// _mop.recover_mem(pol);
				}	
				return;
			}
			pol->p_lk->OnSend(this,&pol->p_lk, buffer, len, &pol->ol);
		}
		WB_TCP_OVERLAPPED::operator delete(pol, _mop); //_mop.recover_mem(pol);
	};
	void DoOnSend_No_Copy(const char* buffer, int len, WB_TCP_OVERLAPPED* pol) {
		if (len > 0)
		{
			if (pol->wbuf.len > len) {
				pol->wbuf.buf = pol->wbuf.buf + len;
				pol->wbuf.len -= len;
				if (!pol->p_lk->Send(&pol->wbuf, &pol->ol))
				{
					WB_TCP_OVERLAPPED::operator delete(pol, _mop);// _mop.recover_mem(pol);
				}
				return;
			}
			pol->p_lk->OnSend_no_copy(this, &pol->p_lk, pol->no_copy, len);
		}
		WB_TCP_OVERLAPPED::operator delete(pol, _mop); //_mop.recover_mem(pol);
	};
	wb_tcp_link* add_connect_link(const ETH_PACK* pg,const ustrt_net_base_info& nbi);
	auto_tcp_link get_link(const ustrt_net_base_info& nbi);
	void write_rst_pack(const ETH_PACK* pg);
	virtual void OnRead(const ETH_PACK* pg, int len) ;//从网卡读取到网络包,len：包总长度

	virtual bool post_recv(wb_link_interface* plink, void* const lp_link, LPOVERLAPPED pol);
	virtual bool post_send(wb_link_interface* plink, void* const lp_link, const char* buf, int len);
	virtual bool post_send_no_copy(wb_link_interface* plink, void* const lp_link, char* buf, int len);

	virtual wb_data_pack_ex* get_data_pack_ex() { return (wb_data_pack_ex * )_mdp.get_mem(); };//获取一个数据包结构
	virtual void back_data_pack_ex(wb_data_pack_ex* pd) { _mdp.recover_mem(pd); };//归还数据包结构
};
