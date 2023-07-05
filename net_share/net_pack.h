#pragma once
#include <winsock2.h>
#include "net_base.h"

constexpr int c_mac_head_len	= sizeof(ETH_HEADER);	//帧头长度
constexpr int c_ip_head_len		= sizeof(IP_HEADER);	//ip头长度
constexpr int c_udp_head_len	= sizeof(UDP_HEADER);	//udp头长度
constexpr int c_tcp_head_len	= sizeof(TCP_HEADER);	//tcp长度
constexpr int c_icmp_head_len   = sizeof(ICMP_HEADER);	//icmp长度

constexpr int c_udp_fhead_len	= sizeof(FALS_UDP_HEADER);//udp伪头部长度
constexpr int c_tcp_fhead_len	= sizeof(FALS_TCP_HEADER);//tcp伪头部长度

constexpr int c_mac_data_size	= sizeof(ETH_HEADER);	//以太网数据偏移量
constexpr int c_ip_head_size	= sizeof(ETH_HEADER) + sizeof(IP_HEADER);	//ip包头总长度
constexpr int c_udp_head_size	= sizeof(ETH_UDP_PACK);	//udp包头总长度
constexpr int c_tcp_head_size	= sizeof(ETH_TCP_PACK);	//tcp包头总长度

#define MAC_DATA_LEN_MAX  DEFAULT_MAC_LEN

constexpr int c_ip_data_len_max = MAC_DATA_LEN_MAX - c_ip_head_len;//ip包数据最大长 不含ip头长度
constexpr int c_udp_data_len_max= c_ip_data_len_max - c_udp_head_len;//udp包数据最大长 不含udp头长度
constexpr int c_tcp_data_len_max= c_ip_data_len_max - c_tcp_head_len;//udp包数据最大长 不含udp头长度

constexpr int c_ip_frag_offset_base = c_ip_data_len_max / 8 ;//ip分片偏移单位,分片值/c_ip_frag_offset_base 得到分片的索引值

#define TCP_FLAG(x) (x&0x3F)
#define TCP_FLAG_FIN(x) (x&0x01)
#define TCP_FLAG_SYN(x) (x&0x02)
#define TCP_FLAG_RST(x) (x&0x04)
#define TCP_FLAG_PSH(x) (x&0x08)
#define TCP_FLAG_ACK(x) (x&0x0F)
//用于快速查找此网络包对应的连接对象的key结构

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


//计算校验和
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
//获取整个ip包长度，包含ip头
inline int get_ip_pack_len(const ETH_PACK* pg) { return ntohs(pg->ip_h.sTotalLenOfPack); }
//获取udp包数据长度
inline int get_udp_pack_date_len(const ETH_PACK* pk) {return ntohs(pk->udp_h.usLen) - c_udp_head_len;}
//获取tcp包数据长度
inline int get_tcp_pack_date_len(const ETH_PACK* pg) { return ntohs(pg->ip_h.sTotalLenOfPack) - c_ip_head_len - (pg->tcp_h.cHeaderLenAndFlag >> 4 & 0x0F) * 4; }
//获取icmp包数据长度
inline int get_icmp_pack_date_len(const ETH_PACK* pg) { return ntohs(pg->ip_h.sTotalLenOfPack) - c_ip_head_len; }


//获取udp包的数据
inline const char* get_udp_pack_date(const ETH_PACK* pg) { return (const char*)pg + c_udp_head_size; }
//获取tcp包的数据
inline const char* get_tcp_pack_date(const ETH_PACK* pg,int len,int data_len) { return (const char*)pg + (len - data_len); }
//获取ip包数据
inline const char* get_ip_pack_date(const ETH_PACK* pg) { return (const char*)pg + c_ip_head_size; }


struct wb_data_pack
{
	USHORT usPackLen;//udp数据实际长度
	//USHORT usPackId;
	USHORT usLen;	//udp整包长度
	char   DF;	  //0分片包 1非分片
	char   MF_Idx;//分片索引
	char   data[c_ip_data_len_max];//数据
};

struct wb_data_pack_ex
{
	bool	bPsh;//false表示已是缓存的，否则是需要立马提交的
	int		len;
	char	data[DEFAULT_RECV_SIZE];//数据
};


struct wb_link_info {

	ustrt_net_base_info ip_info;
	unsigned char  mac[MAC_ADDR_LEN];//该机器码
};


struct wb_flow_info {
	INT64		send_bytes;
	INT64		recv_bytes;

	INT64		read_packs;
	INT64		write_packs;
};

__interface wb_link_interface;//连接对象
__interface wb_filter_interface;//过滤器对象
__interface wb_net_card_interface;//网卡对象
__interface wb_machine_interface;//主机接口

__interface wb_machine_event;
__interface wb_filter_event;//过滤器事件



//连接接口
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
* 事件返回false，表示本连接不再需要,连接管理器将关闭连接并在适当的时候回收资源
*/
	virtual bool OnWrite(wb_filter_interface* p_fi, const ETH_PACK* pg, int len) = 0;//从网卡写完网络包
	virtual bool OnRead(wb_filter_interface* p_fi, void* const lp_link, const ETH_PACK* pg, int len) = 0;//从网卡读取到网络包,len：包总长度

	virtual bool OnRecv(wb_filter_interface* p_fi, void* const lp_link, const char* buf, int len, LPOVERLAPPED pol) = 0;//从网络层读取到网络包,len：包总长度
	virtual bool OnSend(wb_filter_interface* p_fi, void* const lp_link, const char* buf, int len, LPOVERLAPPED pol) = 0;//从网络层写完网络包
	virtual bool OnSend_no_copy(wb_filter_interface* p_fi, void* const lp_link, char* buf, int len) = 0;//数据不进行拷贝

	virtual void set_data(void* pd)=0 ;
	virtual void* get_data()=0;
};

//过滤器接口
__interface wb_filter_interface
{
	virtual bool post_send(wb_link_interface* plink, void* const lp_link, const char* buf, int len) = 0;
	virtual bool post_send_ex(wb_link_interface* plink, void* const lp_link, const char* buf, int len,void* pd) = 0;
	virtual bool post_send_no_copy(wb_link_interface* plink, void* const lp_link, char* buf, int len) = 0;//数据不进行拷贝
	virtual bool post_recv(wb_link_interface* plink, void* const lp_link, LPOVERLAPPED pol) = 0;

	virtual wb_data_pack* get_data_pack() = 0;//获取一个数据包结构
	virtual void back_data_pack(wb_data_pack*) = 0;//归还数据包结构

	virtual wb_data_pack_ex* get_data_pack_ex() = 0;//获取一个数据包结构
	virtual void back_data_pack_ex(wb_data_pack_ex*) = 0;//归还数据包结构

	virtual void post_write(wb_link_interface* plink, const ETH_PACK* pk, int len, int data_len = 0) = 0;

	virtual const wb_flow_info get_flow_info() const = 0;

	//virtual const get_links() = 0 ;
};

//过滤器接收事件
__interface wb_filter_event
{
	virtual void OnRead(const ETH_PACK* pg, int len) = 0;//从网卡读取到网络包,len：包总长度
	virtual void OnWrite(wb_link_interface* plink, const ETH_PACK* pg, int len) = 0;//从网卡写完网络包
};

//网卡接口
__interface wb_net_card_interface
{
	//方法接口
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
