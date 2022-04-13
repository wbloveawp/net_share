#pragma once
#include "net_pack.h"
#include "base_component.h"
#include <ostream>
#include <map>
#include <assert.h>

class wb_net_link:public wb_link_interface {

protected:
	char		_write_buf[MAX_MAC_LEN] = {};//ÿ������ÿ��ֻ������Ͷ��һ����̫��֡
	ETH_PACK*	_pack = nullptr;	//���Կ��ٸ����������
	IP_HEADER*	_ip_h = nullptr;
	IP_HEADER	_m_ip_h = {};
	SOCKET		_socket=INVALID_SOCKET;//
	USHORT		_pack_id;//��ǰ�İ���
	ustrt_net_base_info _nbi;
	std::atomic_int64_t	_last_op_time = 0;
public:
	wb_net_link(const ETH_PACK* pg);

	virtual ~wb_net_link() {}

	explicit operator const ETH_PACK* ()const	{ return _pack; }
	explicit operator		ETH_PACK* ()		{ return _pack; }
	explicit operator const SOCKET()	const   { return _socket;}
	explicit operator const ustrt_net_base_info& () const { return _nbi; }

	virtual void close() {
		if (INVALID_SOCKET!= _socket)
		{
			closesocket(_socket);
		}
		_socket = INVALID_SOCKET;
	}

	void* operator new(size_t size, wb_base_memory_pool& mp) {return mp.get_mem();}
	void operator delete(void* p, wb_base_memory_pool& mp) {
		((wb_net_link*)p)->~wb_net_link();
		mp.recover_mem(p);
	}

	virtual const ETH_PACK* make_pack(const char* buffer, int len,int &pack_len) = 0;
	
	virtual bool OnRecv(wb_filter_interface* p_fi, const char* buf, int len, LPOVERLAPPED pol) {
		
		return true;
	};//��������ȡ�������,len�����ܳ���
	virtual bool OnSend(wb_filter_interface* p_fi, const char* buf, int len, LPOVERLAPPED pol) {
		//WLOG("udp���ݷ������:%d\n", len);
		return true;
	};//������д�������

	virtual bool OnWrite(wb_filter_interface* p_fi, const ETH_PACK* pg, int len) {
		//WLOG("����д���:%d\n", len);
		return true;
	};//������д�������

	virtual bool is_timeout() { 
		//assert(0);
		return true;
	}
};

struct FRAME_KEY
{
	USHORT		usPid;//��id
	UINT		uDstIp;
	UINT		uSrcIp;

	bool operator <(const FRAME_KEY& fk) const {
		if (usPid == fk.usPid)
		{
			if (uDstIp == fk.uDstIp)
				return uSrcIp < fk.uSrcIp;
			else
				return uDstIp < fk.uDstIp;
		}
		return usPid < fk.usPid;
	}
};


class wb_udp_link :public wb_net_link
{
	FALS_UDP_HEADER*	_p_fudp_h = nullptr;
	UDP_HEADER*			_p_udp_h  = nullptr;
	char*				_udp_data = nullptr;

	FALS_UDP_HEADER		_f_udp_h = {};//udpαͷ
	UDP_HEADER			_m_udp_h = {};
	sockaddr_in			_addr = {};
	int					_addr_len = sizeof(sockaddr_in);

	std::atomic_int		_psk = 0;
public:
	wb_udp_link(const ETH_PACK* pg, const ustrt_net_base_info* pif);
	virtual ~wb_udp_link() { 
		//WLOG("~wb_udp_link %p\n",this);
		close();
	}
	virtual bool OnRead(wb_filter_interface* p_fi, const ETH_PACK* pk, int len);
	virtual bool OnSend_no_copy(wb_filter_interface* p_fi, char* buf, int len) { return true; };
	virtual bool OnRecv(wb_filter_interface* p_fi, const char* buf, int len, LPOVERLAPPED pol);
	//���ݴ��
	virtual const ETH_PACK* make_pack_1st(const char* buffer, int len, int& pack_len);
	virtual const ETH_PACK* make_pack_mid(USHORT MF_idx, const char* buffer, int len, bool is_end, int& pack_len);
	virtual const ETH_PACK* make_pack(const char* buffer, int len, int& pack_len);

	virtual bool create_socket();

	virtual bool Recv(LPWSABUF buffer, DWORD& dwFlags, sockaddr* addr, int* addr_len, LPOVERLAPPED pol) {
		dwFlags = MSG_PARTIAL;
		if (SOCKET_ERROR == WSARecvFrom(_socket, buffer, 1, nullptr, &dwFlags,addr, addr_len, pol, nullptr)
			&& GetLastError() != ERROR_IO_PENDING)
		{
			return false;
		}
		return true;
	}

	virtual bool Send(LPWSABUF buffer, LPOVERLAPPED pol) {
		if (SOCKET_ERROR == WSASendTo(_socket, buffer, 1, nullptr, 0, (sockaddr*)&_addr, _addr_len, pol, nullptr)
			&& GetLastError() != ERROR_IO_PENDING)
		{
			return false;
		}
		return true;
	}

	virtual bool is_timeout() {return time(0) - _last_op_time > TIME_OUT_UDP;}
};

//ԭʼicmp socket ȫ��ֻ�ܴ���һ��
//#pragma pack(push,1)
struct  ICMP_KEY
{
	UINT		uDstIp;//ping��Ŀ��ip
	USHORT		usSequence;//��ǰ�����
	
	bool operator <(const ICMP_KEY& ik) const{
		if (uDstIp == ik.uDstIp)
			return usSequence < ik.usSequence;
		return uDstIp < ik.uDstIp;
	}
};
//#pragma pack(pop)

class wb_icmp_link : public wb_link_interface
{
	char		_write_buf[c_ip_head_size + c_icmp_head_len] = {};
	ETH_PACK*	_pack = nullptr;	//���Կ��ٸ����������
	IP_HEADER*	_ip_h = nullptr;
	char*		_p_icmp_data= nullptr;// = nullptr;

	sockaddr_in	_addr = {};

	ICMP_KEY	_key;
	UINT		_uSrcIp;
public:
	wb_icmp_link(const ETH_PACK* pg) {
		_pack = (ETH_PACK*)_write_buf;
		_ip_h = &_pack->ip_h;
		_p_icmp_data = (char*)_ip_h + c_ip_head_len;
		_uSrcIp = pg->ip_h.uiSourIp;
		_addr.sin_family = AF_INET;
		_key.uDstIp = _addr.sin_addr.S_un.S_addr = pg->ip_h.uiDestIp;

		memcpy(_pack->DstAddr, pg->SrcAddr, MAC_ADDR_LEN);
		memcpy(_pack->SrcAddr, pg->DstAddr, MAC_ADDR_LEN);
		_pack->EthType = IP_FLAG;
	}
	virtual ~wb_icmp_link() {
		WLOG(" ~wb_icmp_link %p\n ",this);
	}
	virtual bool create_socket() {
		return true;
	};
	virtual void close() {};
	const sockaddr* get_addr() const { return (sockaddr*)&_addr; }
	explicit operator const ICMP_KEY&()	const { return _key; }
	void* operator new(size_t size, wb_base_memory_pool& mp) { return mp.get_mem(); }
	void operator delete(void* p, wb_base_memory_pool& mp) {
		((wb_icmp_link*)p)->~wb_icmp_link();
		mp.recover_mem(p);
	}

	virtual const ETH_PACK* make_pack(const char* buffer, int len, int& pack_len) {
	
		memcpy(_ip_h, buffer, len);
		_ip_h->uiDestIp = _uSrcIp;
		//2.1����У���
		_ip_h->sCheckSum = 0x0000;
		_ip_h->sCheckSum = ip_check_sum((USHORT*)_ip_h, c_ip_head_len);
		pack_len = len + c_mac_head_len;
		return _pack;
	}
	
	// �¼�����false����ʾ�����Ӳ�����Ҫ,���ӹ��������ر����Ӳ����ʵ���ʱ�������Դ
	virtual bool OnRead(wb_filter_interface* p_fi, const ETH_PACK* pg, int len) {
		_key.usSequence = pg->icmp_h.usSequence;//�ڴ˸������
		return p_fi->post_send(this, get_ip_pack_date(pg), get_icmp_pack_date_len(pg));
	}
	virtual bool OnWrite(wb_filter_interface* p_fi, const ETH_PACK* pg, int len) {return true;}
	virtual bool OnRecv(wb_filter_interface* p_fi, const char* buf, int len, LPOVERLAPPED pol) {
		//RLOG("ICMP OnRecv %d\n", len) ;
		int out_l = 0;
		auto mk = make_pack(buf, len, out_l);
		p_fi->post_write(this, mk,out_l);
		return true;
	}
	virtual bool OnSend(wb_filter_interface* p_fi, const char* buf, int len, LPOVERLAPPED pol) {return true;}
	virtual bool OnSend_no_copy(wb_filter_interface* p_fi, char* buf, int len) {return true;}
	virtual bool Recv(LPWSABUF buffer, DWORD& dwFlags, sockaddr* addr, int* addr_len, LPOVERLAPPED pol) {return true;}
	virtual bool Send(LPWSABUF buffer, LPOVERLAPPED pol) {return true;}		
};

//TCPЭ������
/*
1. -> syn��
2. <- syn2��
3. -> ack(����Я������)	
*/


class wb_tcp_link :public wb_net_link
{
	UINT				_seq = 0 ;//��ǰά�������,���ݷ��ͳɹ����յ��Է���ȷ�Ϻ�_seq+=���ݳ���
	UINT				_next_seq=1;//�´ε�seq�����ڴ��Է���һ��_next_seq��ackֵ�������ȣ����ʾ���Է��ɹ��յ�
	UINT				_ack = 0 ;//��ǰ���Ͷ˵�ȷ����ţ����Է���seqֵ������ҳɹ����������ݣ���ȷ�ϸ�ֵ
	PTCP_HEADER			_p_tcp_h;    
	PFALS_TCP_HEADER	_p_f_tcp_h;
	char*				_p_tcp_data;
	TCP_HEADER			_tcp_h = {};
	char				_tcp_syn_data[40] = {};//tcpͷ���еĶ������ݣ������40
	char				_tcp_syn_data_len = 0 ;
	FALS_TCP_HEADER		_f_tcp_h = {};//tcpαͷ
	
	using pack_map = std::map<UINT, wb_data_pack_ex*>;
	pack_map			_p_cache ;
	wb_lock				_p_lock;
	int					_psh_num = 0;
	
	std::string			_ip;

	wb_lock				_close_lock;
	int					_close_status = 0; //1������FIN����2�ȴ�FIN�� 3�ȴ�ACK��
	time_t				_close_tm = 0;
public:
	int get_close_status() { return _close_status; };
	wb_tcp_link(const ETH_PACK* p_pack);
	virtual ~wb_tcp_link() { 
#ifdef _DEBUG
		sockaddr_in addr = {};
		addr.sin_addr.S_un.S_addr = _m_ip_h.uiSourIp;
		_ip = inet_ntoa(addr.sin_addr);
		//WLOG("tcp[%s   %d]���ض˿�:%d linke �ͷ�%p\n", _ip.c_str(), ntohs(_tcp_h.sSourPort), ntohs(_tcp_h.sDestPort), this);
#endif // DEBUG
	};
	virtual bool create_socket() {
		_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		return _socket != INVALID_SOCKET;
	}
	virtual void close() {
		//_closed = true;
		if (INVALID_SOCKET != _socket)
			closesocket(_socket);
		_socket = INVALID_SOCKET;
	}
	virtual bool OnWrite(wb_filter_interface* p_fi, const ETH_PACK* pg, int len) { 
		return true;
	}
	bool connect();
	void OnConnected(wb_filter_interface* p_fi) {
		//�������ְ�
		int out_l = 0;
		auto mk = make_pack_syn(out_l);
		p_fi->post_write(this,mk , out_l);
	}
	void OnClose(wb_filter_interface* p_fi) {
		int out_l = 0;
		auto mk = make_pack_ack(out_l, _ack);
		p_fi->post_write(this,mk , out_l);
		mk = make_pack_fin(out_l);
		p_fi->post_write(this,mk , out_l);
		
	};

	virtual bool OnRead(wb_filter_interface* p_fi, const ETH_PACK* pg, int len);

	const ETH_PACK* make_pack_syn(int& pack_len);
	const ETH_PACK* make_pack_ack(int& pack_len,UINT ack);
	const ETH_PACK* make_pack_data(const char* buf,int len,int& pack_len, char flag = TCP_DATA_ACK);
	const ETH_PACK* make_pack_fin(int& pack_len,char flag = TCP_FIN);
	virtual const ETH_PACK* make_pack(const char* buffer, int len, int& pack_len) {return _pack;}

	virtual bool OnSend_no_copy(wb_filter_interface* plink, char* buf, int len) {
		return true;
	}

	bool DoOnAck(wb_filter_interface* p_fi, const ETH_PACK* pg, int len);
	bool DoOnData(wb_filter_interface* p_fi, const char* pdata, int data_len, UINT c_seq, bool bPsh);
	bool DoOnFin(wb_filter_interface* p_fi, const ETH_PACK* pg, int len)
	{
		_ack = ntohl(pg->tcp_h.uiSequNum) + 1;
		_seq = _next_seq;
		OnClose(p_fi);
		return false;
	}

	virtual bool OnSend(wb_filter_interface* p_fi, const char* buf, int len, LPOVERLAPPED pol) { return true; };
	virtual bool OnRecv(wb_filter_interface* p_fi, const char* buf, int len, LPOVERLAPPED pol) ;

	virtual bool Recv(LPWSABUF buffer, DWORD& dwFlags, sockaddr* addr, int* addr_len, LPOVERLAPPED pol) {

		dwFlags = MSG_PARTIAL;
		if (SOCKET_ERROR == WSARecv(_socket, buffer, 1, nullptr, &dwFlags, pol, nullptr)
			&& GetLastError() != ERROR_IO_PENDING)
		{
			return false;
		}
		return true;
	}
	virtual bool Send(LPWSABUF buffer, LPOVERLAPPED pol) {
		if (SOCKET_ERROR == WSASend(_socket, buffer, 1, nullptr, 0, pol, nullptr)
			&& GetLastError() != ERROR_IO_PENDING)
		{
			return false;
		}
		return true;
	}
	virtual bool is_timeout() { return time(0) - _last_op_time > TIME_OUT_TCP; }
	time_t close_tm() { return _close_tm; }
	//flag=true,��ʾ���յ���FIN��
	bool DoClose(wb_filter_interface* p_fi,char flag) {
		//AUTO_LOCK(_close_lock);
		if (TCP_FLAG_FIN(flag))++_ack;
		_close_tm = time(0);
		switch (_close_status)
		{
			case 0:
			{
				//WLOG("%p��һ�ιر�(%d)\n",this, flag);
				close(); //WSARecv�᷵�أ�post_recv��post_send��ʧ��
				if (TCP_FLAG_RST(flag))
				{
					_close_status = 4;
					return true;
				}
				int out_l = 0;
				if (TCP_FLAG_FIN(flag))
				{
					auto pk = make_pack_ack(out_l, _ack);
					p_fi->post_write(this, pk, out_l);
					_close_status = 4;//�������ɶϿ��İ��ظ�
				}
				else
				{
					_close_status = 1;
					auto pk = make_pack_fin(out_l, TCP_RST_ACK );
					p_fi->post_write(this, pk, out_l);
					return false;
				}
				auto pk = make_pack_fin(out_l,TCP_FIN);
				p_fi->post_write(this, pk, out_l);
				for (auto i : _p_cache)
				{
					p_fi->back_data_pack_ex(i.second);
				}
				_p_cache.clear();
			}break;
			case 1:_close_status = 2; 
			case 2:_close_status = 3; //�Ͽ���ȷ�ϰ������Բ��ô���
			case 3: 
			{
				int out_l = 0;
				auto pk = make_pack_ack(out_l, _ack);
				p_fi->post_write(this, pk, out_l);
				return false;
			}break;
			default:return false;
		}
		return true;
	};
};