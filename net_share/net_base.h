#pragma once
#include "base_def.h"
#pragma pack(push,1)
//IP4Э�飬������Ҫ����IP6Э��
//����������Ϣ
typedef struct __BASE_NET_INFO
{
	unsigned long				ulMaxFrameSize;   //��������֡���ֵ��IP��ҪС�ڵ��ڴ�ֵ,Ĭ�϶���1500
	unsigned char               szCurrentAddress[MAC_ADDR_LEN];
	unsigned char				szDeviceDescr[MAC_DESCSTR_LEN];

}BASE_NET_INFO, * PBASE_NET_INFO;

//������Ϣ
typedef struct _NET_IP_FILTER
{
	char				cFlag;	//0==ip  1==mac
	unsigned int		uiNetSeccion; //IP��
	unsigned int		uiNetMasks;   //��������

	unsigned int		iCount;
	unsigned int		uiMacSeccion[1];//MAC��ַ,���Դ�����mac��ַ����

}NET_IP_FILTER, * PNET_IP_FILTER;
//��̫��ͷ
typedef struct __ETH_HEADER
{
	unsigned char       szDstAddr[MAC_ADDR_LEN];
	unsigned char       szSrcAddr[MAC_ADDR_LEN];
	unsigned short      usEthType;

} ETH_HEADER, * PETH_HEADER;

//IPͷ
typedef struct _IP_HEADER
{
	char				cVersionAndHeaderLen;   //�汾��Ϣ(ǰ4λ)��ͷ����(��4λ,��ʾռ��32bit��λ�������Գ��ȵ���ֵ*4��tcpͷ��󳤶�Ϊ60) ����ip4���'E'����,
	char				cTypeOfService;			//��������8λ	һ��Ϊ0(ǰ3bit���ԣ����bit������Ϊ0���м�4λ�ֱ�����Сʱ�ӣ��������������߿ɿ��ԣ���С���ã�����ֻ����1bit��1 ��������Ϊ0)
	short				sTotalLenOfPack;		//���ݰ�����	����IP���ĳ���,�����ֽ���
	short				sPackId;				//���ݰ���ʶ
	short				sFragFlags;				//��Ƭʹ��		������̫������֡�����Է�Ƭ
												//���ֶκ�Flags��Fragment Offest�ֶ�����ʹ��
												/*
												Flags:3λ
												��һλ:��ʹ��
												�ڶ�λDF:0�����Ƭ��1��ֹ��Ƭ
												����λMF:��������ݽ����˷�Ƭ����ô���һƬ��MF=1
												Ƭƫ��Fragment Offset:13λ���÷�Ƭ�������е�λ��
												*/
	//�����ֶ���udp��tcp��αͷ������
	char				cTTL;					//���ʱ��		ʹ��8λ�����128(32�ı���)
	char				cTypeOfProtocol;		//Э������ 0x01=ICMP 0x02IGMP 0x06=TCP UDP=0x11
	short				sCheckSum;				//У���         
	unsigned int		uiSourIp;				//Դip
	unsigned int		uiDestIp;				//Ŀ��ip

}IP_HEADER, * PIP_HEADER;

//UDPͷ   8�ֽ�
typedef struct _UDP_HEADER
{
	unsigned short		usSourcePort;
	unsigned short		usDestPort;
	unsigned short		usLen;	//UDPͷ+���ݵĳ���,�����ֽ���
	short				sCheckSum;

} UDP_HEADER, * PUDP_HEADER;

//UDPαͷ�� 
typedef struct _FALS_UDP_HEADER
{
	unsigned int		uiSourceIP;
	unsigned int		uiDestIP;
	char				mbz;//��0
	char				cProtoolType;//Э������
	unsigned short		usLen; //TCP/UDP���ݰ��ĳ���(����TCP/UDP��ͷ�������ݰ������ĳ��� ��λ:�ֽ�),�����ֽ���

} FALS_UDP_HEADER, * PFALS_UDP_HEADER;

//TCPͷ
//TCPͷ�����ܳ���20�ֽ�   
typedef struct _TCP_HEADER
{
	short				sSourPort;                  // Դ�˿ں�16bit
	short				sDestPort;                  // Ŀ�Ķ˿ں�16bit
	unsigned int		uiSequNum;					// ���к�32bit
	unsigned int		uiAcknowledgeNum;			// ȷ�Ϻ�32bit
	char				cHeaderLenAndFlag;          // ǰ4λ��TCPͷ����
	char				cFlag;                      // ��6λΪ��־λ
	short				sWindowSize;                // ���ڴ�С16bit
	short				sCheckSum;                  // �����16bit,TCPͷ+TCP����
	short				sSurgentPointer;            // ��������ƫ����16bit

}TCP_HEADER, * PTCP_HEADER;

//TCPαͷ��
//using FALS_TCP_HEADER = FALS_UDP_HEADER;
//using PFALS_TCP_HEADER = PFALS_UDP_HEADER;
typedef FALS_UDP_HEADER FALS_TCP_HEADER;
typedef PFALS_UDP_HEADER PFALS_TCP_HEADER;

//ICMPͷ��
typedef struct _ICMP_HEADER
{
	char			cType;	//���� ping����ʹ�õ���8 �ظ�ʹ�õ���0  ��γ��õľ���3Ŀ�Ĳ��ɴ�
	char			cCode;	//����
	short			sCheckSum;

	unsigned short  usID;
	unsigned short  usSequence;
	char			cData[32];

}ICMP_HEADER, * PICMP_HEADER;

//TCPͷ�е�ѡ��  ֻ����6��ѡ��
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
		struct __UTIME {              //ʱ���
			unsigned int Time1;
			unsigned int Time2;
		}U_TIME;
	};
}TCP_OPTIONS, * PTCP_OPTIONS;

//ICMPͷ�����ܳ���4�ֽ�   
typedef struct _icmp_hdr
{
	unsigned    char	cIcmp_type;   //����
	unsigned    char	cCode;        //����
	unsigned    short	usChecksum;    //16λ�����   
}icmp_hdr;

//��̫��֡��
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

//UDP��
typedef struct __ETH_UDP_PACK
{
	ETH_HEADER		eth_h;
	IP_HEADER		ip_h;
	UDP_HEADER		udp_h;
}ETH_UDP_PACK, * PETH_UDP_PACK;


//TCP��
typedef struct __ETH_TCP_PACK
{
	ETH_HEADER		eth_h;
	IP_HEADER		ip_h;
	TCP_HEADER		tcp_h;
}ETH_TCP_PACK, * PETH_TCP_PACK;

//ICMP��
typedef struct __ETH_ICMP_PACK
{
	ETH_HEADER		eth_h;
	IP_HEADER		ip_h;
	ICMP_HEADER		icmp_h;
}ETH_ICMP_PACK, * PETH_ICMP_PACK;

//ICMP��
typedef struct _ICMP_DATA
{
	IP_HEADER		ip_h;
	ICMP_HEADER		icmp_h;
}ICMP_DATA, * PICMP_DATA;

//ARP
typedef struct _ARP
{
	short			sHardWare;	//Ӳ������ 1��ʾ��̫����ַ
	short			sProType;	//Э������	0x0800��ʾip��ַ 0x8035��ʾRARP
	char			cHD_len;	//Ӳ����ַ���� 6 mac����Ϊ6
	char			cPT_len;	//Э���ַ���� 4 ip����Ϊ4
	short			sOP;		//����:ARP����(1),ARPӦ��(2),RARP����(3),RARPӦ��(4)
	char			SrcAddr[MAC_ADDR_LEN];//���Ͷ�mac��ַ
	unsigned int	SrcIP;		//���Ͷ�ip��ַ

	char			DstAddr[MAC_ADDR_LEN];//Ŀ������mac��ַ
	unsigned int	DstIP;		//Ŀ��������ip��ַ
};
//ARP
typedef struct  _ARP_PACK
{
	ETH_HEADER		eth_h;//dstaddr=ȫΪ1��bit��0xffffffffffff usEthType��д0x0806��ʾ��arp�����ش�
							//(ff:ff:ff:ff:ff:ff����̫���Ĺ㲥��ַ����̫���ӿڶ��ᴦ��֮)
	_ARP			arp;
	//char			srev[18];//��̫��֡��С��60�ֽڣ��ڴ˲���18�ֽڳ��ȣ�����ʵ�����塣��������豸���������ӿڿ��Զ���������ַ�����̫������֡��������С����
};

#pragma pack(pop)
//�����붨��
