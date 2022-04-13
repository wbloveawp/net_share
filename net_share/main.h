#pragma once
#include "net_filter.h"
#include "net_card.h"
#include "wb_io.hpp"

enum class io_oper_type {

	e_read,
	e_write,
};

//��д����ʹ��
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
* ������
* ������װ�������
*/

class mainer :public wb_net_card_interface {
	std::atomic_int		_default_reads = 0;
	wb_base_memory_pool _mp;//����overloap mem
	wb_io				_nc_io;//����io
	wb_net_card_file	_ncf;//��������

	wb_udp_filter*		_udp ;//udp������
	wb_tcp_filter*		_tcp ;//tcp������
	wb_icmp_filter*		_icmp;
	
	std::atomic_int writes = 0;
	using pack_list = std::list<WB_OVERLAPPED*> ;
	pack_list _write_list;
	wb_lock   _lock;
	
public:
	mainer();
	virtual ~mainer();
	void start();
	void stop();
	bool get_net_card_name(const char* ip, char* buffer, int buf_len,char* mask, int mask_len);
	virtual void post_write(wb_filter_event* p_nce, wb_link_interface* plink, const ETH_PACK* pk, int len);
	//io�¼�
	void DoOnWrite(DWORD numberOfBytes, WB_OVERLAPPED* pol);
	void DoOnRead(DWORD numberOfBytes, WB_OVERLAPPED* pol);//�����ȡ����������

};