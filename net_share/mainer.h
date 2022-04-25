#pragma once
#include "net_filter.h"
#include "net_card.h"
#include "wb_io.hpp"

enum class io_oper_type {

	e_read,
	e_write,
};

//读写网卡使用
struct WB_OVERLAPPED
{
	OVERLAPPED			ol;
	io_oper_type		ot;
	char				buf[MAX_MAC_LEN];
	int					len;
	wb_link_interface*	p_lk;
	wb_filter_event	*	p_nce;
};
/*
* 主程序
* 管理、组装各类组件
*/

struct wb_net_info
{
	INT64 read_packs;
	INT64 write_packs;

	INT64 read_bytes;
	INT64 write_bytes;
};

class mainer :public wb_net_card_interface {
	std::atomic_int		_default_reads = 0;
	wb_base_memory_pool _mp;//网卡overloap mem
	wb_io				_nc_io;//网卡io
	wb_net_card_file	_ncf;//网卡对象

	wb_udp_filter*		_udp ;//udp过滤器
	wb_tcp_filter*		_tcp ;//tcp过滤器
	wb_icmp_filter*		_icmp;
	
	std::atomic_int writes = 0;
	using pack_list = std::list<WB_OVERLAPPED*> ;
	pack_list _write_list;
	wb_lock   _lock;

	std::string	 _szIP="";
	std::string  _szCardName = "";
	//统计数据
	std::atomic<INT64> _read_packs =0;
	std::atomic<INT64> _write_packs=0;

	std::atomic<INT64> _read_bytes=0;
	std::atomic<INT64> _write_bytes=0;
public:
	mainer(wb_machine_interface* mi = nullptr);
	virtual ~mainer();
	bool start();
	void stop();

	virtual void post_write(wb_filter_event* p_nce, wb_link_interface* plink, const ETH_PACK* pk, int pack_len, int data_len = 0);

	std::string get_card_name() { return _szCardName ; };
	std::string get_card_ip() { return _szIP; }

	wb_net_info get_net_info()const {

		return { _read_packs,_write_packs,_read_bytes,_write_bytes };
	}

	auto get_net_info(BYTE cbProtocol)const ->decltype(_tcp->get_flow_info()) {

		if (PRO_TCP == cbProtocol)
			return _tcp->get_flow_info();

		if (PRO_UDP == cbProtocol)
			return _udp->get_flow_info();
		return { 0,0 ,0,0};
	}
protected:
	bool get_net_card_name(const char* ip, char* buffer, int buf_len, char* mask, int mask_len);
	//io事件
	void DoOnWrite(DWORD numberOfBytes, WB_OVERLAPPED* pol);
	void DoOnRead(DWORD numberOfBytes, WB_OVERLAPPED* pol);//处理读取到的网卡包
};