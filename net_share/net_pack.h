#pragma once
#include <winsock2.h>
#include "net_base.h"

constexpr int c_mac_head_len	= sizeof(ETH_HEADER);	//֡ͷ����
constexpr int c_ip_head_len		= sizeof(IP_HEADER);	//ipͷ����
constexpr int c_udp_head_len	= sizeof(UDP_HEADER);	//udpͷ����
constexpr int c_tcp_head_len	= sizeof(TCP_HEADER);	//tcp����
constexpr int c_icmp_head_len   = sizeof(ICMP_HEADER);	//icmp����

constexpr int c_udp_fhead_len	= sizeof(FALS_UDP_HEADER);//udpαͷ������
constexpr int c_tcp_fhead_len	= sizeof(FALS_TCP_HEADER);//tcpαͷ������

constexpr int c_mac_data_size	= sizeof(ETH_HEADER);	//��̫������ƫ����
constexpr int c_ip_head_size	= sizeof(ETH_HEADER) + sizeof(IP_HEADER);	//ip��ͷ�ܳ���
constexpr int c_udp_head_size	= sizeof(ETH_UDP_PACK);	//udp��ͷ�ܳ���
constexpr int c_tcp_head_size	= sizeof(ETH_TCP_PACK);	//tcp��ͷ�ܳ���

#define MAC_DATA_LEN_MAX  DEFAULT_MAC_LEN

constexpr int c_ip_data_len_max = MAC_DATA_LEN_MAX - c_ip_head_len;//ip��������� ����ipͷ����
constexpr int c_udp_data_len_max= c_ip_data_len_max - c_udp_head_len;//udp��������� ����udpͷ����
constexpr int c_tcp_data_len_max= c_ip_data_len_max - c_tcp_head_len;//udp��������� ����udpͷ����

constexpr int c_ip_frag_offset_base = c_ip_data_len_max / 8 ;//ip��Ƭƫ�Ƶ�λ,��Ƭֵ/c_ip_frag_offset_base �õ���Ƭ������ֵ

#define TCP_FLAG(x) (x&0x3F)
#define TCP_FLAG_FIN(x) (x&0x01)
#define TCP_FLAG_SYN(x) (x&0x02)
#define TCP_FLAG_RST(x) (x&0x04)
#define TCP_FLAG_PSH(x) (x&0x08)
#define TCP_FLAG_ACK(x) (x&0x0F)
//���ڿ��ٲ��Ҵ��������Ӧ�����Ӷ����key�ṹ

#pragma	 pack (push,1)
union ustrt_net_base_info {

	struct
	{
		unsigned int src_ip;
		unsigned int dst_ip;

		unsigned short src_port;
		unsigned short dst_port;

	}ip_info;

	struct
	{
		unsigned __int64		ip;
		unsigned int			port;
	}big_info;

	bool operator < (const ustrt_net_base_info& nbi)const {
#if 1
		if (big_info.ip == nbi.big_info.ip)
			return big_info.port < nbi.big_info.port;
		return big_info.ip < nbi.big_info.ip;
#else	
		if (ip_info.src_port == nbi.ip_info.src_port)
		{
			if (ip_info.dst_port == nbi.ip_info.dst_port)
			{
				if (ip_info.dst_ip == nbi.ip_info.dst_ip)
					return (ip_info.dst_port < nbi.ip_info.src_ip);
				else
					return ip_info.dst_ip < nbi.ip_info.dst_ip;
			}
			else
				return ip_info.dst_port < nbi.ip_info.dst_port
		}
		else
			ip_info.src_port < nbi.ip_info.src_port;
#endif
		return false;
	}
};
#pragma pack(pop)
#define ETH_PACK_TO_net_base_info(x) ((ustrt_net_base_info*)&x.ip_h.uiSourIp)

#ifdef _DEBUG
#define WLOG(...) { \
	printf(__VA_ARGS__) ; \
}
#else
#define WLOG(...){}
#endif


#define RLOG(...) { \
	printf(__VA_ARGS__) ; \
}

/*
#define NLOG(...) { \
	system("cls") ;\
	printf(__VA_ARGS__) ; \
}
*/

#define NLOG(...) { printf(__VA_ARGS__) ; }


//����У���
inline unsigned short ip_check_sum(unsigned short* a, int len)
{
	unsigned int sum = 0;
	while (len > 1) {
		sum += *a++;
		len -= 2;
	}
	if (len) {
		sum += *(unsigned char*)a;
	}
	while (sum >> 16) {
		sum = (sum >> 16) + (sum & 0xffff);
	}
	return (unsigned short)(~sum);
}
//��ȡ����ip�����ȣ�����ipͷ
inline int get_ip_pack_len(const ETH_PACK* pg) { return ntohs(pg->ip_h.sTotalLenOfPack); }
//��ȡudp�����ݳ���
inline int get_udp_pack_date_len(const ETH_PACK* pk) {return ntohs(pk->udp_h.usLen) - c_udp_head_len;}
//��ȡtcp�����ݳ���
inline int get_tcp_pack_date_len(const ETH_PACK* pg) { return ntohs(pg->ip_h.sTotalLenOfPack) - c_ip_head_len - (pg->tcp_h.cHeaderLenAndFlag >> 4 & 0x0F) * 4; }
//��ȡicmp�����ݳ���
inline int get_icmp_pack_date_len(const ETH_PACK* pg) { return ntohs(pg->ip_h.sTotalLenOfPack) - c_ip_head_len; }


//��ȡudp��������
inline const char* get_udp_pack_date(const ETH_PACK* pg) { return (const char*)pg + c_udp_head_size; }
//��ȡtcp��������
inline const char* get_tcp_pack_date(const ETH_PACK* pg,int len,int data_len) { return (const char*)pg + (len - data_len); }
//��ȡip������
inline const char* get_ip_pack_date(const ETH_PACK* pg) { return (const char*)pg + c_ip_head_size; }


struct wb_data_pack
{
	USHORT usPackLen;//udp����ʵ�ʳ���
	//USHORT usPackId;
	USHORT usLen;	//udp��������
	char   DF;	  //0��Ƭ�� 1�Ƿ�Ƭ
	char   MF_Idx;//��Ƭ����
	char   data[c_ip_data_len_max];//����
};

struct wb_data_pack_ex
{
	bool	bPsh;//false��ʾ���ǻ���ģ���������Ҫ�����ύ��
	int		len;
	char	data[DEFAULT_RECV_SIZE];//����
};


struct wb_link_info {

	ustrt_net_base_info ip_info;
	unsigned char  mac[MAC_ADDR_LEN];//�û�����
};


struct wb_flow_info {
	INT64		send_bytes;
	INT64		recv_bytes;

	INT64		read_packs;
	INT64		write_packs;
};

__interface wb_link_interface;//���Ӷ���
__interface wb_filter_interface;//����������
__interface wb_net_card_interface;//��������
__interface wb_machine_interface;//�����ӿ�

__interface wb_machine_event;
__interface wb_filter_event;//�������¼�



//���ӽӿ�
__interface wb_link_interface
{
	virtual bool create_socket() = 0;
	virtual void close() = 0;

	virtual bool Recv(LPWSABUF buffer, DWORD& dwFlags,sockaddr* addr,int* addr_len, LPOVERLAPPED pol) = 0;
	virtual bool Send(LPWSABUF buffer, LPOVERLAPPED pol) = 0;

	virtual const wb_link_info& get_link_info() const = 0;
	virtual byte get_Protocol() const = 0 ;
	virtual void set_event(wb_machine_event* ev) = 0;
	virtual wb_machine_event* get_event()const  = 0;
/*
* OnRead,OnWrite,
* �¼�����false����ʾ�����Ӳ�����Ҫ,���ӹ��������ر����Ӳ����ʵ���ʱ�������Դ
*/
	virtual bool OnWrite(wb_filter_interface* p_fi, const ETH_PACK* pg, int len) = 0;//������д�������
	virtual bool OnRead(wb_filter_interface* p_fi, void* const lp_link, const ETH_PACK* pg, int len) = 0;//��������ȡ�������,len�����ܳ���

	virtual bool OnRecv(wb_filter_interface* p_fi, void* const lp_link, const char* buf, int len, LPOVERLAPPED pol) = 0;//��������ȡ�������,len�����ܳ���
	virtual bool OnSend(wb_filter_interface* p_fi, void* const lp_link, const char* buf, int len, LPOVERLAPPED pol) = 0;//�������д�������
	virtual bool OnSend_no_copy(wb_filter_interface* p_fi, void* const lp_link, char* buf, int len) = 0;//���ݲ����п���

	virtual void set_data(void* pd)=0 ;
	virtual void* get_data()=0;
};

//�������ӿ�
__interface wb_filter_interface
{
	virtual bool post_send(wb_link_interface* plink, void* const lp_link, const char* buf, int len) = 0;
	virtual bool post_send_ex(wb_link_interface* plink, void* const lp_link, const char* buf, int len,void* pd) = 0;
	virtual bool post_send_no_copy(wb_link_interface* plink, void* const lp_link, char* buf, int len) = 0;//���ݲ����п���
	virtual bool post_recv(wb_link_interface* plink, void* const lp_link, LPOVERLAPPED pol) = 0;

	virtual wb_data_pack* get_data_pack() = 0;//��ȡһ�����ݰ��ṹ
	virtual void back_data_pack(wb_data_pack*) = 0;//�黹���ݰ��ṹ

	virtual wb_data_pack_ex* get_data_pack_ex() = 0;//��ȡһ�����ݰ��ṹ
	virtual void back_data_pack_ex(wb_data_pack_ex*) = 0;//�黹���ݰ��ṹ

	virtual void post_write(wb_link_interface* plink, const ETH_PACK* pk, int len, int data_len = 0) = 0;

	virtual const wb_flow_info get_flow_info() const = 0;

	//virtual const get_links() = 0 ;
};

//�����������¼�
__interface wb_filter_event
{
	virtual void OnRead(const ETH_PACK* pg, int len) = 0;//��������ȡ�������,len�����ܳ���
	virtual void OnWrite(wb_link_interface* plink, const ETH_PACK* pg, int len) = 0;//������д�������
};

//�����ӿ�
__interface wb_net_card_interface
{
	//�����ӿ�
	virtual void post_write(wb_filter_event* p_nce, wb_link_interface* plink, const ETH_PACK* pk, int pack_len,int data_len=0) = 0;
};


__interface wb_machine_event 
{
	virtual void OnRead(const wb_link_interface* lk, int len)=0;
	virtual void OnWrite(const wb_link_interface* lk, int len)=0;

	virtual void OnSend(const wb_link_interface* lk, int len)=0;
	virtual void OnRecv(const wb_link_interface* lkc, int len)=0;
};

__interface wb_machine_interface
{
	virtual void AddLink(wb_link_interface* lk) = 0 ;
	virtual void DelLink(const wb_link_interface* lk) = 0;
};
