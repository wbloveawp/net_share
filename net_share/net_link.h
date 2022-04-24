#pragma once
#include "net_pack.h"
#include "base_component.h"
#include <ostream>
#include <map>
#include <assert.h>

class wb_net_link:public wb_link_interface {

protected:
	char					_write_buf[MAX_MAC_LEN] = {};//ÿ������ÿ��ֻ������Ͷ��һ����̫��֡
	ETH_PACK*				_pack = nullptr;	//���Կ��ٸ����������
	IP_HEADER*				_ip_h = nullptr;
	IP_HEADER				_m_ip_h = {};
	SOCKET					_socket=INVALID_SOCKET;//
	USHORT					_pack_id;//��ǰ�İ���
	std::atomic_int64_t		_last_op_time = 0;

	std::atomic<int>		_ref = 0;//���ü�����

	wb_link_info			_info;

	ustrt_net_base_info&	_nbi= _info.ip_info;

	void*					_pd;
	//����ͳ��
	std::atomic<INT64>		_recv_bytes=0;
	std::atomic<INT64>		_send_bytes=0;
public:
	wb_net_link(const ETH_PACK* pg);

	virtual ~wb_net_link() {}

	explicit operator const ETH_PACK* ()const	{ return _pack; }
	explicit operator		ETH_PACK* ()		{ return _pack; }
	explicit operator const SOCKET()	const   { return _socket;}
	explicit operator const ustrt_net_base_info& () const { return _nbi; }


	virtual void set_data(void* pd) { _pd = pd; };
	virtual void* get_data() { return _pd; };
	virtual const wb_link_info& get_link_info() const { return _info; };

	inline int AddRef() { return ++_ref;}
	inline int DelRef() { return --_ref; }
	inline int Ref() { return _ref.load(); }

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
	
	virtual bool OnRecv(wb_filter_interface* p_fi, void* const lp_link, const char* buf, int len, LPOVERLAPPED pol) {
		
		_recv_bytes += len;
		return true;
	};//��������ȡ�������,len�����ܳ���
	virtual bool OnSend(wb_filter_interface* p_fi, void* const lp_link, const char* buf, int len, LPOVERLAPPED pol) {
		//WLOG("udp���ݷ������:%d\n", len);
		_send_bytes += len;
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

	virtual wb_link_send_recv get_send_recv() const
	{
		return { _send_bytes ,_recv_bytes };
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

public:
	wb_udp_link(const ETH_PACK* pg, const ustrt_net_base_info* pif);
	virtual ~wb_udp_link() { 
		close();
	}
	virtual bool OnRead(wb_filter_interface* p_fi, void* const lp_link, const ETH_PACK* pk, int len);
	virtual bool OnSend_no_copy(wb_filter_interface* plink, void* const lp_link, char* buf, int len) { _send_bytes += len; return true; };
	virtual bool OnRecv(wb_filter_interface* p_fi, void* const lp_link, const char* buf, int len, LPOVERLAPPED pol);
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

	virtual byte get_Protocol() const { return PRO_UDP; } ;
};

//ԭʼicmp socket ȫ��ֻ�ܴ���һ��
#pragma pack(push,1)
struct  ICMP_LINK_KEY
{
	USHORT					usID;//��ʶ��
	union {
		unsigned __int64	ip64;//ping��Ŀ��ip
		struct {
			UINT	uiSrcIp;
			UINT	uiDstIp;
		}detail;
	};
	bool operator <(const ICMP_LINK_KEY& ik) const {
		if (ip64 == ik.ip64)
			return usID < ik.usID;
		return ip64 < ik.ip64;
	}
};

struct  ICMP_PACK_KEY
{
	long long			tm;//��������ʱ��
	UINT				uiDstIp;//ping��Ŀ��ip
	union {
		UINT				uID;
		struct {
			USHORT				usSequence;//�����
			USHORT				usID;//��ʶ��
		}detail;
	};

	bool operator <(const ICMP_PACK_KEY& ik) const{
		if (uiDstIp == ik.uiDstIp)
			return uID < ik.uID;
		return uiDstIp < ik.uiDstIp;
	}
};
#pragma pack(pop)

class wb_icmp_link : public wb_link_interface
{
	char				_write_buf[ICMP_BUF_SIZE] = {};
	ETH_PACK*			_pack = nullptr;	//���Կ��ٸ����������
	IP_HEADER*			_ip_h = nullptr;
	char*				_p_icmp_data= nullptr;// = nullptr;

	sockaddr_in			_addr = {};

	ICMP_LINK_KEY		_key = {};

	std::atomic_int64_t	_last_op_time = 0;
	std::atomic<int>	_ref = 0;
	wb_link_info		_info;//�����壬�ӿ�ʵ����

public:

	virtual const wb_link_info& get_link_info()const { return _info; };
	virtual wb_link_send_recv get_send_recv() const { return {0,0}; }
	wb_icmp_link(const ETH_PACK* pg) {
		auto pif = (ustrt_net_base_info*)&pg->ip_h.uiSourIp;
		_key.ip64 = pif->big_info.ip;
		_key.usID = pg->icmp_h.usID;
		_pack = (ETH_PACK*)_write_buf;
		_ip_h = &_pack->ip_h;
		_p_icmp_data = (char*)_ip_h + c_ip_head_len;
		_addr.sin_family = AF_INET;
		_addr.sin_addr.S_un.S_addr = pg->ip_h.uiDestIp;

		memcpy(_pack->DstAddr, pg->SrcAddr, MAC_ADDR_LEN);
		memcpy(_pack->SrcAddr, pg->DstAddr, MAC_ADDR_LEN);
		_pack->EthType = IP_FLAG;
		_last_op_time = time(0);
		WLOG(" wb_icmp_link %p\n ", this);
	}
	virtual ~wb_icmp_link() {
		WLOG(" ~wb_icmp_link %p\n ",this);
	}
	inline int AddRef() { return ++_ref; }
	inline int DelRef() { return --_ref; }
	inline int Ref() { return _ref.load(); }
	virtual bool create_socket() {return true;};
	virtual void close() {};
	const sockaddr* get_addr() const { return (sockaddr*)&_addr; }
	explicit operator const ICMP_LINK_KEY&()	const { return _key; }
	void* operator new(size_t size, wb_base_memory_pool& mp) { return mp.get_mem(); }
	void operator delete(void* p, wb_base_memory_pool& mp) {
		((wb_icmp_link*)p)->~wb_icmp_link();
		mp.recover_mem(p);
	}

	virtual const ETH_PACK* make_pack(const char* buffer, int len, int& pack_len) {
		constexpr int s_size = ICMP_BUF_SIZE - sizeof(ETH_HEADER);
		if (len > s_size)len = s_size;
		memcpy(_ip_h, buffer, len);
		_ip_h->uiDestIp = _key.detail.uiSrcIp;//  _uSrcIp;
		//2.1����У���
		_ip_h->sCheckSum = 0x0000;
		_ip_h->sCheckSum = ip_check_sum((USHORT*)_ip_h, c_ip_head_len);
		pack_len = len + c_mac_head_len;
		return _pack;
	}
	
	// �¼�����false����ʾ�����Ӳ�����Ҫ,���ӹ��������ر����Ӳ����ʵ���ʱ�������Դ
	virtual bool OnRead(wb_filter_interface* p_fi, void* const lp_link, const ETH_PACK* pg, int len) {
		_last_op_time = time(0);
		ICMP_PACK_KEY pk = { _last_op_time.load() ,_key.detail.uiDstIp ,pg->icmp_h.usSequence  };
		pk.detail.usID = _key.usID;
		return p_fi->post_send_ex(this, lp_link,get_ip_pack_date(pg), get_icmp_pack_date_len(pg),&pk);
	}
	virtual bool OnWrite(wb_filter_interface* p_fi, const ETH_PACK* pg, int len) {return true;}
	virtual bool OnRecv(wb_filter_interface* p_fi, void* const lp_link, const char* buf, int len, LPOVERLAPPED pol) {
		_last_op_time = time(0);
		int out_l = 0;
		auto mk = make_pack(buf, len, out_l);
		p_fi->post_write(this, mk,out_l);
		return true;
	}
	virtual bool OnSend(wb_filter_interface* p_fi, void* const lp_link, const char* buf, int len, LPOVERLAPPED pol) {return true;}
	virtual bool OnSend_no_copy(wb_filter_interface* p_fi, void* const lp_link, char* buf, int len) {return true;}
	virtual bool Recv(LPWSABUF buffer, DWORD& dwFlags, sockaddr* addr, int* addr_len, LPOVERLAPPED pol) {return true;}
	virtual bool Send(LPWSABUF buffer, LPOVERLAPPED pol) {return true;}		

	virtual bool is_timeout() {return time(0) -  _last_op_time> TIME_OUT_ICMP;}
	virtual byte get_Protocol() const { return PRO_ICMP; };

	virtual void set_data(void* pd) {};
	virtual void* get_data() { return nullptr; };
};

//TCPЭ������

class wb_tcp_link :public wb_net_link
{
	UINT				_seq = 0 ;//��ǰά�������,���ݷ��ͳɹ����յ��Է���ȷ�Ϻ�_seq+=���ݳ���
	UINT				_next_seq=1;//�´ε�seq�����ڴ��Է���һ��_next_seq��ackֵ�������ȣ����ʾ���Է��ɹ��յ�
	UINT				_ack = 0 ;//��ǰ���Ͷ˵�ȷ����ţ����Է���seqֵ������ҳɹ����������ݣ���ȷ�ϸ�ֵ
	UINT				_winscal = 0;
	PTCP_HEADER			_p_tcp_h;    
	PFALS_TCP_HEADER	_p_f_tcp_h;
	char*				_p_tcp_data;
	TCP_HEADER			_tcp_h = {};
	char				_tcp_syn_data[40] = {};//tcpͷ���еĶ������ݣ������40
	char				_tcp_syn_data_len = 0 ;
	FALS_TCP_HEADER		_f_tcp_h = {};//tcpαͷ
	
	using pack_map = std::map<UINT, wb_data_pack_ex*>;
	pack_map			_p_cache ;
	int					_psh_num = 0;
	
	std::string			_ip;

	std::atomic<int>	_close_status = 0; //1������FIN����2�ȴ�FIN�� 3�ȴ�ACK��
	time_t				_close_tm = 0;
	
	unsigned short		_next_win_max;
	UINT				_last_ack;
	LPOVERLAPPED		_pol=nullptr;

	wb_lock				_pack_lock;

	char				_data_buf[TCP_RECV_SIZE*2];
	int					_data_len = 0;
#ifdef _DEBUG
	UINT _s_ack = 0;
#endif	
public:
	int get_close_status() { return _close_status; };
	wb_tcp_link(const ETH_PACK* p_pack);
	virtual ~wb_tcp_link() { 
#ifdef _DEBUG
		sockaddr_in addr = {};
		addr.sin_addr.S_un.S_addr = _m_ip_h.uiSourIp;
		_ip = inet_ntoa(addr.sin_addr);
		WLOG("tcp[%s   %d]���ض˿�:%d linke �ͷ�%p\n", _ip.c_str(), ntohs(_tcp_h.sSourPort), ntohs(_tcp_h.sDestPort), this);
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
	virtual bool OnRead(wb_filter_interface* p_fi, void* const lp_link, const ETH_PACK* pg, int len);
	virtual bool OnRecv(wb_filter_interface* p_fi, void* const lp_link, const char* buf, int len, LPOVERLAPPED pol);

	const ETH_PACK* make_pack_syn(int& pack_len);
	const ETH_PACK* make_pack_ack(int& pack_len,UINT ack);
	const ETH_PACK* make_pack_data(const char* buf,int len,int& pack_len, char flag = TCP_DATA_ACK);
	const ETH_PACK* make_pack_fin(int& pack_len,char flag = TCP_FIN);
	virtual const ETH_PACK* make_pack(const char* buffer, int len, int& pack_len) {return _pack;}

	virtual bool OnSend_no_copy(wb_filter_interface* plink, void* const lp_link, char* buf, int len) {
		WLOG("tcp %p OnSend_no_copy buf=%p\n" , this, buf);
		_send_bytes += len;
		delete[]buf;
		return true;
	}

	bool DoOnAck(wb_filter_interface* p_fi, void* const lp_link, const ETH_PACK* pg, int len);
	bool DoOnData(wb_filter_interface* p_fi, void* const lp_link, const char* pdata, int data_len, UINT c_seq, bool bPsh);

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

	virtual byte get_Protocol() const { return PRO_TCP; };
};
