#pragma once
#include "base_def.h"
#pragma pack(push,1)
//IP4协议，后续需要增加IP6协议
//网卡基本信息
typedef struct __BASE_NET_INFO
{
	unsigned long				ulMaxFrameSize;   //网卡传输帧最大值，IP包要小于等于此值,默认都是1500
	unsigned char               szCurrentAddress[MAC_ADDR_LEN];
	unsigned char				szDeviceDescr[MAC_DESCSTR_LEN];

}BASE_NET_INFO, * PBASE_NET_INFO;

//过滤信息
typedef struct _NET_IP_FILTER
{
	char				cFlag;	//0==ip  1==mac
	unsigned int		uiNetSeccion; //IP段
	unsigned int		uiNetMasks;   //子网掩码

	unsigned int		iCount;
	unsigned int		uiMacSeccion[1];//MAC地址,可以传输多个mac地址配置

}NET_IP_FILTER, * PNET_IP_FILTER;
//以太网头
typedef struct __ETH_HEADER
{
	unsigned char       szDstAddr[MAC_ADDR_LEN];
	unsigned char       szSrcAddr[MAC_ADDR_LEN];
	unsigned short      usEthType;

} ETH_HEADER, * PETH_HEADER;

//IP头
typedef struct _IP_HEADER
{
	char				cVersionAndHeaderLen;   //版本信息(前4位)，头长度(后4位,表示占用32bit的位数，所以长度等于值*4，tcp头最大长度为60) 对于ip4填充'E'即可,
	char				cTypeOfService;			//服务类型8位	一般为0(前3bit忽略，最后bit必须置为0，中间4位分别是最小时延，最大吞吐量，最高可靠性，最小费用，其中只能有1bit置1 其他必须为0)
	short				sTotalLenOfPack;		//数据包长度	整个IP报文长度,网络字节序
	short				sPackId;				//数据包标识
	short				sFragFlags;				//分片使用		大于以太网传输帧则予以分片
												//该字段和Flags和Fragment Offest字段联合使用
												/*
												Flags:3位
												第一位:不使用
												第二位DF:0允许分片，1禁止分片
												第三位MF:如果对数据进行了分片，那么最后一片的MF=1
												片偏移Fragment Offset:13位，该分片在整包中的位置
												*/
	//以下字段与udp和tcp的伪头部共用
	char				cTTL;					//存活时间		使用8位，填充128(32的倍数)
	char				cTypeOfProtocol;		//协议类型 0x01=ICMP 0x02IGMP 0x06=TCP UDP=0x11
	short				sCheckSum;				//校验和         
	unsigned int		uiSourIp;				//源ip
	unsigned int		uiDestIp;				//目的ip

}IP_HEADER, * PIP_HEADER;

//UDP头   8字节
typedef struct _UDP_HEADER
{
	unsigned short		usSourcePort;
	unsigned short		usDestPort;
	unsigned short		usLen;	//UDP头+数据的长度,网络字节序
	short				sCheckSum;

} UDP_HEADER, * PUDP_HEADER;

//UDP伪头部 
typedef struct _FALS_UDP_HEADER
{
	unsigned int		uiSourceIP;
	unsigned int		uiDestIP;
	char				mbz;//填0
	char				cProtoolType;//协议类型
	unsigned short		usLen; //TCP/UDP数据包的长度(即从TCP/UDP报头算起到数据包结束的长度 单位:字节),网络字节序

} FALS_UDP_HEADER, * PFALS_UDP_HEADER;

//TCP头
//TCP头部，总长度20字节   
typedef struct _TCP_HEADER
{
	short				sSourPort;                  // 源端口号16bit
	short				sDestPort;                  // 目的端口号16bit
	unsigned int		uiSequNum;					// 序列号32bit
	unsigned int		uiAcknowledgeNum;			// 确认号32bit
	char				cHeaderLenAndFlag;          // 前4位：TCP头长度
	char				cFlag;                      // 后6位为标志位
	short				sWindowSize;                // 窗口大小16bit
	short				sCheckSum;                  // 检验和16bit,TCP头+TCP数据
	short				sSurgentPointer;            // 紧急数据偏移量16bit

}TCP_HEADER, * PTCP_HEADER;

//TCP伪头部
//using FALS_TCP_HEADER = FALS_UDP_HEADER;
//using PFALS_TCP_HEADER = PFALS_UDP_HEADER;
typedef FALS_UDP_HEADER FALS_TCP_HEADER;
typedef PFALS_UDP_HEADER PFALS_TCP_HEADER;

//ICMP头部
typedef struct _ICMP_HEADER
{
	char			cType;	//类型 ping请求使用的是8 回复使用的是0  其次常用的就是3目的不可达
	char			cCode;	//代码
	short			sCheckSum;

	unsigned short  usID;
	unsigned short  usSequence;
	char			cData[32];

}ICMP_HEADER, * PICMP_HEADER;

//TCP头中的选项  只存在6种选项
typedef struct _TCP_OPTIONS
{
	char			ckind;
	char			cLength;
	union {
		char            m_cContext[36];
		unsigned short  MSS; //maximum segment
		unsigned short  WSS; //windows scale
		char            sackP[36];//SACK permitted
		char            sack[36];//SACK
		struct __UTIME {              //时间戳
			unsigned int Time1;
			unsigned int Time2;
		}U_TIME;
	};
}TCP_OPTIONS, * PTCP_OPTIONS;

//ICMP头部，总长度4字节   
typedef struct _icmp_hdr
{
	unsigned    char	cIcmp_type;   //类型
	unsigned    char	cCode;        //代码
	unsigned    short	usChecksum;    //16位检验和   
}icmp_hdr;

//以太网帧包
typedef struct __ETH_PACK
{

	char			DstAddr[MAC_ADDR_LEN];

	union {
		char			SrcAddr[MAC_ADDR_LEN];
		unsigned int	dstMac;
	};

	unsigned short	EthType;
	IP_HEADER		ip_h;
	union {
		UDP_HEADER  udp_h;
		TCP_HEADER  tcp_h;
		ICMP_HEADER icmp_h;
	};

} ETH_PACK, * PETH_PACK;

//UDP包
typedef struct __ETH_UDP_PACK
{
	ETH_HEADER		eth_h;
	IP_HEADER		ip_h;
	UDP_HEADER		udp_h;
}ETH_UDP_PACK, * PETH_UDP_PACK;


//TCP包
typedef struct __ETH_TCP_PACK
{
	ETH_HEADER		eth_h;
	IP_HEADER		ip_h;
	TCP_HEADER		tcp_h;
}ETH_TCP_PACK, * PETH_TCP_PACK;

//ICMP包
typedef struct __ETH_ICMP_PACK
{
	ETH_HEADER		eth_h;
	IP_HEADER		ip_h;
	ICMP_HEADER		icmp_h;
}ETH_ICMP_PACK, * PETH_ICMP_PACK;

//ICMP包
typedef struct _ICMP_DATA
{
	IP_HEADER		ip_h;
	ICMP_HEADER		icmp_h;
}ICMP_DATA, * PICMP_DATA;

//ARP
typedef struct _ARP
{
	short			sHardWare;	//硬件类型 1表示以太网地址
	short			sProType;	//协议类型	0x0800表示ip地址 0x8035表示RARP
	char			cHD_len;	//硬件地址长度 6 mac长度为6
	char			cPT_len;	//协议地址长度 4 ip长度为4
	short			sOP;		//操作:ARP请求(1),ARP应答(2),RARP请求(3),RARP应答(4)
	char			SrcAddr[MAC_ADDR_LEN];//发送端mac地址
	unsigned int	SrcIP;		//发送端ip地址

	char			DstAddr[MAC_ADDR_LEN];//目标主机mac地址
	unsigned int	DstIP;		//目标主机的ip地址
};
//ARP
typedef struct  _ARP_PACK
{
	ETH_HEADER		eth_h;//dstaddr=全为1的bit即0xffffffffffff usEthType填写0x0806表示是arp请求或回答
							//(ff:ff:ff:ff:ff:ff是以太网的广播地址，以太网接口都会处置之)
	_ARP			arp;
	//char			srev[18];//以太网帧最小是60字节，在此补充18字节长度，但无实质意义。大多数的设备驱动程序或接口卡自动地用填充字符把以太网数据帧充满到最小长度
};

#pragma pack(pop)
//命令码定义
