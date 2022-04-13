#pragma once

#include "net_link.h"
#include "wb_io.hpp"
#include <map>
//抽象基本过滤器
class wb_net_filter :public wb_filter_interface, public wb_filter_event
{
protected:
	wb_net_card_interface* _p_nci = nullptr;
	wb_io					_io;
	wb_base_memory_pool		_mdp;
public:
	wb_net_filter(wb_net_card_interface* pnci) :_p_nci(pnci) {};
	virtual ~wb_net_filter() {};

	virtual bool start(int thds) {
		return _mdp.start(sizeof(wb_data_pack), 10240);
	};
	virtual void stop() {
		_mdp.stop();
	};

	virtual wb_data_pack* get_data_pack() { return (wb_data_pack*)_mdp.get_mem(); };
	void back_data_pack(wb_data_pack* p) { _mdp.recover_mem(p); };

	virtual void post_write(wb_link_interface* plink, const ETH_PACK* pk, int len) { _p_nci->post_write(this, plink, pk, len); };

	virtual bool post_send_no_copy(wb_link_interface* plink, char* buf, int len) { return true; }
	//3个网络接口

	//2个网卡事件
	//virtual void OnRead(const ETH_PACK* pg, int len) = 0;//从网卡读取到网络包,len：包总长度
	virtual void OnWrite(wb_link_interface* plink, const ETH_PACK* pg, int len) {
		plink->OnWrite(this, pg, len);
	};

	virtual wb_data_pack_ex* get_data_pack_ex() { return nullptr; };//获取一个数据包结构
	virtual void back_data_pack_ex(wb_data_pack_ex*) {};//归还数据包结构
};

enum class udp_io_oper_type {

	e_recv,
	e_send,
	e_send_no_copy
};

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
	wb_udp_link* p_lk;
};


//udp过滤器
class wb_udp_filter :public wb_net_filter
{
protected:
	using udp_link = wb_share_ptr<wb_udp_link>;
	using link_map = std::map<ustrt_net_base_info, wb_udp_link*>;

	wb_base_memory_pool	_mlp;//连接池
	wb_base_memory_pool	_mop;//重叠池
	link_map			_links;
	wb_lock				_lock;
	std::thread			_check_t;
	std::atomic_bool	_active = false;
	//分片
	using frame_list = std::list<wb_data_pack*>;
	struct frame_info
	{
		ustrt_net_base_info	nbi;
		wb_lock  lk;
		frame_list	fl;
	};
	using frame_data = std::map<FRAME_KEY, frame_info*>;
	frame_data		_fd;
	wb_lock			_f_lock;


	using link_list = std::list<wb_udp_link*>;
	wb_lock			_close_lk;
#ifdef _DEBUG
	link_map		_close_map;
#endif 
	link_list		_close_lst;
public:
	wb_udp_filter(wb_net_card_interface* pnci) :wb_net_filter(pnci) {}

	virtual bool start(int thds);
	virtual void stop();
	wb_udp_link* get_link(const ustrt_net_base_info* pif, const ETH_PACK* pg);
	void close_link(wb_udp_link* plink) {
		const ustrt_net_base_info& nbi = (const ustrt_net_base_info&)*plink;
		plink->close();
		if (_active) {
			_lock.lock();
			_links.erase(nbi);
			_lock.unlock();

			_close_lk.lock();
#ifdef _DEBUG
			auto fd = _close_map.find(nbi);
			if (fd != _close_map.end())
			{
				assert(plink!=fd->second);
			}
			for (auto & i : _close_lst)
			{
				assert(i != plink);
			}
			_close_map[nbi] = plink;
#endif
			_close_lst.push_back(plink);
			_close_lk.unlock();
			/*
			std::thread t = std::thread([=]() {
				std::this_thread::sleep_for(std::chrono::seconds(2));
				//释放连接数据
				//WLOG("UDP连接释放:%p\n", plink);
				wb_udp_link::operator delete(plink, this->_mlp);
				});
			t.detach();
			*/
		}
	}

	//udp网卡包读取事件
	virtual void DoOnRecv(wb_udp_link* plink, const char* buffer, int len, WB_OVERLAPPED_UDP* pol) {
		if (len > 0)
		{
			plink->OnRecv(this, buffer, len, &pol->ol);
		}
		else
		{
			close_link(plink);
			_mop.recover_mem(pol);
		}
	}
	virtual void DoOnSend(wb_udp_link* plink, const char* buffer, int len, WB_OVERLAPPED_UDP* pol) {

		if (len > 0 )
		{
			if (pol->wbuf.len > len)
			{
				pol->wbuf.buf = pol->buf + len;
				pol->wbuf.len -= len;
				if (!plink->Send(&pol->wbuf, &pol->ol))
					_mop.recover_mem(pol);
				return;
			}
			plink->OnSend(this, buffer, len, &pol->ol);
		}
		_mop.recover_mem(pol);
	}
	virtual void DoOnSend_No_Copy(wb_udp_link* plink, char* buffer, int len, LPOVERLAPPED pol) {
		WLOG("delete buffer:%p  %d\n", buffer, len);
		delete[]buffer;
		_mop.recover_mem(pol); 
	}
	virtual void OnRead(const ETH_PACK* pg, int len);

	virtual bool post_recv(wb_link_interface* plink, LPOVERLAPPED pol);
	virtual bool post_send(wb_link_interface* plink, const char* buf, int len);
	virtual bool post_send_no_copy(wb_link_interface* plink, char* buf, int len);
};

class wb_icmp_filter : public wb_net_filter
{
	using link_map = std::map<unsigned __int64, wb_icmp_link*>;
	using icmp_map = std::map<ICMP_KEY, wb_icmp_link*>;
	wb_base_memory_pool	_mop;//重叠池
	wb_base_memory_pool	_mlp;//连接池
	icmp_map			_d_links;
	link_map			_links;
	SOCKET				_r_sock = INVALID_SOCKET;
	wb_lock				_lock;
	wb_lock				_ic_lock;
public:

	wb_icmp_filter(wb_net_card_interface* pnci) :wb_net_filter(pnci) {
		
	}

	virtual bool start(int thds);
	virtual void stop();
	
	//IO事件
	virtual void DoOnRecv(wb_icmp_link* plink, const char* buffer, int len, LPOVERLAPPED pol);
	virtual void DoOnSend(wb_icmp_link* plink, const char* buffer, int len, LPOVERLAPPED pol) {
		_ic_lock.lock();
		_d_links[(ICMP_KEY)*plink] = plink;
		_ic_lock.unlock();
		plink->OnSend(this, buffer, len, pol);
		_mop.recover_mem(pol);
		//post_recv(nullptr, pol);
	}
	//网卡事件
	virtual void OnRead(const ETH_PACK* pg, int len);

	virtual bool post_recv(wb_link_interface* plink, LPOVERLAPPED pol);
	virtual bool post_send(wb_link_interface* plink, const char* buf, int len);
};



//连接overloaped
enum class tcp_operator {
	e_send,
	e_recv
};
struct WB_CONNECT_OVERLAPPED
{
	OVERLAPPED		ol;
};

struct WB_TCP_OVERLAPPED
{
	OVERLAPPED		ol;
	tcp_operator	op;
	WSABUF			wbuf;
	char			buf[TCP_RECV_SIZE];
	DWORD			dwFlag;
	wb_tcp_link*	p_lk;
};

class wb_tcp_filter :public wb_net_filter
{
	using tcp_link = wb_share_ptr<wb_tcp_link>;
	using link_map = std::map<ustrt_net_base_info, wb_tcp_link*>;

	wb_base_memory_pool _mlp;
	wb_base_memory_pool _mop;
	wb_base_memory_pool _mcp;

	wb_io				_conn_io;//连接io 执行connect的io

	wb_lock				_con_lock;
	link_map			_con_link;//正在连接的连接

	wb_lock				_lock;
	link_map			_links;//正常的连接

	wb_lock				_fin_lock;
	link_map			_fin_links;//正在关闭的连接

	std::atomic_bool	_active = false;
public:
	wb_tcp_filter(wb_net_card_interface* pnci) :wb_net_filter(pnci) {
	}

	virtual bool start(int thds);
	virtual void stop() {
		_active = false;
	}

	void close_link(wb_tcp_link* cl,char cFlag);
	void DoOnConnect(wb_tcp_link* plink, LPOVERLAPPED pol);
	void DoOnRecv(wb_tcp_link* plink, const char* buffer, int len, WB_TCP_OVERLAPPED* pol);
	void DoOnSend(wb_tcp_link* plink, const char* buffer, int len, WB_TCP_OVERLAPPED* pol) {
		if (len > 0)
		{
			if (pol->wbuf.len > len) {
				pol->wbuf.buf = pol->buf + len;
				pol->wbuf.len -= len;
				if( !plink->Send(&pol->wbuf,&pol->ol))
					_mop.recover_mem(pol);
				return;
			}
			plink->OnSend(this, buffer, len, &pol->ol);
		}
		_mop.recover_mem(pol);
	};
	wb_tcp_link* add_connect_link(const ETH_PACK* pg,const ustrt_net_base_info& nbi);
	wb_tcp_link* get_link(const ustrt_net_base_info& nbi);
	void write_rst_pack(const ETH_PACK* pg);
	virtual void OnRead(const ETH_PACK* pg, int len) ;//从网卡读取到网络包,len：包总长度
	virtual void OnWrite(wb_link_interface* plink, const ETH_PACK* pg, int len) {
		//WLOG("tcp 写完成!%d\n", len);
		//plink->OnWrite(this, pg, len);
	};//从网卡写完网络包

	virtual bool post_recv(wb_link_interface* plink, LPOVERLAPPED pol);
	virtual bool post_send(wb_link_interface* plink, const char* buf, int len);

	virtual wb_data_pack_ex* get_data_pack_ex() { return (wb_data_pack_ex * )_mdp.get_mem(); };//获取一个数据包结构
	virtual void back_data_pack_ex(wb_data_pack_ex* pd) { _mdp.recover_mem(pd); };//归还数据包结构
};