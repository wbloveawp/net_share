#pragma once
//����㲥��
#define UDP_BROADCAST_IP		0xFF000000
//�����鲥��
#define MULTICAST_IP_MIN		0xE0000000
//��ʱ����
#ifdef _DEBUG
#define TIME_OUT_UDP			5		//udp��ʱ��λ��,�������ʱ�䣬�ͷ�����
#define TIME_OUT_TCP			30
#define TIME_OUT_ICMP			10		//��λs
#else
#define TIME_OUT_UDP			60		//udp��ʱ��λ��,�������ʱ�䣬�ͷ�����
#define TIME_OUT_TCP			120
#define TIME_OUT_ICMP			10		//��λs
#endif

//#define VER_2016_10_21		

#define UDP_DEBUG				0
#define ICMP_DEBUG				0
#define TCP_DEBUG				0

#define ETH_MIN_LEN				60

#define MAC_ADDR_LEN			6		
#define MAC_DESCSTR_LEN			128
#define IEEE8023_MIN_MAC_SIZE	66		//IEEE8023 ��С֡�ĳ���
#define DEFAULT_RECV_SIZE		1460	//Ĭ�������ݵ�BUF��Ϊ����֡��С��ȥIPͷ+TCPͷ(��UDPͷ)
#define DEFAULT_MAC_LEN			1500	//Ĭ�����������֡Ϊ1500�ֽ�,��������̫��֡ͷ����β����IPͷ+����
#define MAX_MAC_LEN				1514	//һ����������̫��֡��󳤶�1500+֡ͷ14�ֽ�+4β��CRC
#define DEFAULT_BUFF_SIZE		2048	//�ڴ��Ĭ�ϵĻ�������С
#define ICMP_BUF_SIZE			512

//recv ��������С����
#define TCP_RECV_SIZE			8192
#define UDP_RECV_SIZE			(1024*10)
#define ICMP_RECV_SIZE			DEFAULT_RECV_SIZE
#define TCP_PACK_SIZE			1440
 
//IP�����ͼ�Э��
#define	IP_FLAG					0x0008	//IP��
#define	PRO_ICMP				0x01	//ICMPЭ��
#define	PRO_UDP					0x11	//UDPЭ��
#define	PRO_TCP					0x06	//TCPЭ��

//TCP������
#define TCP_SYN1				0x02	//TCP���ְ�,��һ�����ְ�
#define TCP_SYN2				0x12	//TCP����ȷ�ϰ�,�ڶ������ְ�
#define TCP_RST					0x04	//TCP����ȷ�ϰ�,�ڶ������ְ�
#define TCP_RST_ACK				0x14	//�����,��FIN������
#define TCP_DATA_ACK			0x18	//TCP PSH���ݰ�
#define	TCP_ACK					0x10	//TCP ȷ�ϰ�,���ܺ�������
#define TCP_FIN					0x11	//TCP�Ͽ�ȷ�ϰ�
//TCP��ͷĬ�ϳ���	
#define TCP_HEAD_LEN			20		
#define IP_VER_LEN				0x45

//�����Ͷ���
#define	PACK_NO_NEED			0		//����Ҫ�İ�
#define	PACK_NO_IP				1		//��IP��
#define	PACK_NO_UDP_TCP_ICMP	2		//ΪIP��������UDP��TCP��ICMP��
#define	PACK_IN_LAN				3		//ΪUDP��TCP��ICMP���ҷ�ssl����Ŀ���ַΪ������
#define	PACK_IS_NEED			4		//��Ҫ���˵İ���ΪUDP,TCP��ICMP������Ŀ���ַ�Ǳ����Σ�Դ��ַ�ڱ�������
#define PACK_IS_SSL				5		//�˰��˿�Ϊ443����ΪsslЭ���


#define PACK_UDP_DATA		0x00
#define PACK_ICMP_DATA		0x01
#define PACK_TCP_SYN1		TCP_SYN1
#define PACK_TCP_SYN2		TCP_SYN2
#define PACK_TCP_RST_ACK	TCP_RST_ACK	
#define PACK_TCP_ACK		TCP_ACK
#define PACK_TCP_FIN		TCP_FIN
#define PACK_TCP_DATA_ACK	TCP_DATA_ACK //���ݰ�+ȷ��
#define PACK_TCP_DATA_ONLY	TCP_DATA_ONLY//���������ݰ�


//���ӳ�ʱ����
#define NETLINK_COUNT_MAX	2048
#define TIMER_INTERVAL		10000  //��ʱ�����10000ms,��10��
#define NETLINK_TIME_OUT	20000 //��λ���룬�������ӳ�ʱʱ��	

//Ĭ�� MSS��С
#define DEFAULT_MMS			0x0218
//MMS���ֵΪ���֡1500��ȥ֡ͷ��IPͷ=1460
#define MAX_MMS				0x05B4
//Ĭ�ϴ��ڴ�С
#define DEFAULT_WINDOW_SIZE	(short)0xFFFF	
//Ĭ��ICMP��ʱ 1s
#define ICMP_DEFAULT_TIMEOUT	1000 
//Ĭ��TTL
#define DEFAULT_TTL			(char)0x75//(char)0x80
//Ĭ��TCPͷ������
#define TCP_HEADER_LEN		20

#define RECV_SIZE			8192



#define OUT_IOCTL_NDISPROT_OPEN_DEVICE				0x12c800	//���豸
#define OUT_IOCTL_NDISPROT_QUERY_OID_VALUE			0x12c804	//��ѯOID
#define OUT_IOCTL_NDISPROT_SET_OID_VALUE			0x12c814	//����OID
#define OUT_IOCTL_NDISPROT_QUERY_BINDING			0x12c80c	//��ѯ��
#define OUT_IOCTL_NDISPROT_BIND_WAIT				0x12c810	//�ȴ���
#define OUT_IOCTL_NDISPROT_INDICATE_STATUS			0x12c818	//
#define OUT_IOCTL_NDISPROT_GET_DEVICE_LIST			0x12c81c	//��ȡ�����豸�б�
#define OUT_IOCTL_NDISPROT_SET_HOST_SECCION			0x12c820	//���ù���
#define OUT_IOCTL_NDISPROT_GET_NETWORK_BASE_INFO	0x12c824	//��ȡ����������Ϣ
#define OUT_IOCTL_NDISPROT_GET_NETWORK_TRANS_INFO	0x12c828	//��ȡ����������Ϣ
#define OUT_IOCTL_NDISPROT_CLOSE_DEVICE				0x12c82c	//�ر��豸


#define NDIS_DRIVER_NAME	"\\\\.\\wbndis"