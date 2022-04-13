#pragma once
//定义广播包
#define UDP_BROADCAST_IP		0xFF000000
//定义组播包
#define MULTICAST_IP_MIN		0xE0000000
//超时定义
#ifdef _DEBUG
#define TIME_OUT_UDP			5		//udp超时单位秒,超过这个时间，释放连接
#define TIME_OUT_TCP			30
#define TIME_OUT_ICMP			10		//单位s
#else
#define TIME_OUT_UDP			60		//udp超时单位秒,超过这个时间，释放连接
#define TIME_OUT_TCP			120
#define TIME_OUT_ICMP			10		//单位s
#endif

//#define VER_2016_10_21		

#define UDP_DEBUG				0
#define ICMP_DEBUG				0
#define TCP_DEBUG				0

#define ETH_MIN_LEN				60

#define MAC_ADDR_LEN			6		
#define MAC_DESCSTR_LEN			128
#define IEEE8023_MIN_MAC_SIZE	66		//IEEE8023 最小帧的长度
#define DEFAULT_RECV_SIZE		1460	//默认收数据的BUF，为网卡帧大小减去IP头+TCP头(或UDP头)
#define DEFAULT_MAC_LEN			1500	//默认网卡最大发送帧为1500字节,不包括以太网帧头与其尾，即IP头+数据
#define MAX_MAC_LEN				1514	//一个完整的以太网帧最大长度1500+帧头14字节+4尾部CRC
#define DEFAULT_BUFF_SIZE		2048	//内存池默认的缓冲区大小
#define ICMP_BUF_SIZE			512

//recv 缓冲区大小定义
#define TCP_RECV_SIZE			8192
#define UDP_RECV_SIZE			(1024*10)
#define ICMP_RECV_SIZE			DEFAULT_RECV_SIZE
#define TCP_PACK_SIZE			1440
 
//IP包类型及协议
#define	IP_FLAG					0x0008	//IP包
#define	PRO_ICMP				0x01	//ICMP协议
#define	PRO_UDP					0x11	//UDP协议
#define	PRO_TCP					0x06	//TCP协议

//TCP包类型
#define TCP_SYN1				0x02	//TCP握手包,第一次握手包
#define TCP_SYN2				0x12	//TCP握手确认包,第二次握手包
#define TCP_RST					0x04	//TCP握手确认包,第二次握手包
#define TCP_RST_ACK				0x14	//重设包,当FIN包处理
#define TCP_DATA_ACK			0x18	//TCP PSH数据包
#define	TCP_ACK					0x10	//TCP 确认包,可能含有数据
#define TCP_FIN					0x11	//TCP断开确认包
//TCP包头默认长度	
#define TCP_HEAD_LEN			20		
#define IP_VER_LEN				0x45

//包类型定义
#define	PACK_NO_NEED			0		//不需要的包
#define	PACK_NO_IP				1		//非IP包
#define	PACK_NO_UDP_TCP_ICMP	2		//为IP包，但非UDP，TCP，ICMP包
#define	PACK_IN_LAN				3		//为UDP，TCP，ICMP，且非ssl包但目标地址为本网段
#define	PACK_IS_NEED			4		//需要过滤的包，为UDP,TCP，ICMP包，且目标地址非本网段，源地址在本网段内
#define PACK_IS_SSL				5		//此包端口为443，即为ssl协议包


#define PACK_UDP_DATA		0x00
#define PACK_ICMP_DATA		0x01
#define PACK_TCP_SYN1		TCP_SYN1
#define PACK_TCP_SYN2		TCP_SYN2
#define PACK_TCP_RST_ACK	TCP_RST_ACK	
#define PACK_TCP_ACK		TCP_ACK
#define PACK_TCP_FIN		TCP_FIN
#define PACK_TCP_DATA_ACK	TCP_DATA_ACK //数据包+确认
#define PACK_TCP_DATA_ONLY	TCP_DATA_ONLY//仅仅是数据包


//连接超时定义
#define NETLINK_COUNT_MAX	2048
#define TIMER_INTERVAL		10000  //定时器间隔10000ms,即10秒
#define NETLINK_TIME_OUT	20000 //单位毫秒，网络连接超时时限	

//默认 MSS大小
#define DEFAULT_MMS			0x0218
//MMS最大值为最大帧1500减去帧头和IP头=1460
#define MAX_MMS				0x05B4
//默认窗口大小
#define DEFAULT_WINDOW_SIZE	(short)0xFFFF	
//默认ICMP超时 1s
#define ICMP_DEFAULT_TIMEOUT	1000 
//默认TTL
#define DEFAULT_TTL			(char)0x75//(char)0x80
//默认TCP头部长度
#define TCP_HEADER_LEN		20

#define RECV_SIZE			8192



#define OUT_IOCTL_NDISPROT_OPEN_DEVICE				0x12c800	//打开设备
#define OUT_IOCTL_NDISPROT_QUERY_OID_VALUE			0x12c804	//查询OID
#define OUT_IOCTL_NDISPROT_SET_OID_VALUE			0x12c814	//设置OID
#define OUT_IOCTL_NDISPROT_QUERY_BINDING			0x12c80c	//查询绑定
#define OUT_IOCTL_NDISPROT_BIND_WAIT				0x12c810	//等待绑定
#define OUT_IOCTL_NDISPROT_INDICATE_STATUS			0x12c818	//
#define OUT_IOCTL_NDISPROT_GET_DEVICE_LIST			0x12c81c	//获取网卡设备列表
#define OUT_IOCTL_NDISPROT_SET_HOST_SECCION			0x12c820	//设置过滤
#define OUT_IOCTL_NDISPROT_GET_NETWORK_BASE_INFO	0x12c824	//获取网卡基本信息
#define OUT_IOCTL_NDISPROT_GET_NETWORK_TRANS_INFO	0x12c828	//获取网卡传输信息
#define OUT_IOCTL_NDISPROT_CLOSE_DEVICE				0x12c82c	//关闭设备


#define NDIS_DRIVER_NAME	"\\\\.\\wbndis"