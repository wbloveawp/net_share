
#include "net_link.h"
#include <algorithm>

wb_net_link::wb_net_link(const ETH_PACK* pg){
	_nbi = *((ustrt_net_base_info*)&pg->ip_h.uiSourIp);
	//_info.ip_info = _nbi;
	memcpy(_info.mac, pg->SrcAddr, MAC_ADDR_LEN);
	//assert(_nbi.ip_info.dst_port==0xa283);
	//WLOG("udp:%p         [%x  %x] \n", this, _nbi.ip_info.dst_port, _nbi.ip_info.src_port);
	_pack = (ETH_PACK*)_write_buf;
	_ip_h = &_pack->ip_h;
	_pack_id = 0;// pg->ip_h.sPackId;
	//1.以太网帧
	memcpy(_pack->DstAddr, pg->SrcAddr, MAC_ADDR_LEN);
	memcpy(_pack->SrcAddr, pg->DstAddr, MAC_ADDR_LEN);
	_pack->EthType = IP_FLAG;
	//2.ip头
	_m_ip_h.cVersionAndHeaderLen = IP_VER_LEN;
	_m_ip_h.cTypeOfService = 0;
	//_ip_h->sTotalLenOfPack = xx 打包时填写;
	_m_ip_h.sPackId = 0;//打包的时候这个值自增
	_m_ip_h.sFragFlags = 0;//分片
	_m_ip_h.cTTL = DEFAULT_TTL;
	_m_ip_h.cTypeOfProtocol = pg->ip_h.cTypeOfProtocol;
	_m_ip_h.uiDestIp = pg->ip_h.uiSourIp;// nbi.ip_info.src_ip;// pBni->m_SrcIP;
	_m_ip_h.uiSourIp = pg->ip_h.uiDestIp;// nbi.ip_info.dst_ip;// pBni->m_DestIP
	_last_op_time = time(0);
 };


wb_udp_link::wb_udp_link(const ETH_PACK* pg,const ustrt_net_base_info* pif) :wb_net_link(pg) {
	//_last_op_time = time(0);
	_addr.sin_family = AF_INET;
	_addr.sin_addr.S_un.S_addr = pif->ip_info.dst_ip;//  nbi.ip_info.dst_ip;
	_addr.sin_port = pif->ip_info.dst_port;// pg->udp_h.usDestPort;  //nbi.ip_info.dst_port;

	//udp头地址
	_p_udp_h = &_pack->udp_h;

	//udp数据地址
	_udp_data = ((char*)&_pack->udp_h) + c_udp_head_len;// sizeof(UDP_HEADER);

	//udp伪头地址
	_p_fudp_h = (PFALS_UDP_HEADER)((char*)(&_pack->udp_h) - c_udp_fhead_len);  // sizeof(FALS_UDP_HEADER));

	//初始化ucp头
	_m_udp_h.usSourcePort = pif->ip_info.dst_port;// pg->udp_h.usDestPort;// pBni->m_DestPort;
	_m_udp_h.usDestPort = pif->ip_info.src_port;// pg->udp_h.usSourcePort;// pBni->m_SrcPort;
	//_m_udp_h = *_p_udp_h;
	//_p_udp_h->usLen 打包时填写
	//_p_udp_h->sCheckSum = 打包时计算;
	//初始化udp伪头部
	_f_udp_h.uiSourceIP = _m_ip_h.uiSourIp;//_ip_h->uiDestIp;
	_f_udp_h.uiDestIP = _m_ip_h.uiDestIp;// _ip_h->uiSourIp;
	_f_udp_h.cProtoolType = PRO_UDP;
	_f_udp_h.mbz = 0;
	memcpy(&_nbi, pif, sizeof(ustrt_net_base_info));
	//_f_udp_h.usLen 在打包的时候填写
};

bool  wb_udp_link::OnRead(wb_filter_interface* p_fi, void* const lp_link, const ETH_PACK* pk, int len)
{
	_last_op_time = time(0);
	return p_fi->post_send(this, lp_link, get_udp_pack_date(pk), get_udp_pack_date_len(pk));//非分片
}

bool wb_udp_link::OnRecv(wb_filter_interface* p_fi, void* const lp_link, const char* buf, int len, LPOVERLAPPED pol)
{
	//打包写数据大包多次写 写完成后 再recv
	_recv_bytes += len;
	_last_op_time = time(0);
	if (len <= c_udp_data_len_max)
	{
		int out_l = 0;
		auto mk = make_pack(buf, len, out_l);
		p_fi->post_write(this,mk, out_l);
	}
	else
	{
		if (ERROR_MORE_DATA != GetLastError())
		{
			//太大的包直接不处理
			//开始切片
			int MF_idx = 0;//片数
			int offset = 0;	//数据偏移
			int l_len = len; //剩余数据
			int data_len = 0;
			do
			{
				//分片下第一个包数据实际长度是1472  后续应该是 l_len>1480
				
				if (MF_idx == 0) {
					int out_l = 0;
					data_len = c_udp_data_len_max;
					auto mk = make_pack_1st(buf, len, out_l);
					p_fi->post_write(this,mk, out_l);//post_write

					++MF_idx;
				}
				else
				{
					int out_l = 0;
					data_len = l_len <= c_ip_data_len_max ? l_len : c_ip_data_len_max;
					auto mk = make_pack_mid(MF_idx++, buf + offset, data_len, l_len <= c_ip_data_len_max,out_l);
					p_fi->post_write(this, mk, out_l);//
				}
				offset += data_len;
				l_len -= data_len;
			} while (l_len > 0);
		}
		else
		{
			assert(0);
		}
	}
	return true;
	//return p_fi->post_recv(this, lp_link,pol);
}

const ETH_PACK* wb_udp_link::make_pack_1st(const char* buffer, int len, int& pack_len)
{
	USHORT udp_len = len + c_udp_head_len;//udp头+数据长度
	//片首含有udp头，填写的长度为整个包长度，后续不再有udp头信息
	//使用缓冲区来计算校验和
	int all_len = len + c_udp_head_len + c_udp_fhead_len;
	char tmp_buf[UDP_RECV_SIZE + c_udp_head_len + c_udp_fhead_len];
	//memset(tmp_buf, 0, len + c_udp_head_len + c_udp_fhead_len + 1);
	PFALS_UDP_HEADER p_fh_tmp = (PFALS_UDP_HEADER)tmp_buf;
	PUDP_HEADER p_udp_tmp = (PUDP_HEADER)(tmp_buf+ c_udp_fhead_len);
	memcpy(p_fh_tmp, &_f_udp_h, c_udp_fhead_len);
	memcpy(p_udp_tmp, &_m_udp_h, c_udp_head_len);
	memcpy((char*)p_udp_tmp+ c_udp_head_len, buffer, len);//2.拷贝数据
	p_fh_tmp->usLen = p_udp_tmp->usLen = htons(udp_len);//网络字节序
	//p_udp_tmp->sCheckSum = 0x0000;	//4.计算udp的校验和
	//1.组装udp头
	memcpy(_p_udp_h, &_m_udp_h, c_udp_head_len);//1.拷贝udp头
	memcpy(_udp_data, buffer, c_udp_data_len_max);//2.拷贝数据
	_p_udp_h->usLen = htons(udp_len);//网络字节序
	_p_udp_h->sCheckSum = ip_check_sum((USHORT*)p_fh_tmp, all_len);
	//_f_udp_h.usLen = 
	//memcpy(_p_fudp_h, &_f_udp_h, c_udp_fhead_len);	//3.拷贝伪头部
	//_p_udp_h->sCheckSum = 0x0000;	//4.计算udp的校验和
	//_p_udp_h->sCheckSum = p_udp_tmp->sCheckSum; // ip_check_sum((USHORT*)_p_fudp_h, c_udp_data_len_max + c_udp_fhead_len);
	//5.拷贝和生成ip头
	//_m_ip_h.sPackId++;//对于udp包序号无意义
	memcpy(_ip_h, &_m_ip_h, c_ip_head_len);

	USHORT usFlags = 0x2000;
	_ip_h->sFragFlags = htons(usFlags);
	_ip_h->sTotalLenOfPack = htons(c_ip_head_len + c_udp_head_len + c_udp_data_len_max);
	//6.计算ip的校验和
	_ip_h->sPackId = ntohs(_pack_id);
	_ip_h->sCheckSum = ip_check_sum((USHORT*)_ip_h, c_ip_head_len);
	//WLOG("udp checksum=%d ,ip check_sum=%d\n", (int)_p_udp_h->sCheckSum, (int)_ip_h->sCheckSum);
	//7.更新包长度
	pack_len = c_ip_head_size + c_udp_head_len + c_udp_data_len_max;
	//assert(_pack->udp_h.usDestPort == 0xa383);
	return _pack;
}

const ETH_PACK* wb_udp_link::make_pack(const char* buffer, int len, int& pack_len) {
	//1.组装udp头
	USHORT udp_len = c_udp_head_len + len;//udp头+数据长度
	memcpy(_p_udp_h, &_m_udp_h, c_udp_head_len);
	_f_udp_h.usLen = _p_udp_h->usLen = htons(udp_len);//网络字节序
	memcpy(_udp_data, buffer, len);//2.拷贝数据
	memcpy(_p_fudp_h, &_f_udp_h, c_udp_fhead_len);	//3.拷贝伪头部
	//_p_udp_h->sCheckSum = 0x0000;
	_p_udp_h->sCheckSum = ip_check_sum((USHORT*)_p_fudp_h, udp_len + c_udp_fhead_len);	//4.计算udp的校验和
	//5.拷贝和生成ip头
	memcpy(_ip_h, &_m_ip_h, c_ip_head_len);
	_ip_h->sPackId = htons(_pack_id++);
	_ip_h->sTotalLenOfPack = htons(c_ip_head_len + udp_len);
	//6.计算ip的校验和
	_ip_h->sCheckSum = ip_check_sum((USHORT*)_ip_h, c_ip_head_len);
	//7.更新包长度
	pack_len = c_ip_head_size + udp_len;
	return _pack;
}

const ETH_PACK* wb_udp_link::make_pack_mid(USHORT MF_idx, const char* buffer, int len, bool is_end, int& pack_len)
{
	//没有了udp头
	memcpy(_ip_h, &_m_ip_h, c_ip_head_len);
	_ip_h->sPackId = htons(_pack_id);
	memcpy(_p_udp_h, buffer, len);
	_ip_h->sTotalLenOfPack = htons(c_ip_head_len + len);//ip头长度+数据长度即可
	USHORT usFlags = is_end ? 0x000 : 0x2000;//高3位
	USHORT FO = MF_idx * (1480 / 8);
	usFlags |= (FO & 0x1FFF);
	_ip_h->sFragFlags = htons(usFlags);
	_ip_h->sCheckSum = ip_check_sum((USHORT*)_ip_h, c_ip_head_len);
	pack_len = c_ip_head_size + len;
	if (is_end)_pack_id++;
	return _pack;
}


bool wb_udp_link::create_socket() {

	_socket = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (_socket == INVALID_SOCKET)
	{
		WLOG("udp create socket 失败:%d\n",GetLastError());
		return false;
	}
	//设置底层中间缓冲区设置为0，数据直接从协议堆栈里传入和传出，提升性能
	int nZero = 0;
	setsockopt(_socket, SOL_SOCKET, SO_SNDBUF, (char*)nZero, sizeof(nZero));
	setsockopt(_socket, SOL_SOCKET, SO_RCVBUF, (char*)nZero, sizeof(nZero));
	sockaddr_in add = { 0 };
	add.sin_family = AF_INET;
	add.sin_port = ::htons(0);
	add.sin_addr.S_un.S_addr = INADDR_ANY;
	if (SOCKET_ERROR == ::bind(_socket, (sockaddr*)&add, sizeof(sockaddr_in)))
	{
		WLOG("udp socket bind 失败:%d\n", GetLastError());
		closesocket(_socket);
		return false;
	}
	return true;
}

//ICMP协议
//TCP
//wb_lock wb_tcp_link::g_lock;
//wb_tcp_link::tcp_link_map   wb_tcp_link::g_map;

wb_tcp_link::wb_tcp_link(const ETH_PACK* p_pack) :wb_net_link(p_pack) {
	//目标和源端口调换，后续数据回写网卡时可以直接复制数据
	//wb_tcp_link::g_lock.lock();
	//assert(wb_tcp_link::g_map.find((ULONG64)this)== wb_tcp_link::g_map.end());
	//wb_tcp_link::g_map[(ULONG64)this] = this;
	//wb_tcp_link::g_lock.unlock();
	_p_tcp_h = &_pack->tcp_h;
	_p_f_tcp_h =(PFALS_TCP_HEADER)((char*)_p_tcp_h - sizeof(FALS_TCP_HEADER));
	_tcp_h.sDestPort = p_pack->tcp_h.sSourPort;
	_tcp_h.sSourPort = p_pack->tcp_h.sDestPort;
	_tcp_h.sSurgentPointer = 0x0000;
	_tcp_h.sWindowSize = htons(TCP_RECV_SIZE);// p_pack->tcp_h.sWindowSize;//
	_winscal = _ack = ntohl(p_pack->tcp_h.uiSequNum)+1;//表示我下次发送的确认序号
	//_s_ack = _winscal;
	//auto ttp = ((p_pack->tcp_h.cHeaderLenAndFlag >> 4) & 0x0F)*4;
	_tcp_syn_data_len = ((p_pack->tcp_h.cHeaderLenAndFlag >> 4) & 0x0F)*4 - c_tcp_head_len;
	memcpy(_tcp_syn_data, (char*)&p_pack->tcp_h+ c_tcp_head_len , _tcp_syn_data_len);

	_f_tcp_h.uiDestIP = _m_ip_h.uiDestIp;
	_f_tcp_h.uiSourceIP = _m_ip_h.uiSourIp;
	_f_tcp_h.cProtoolType = PRO_TCP;

	_m_ip_h.sFragFlags = p_pack->ip_h.sFragFlags;

	_p_tcp_data = (char*)_p_tcp_h + c_tcp_head_len;
	//auto ws = ntohs(p_pack->tcp_h.sWindowSize);
	_next_win_max = TCP_RECV_SIZE;// ntohs(p_pack->tcp_h.sWindowSize);
	_last_ack = _seq;
#ifdef _DEBUG
	sockaddr_in addr = {};
	addr.sin_addr.S_un.S_addr = p_pack->ip_h.uiDestIp;
	_ip = inet_ntoa(addr.sin_addr);
	//WLOG("tcp[%s   %d]本地端口:%d linke 构造%p\n", _ip.c_str(), ntohs(p_pack->tcp_h.sDestPort), ntohs(p_pack->tcp_h.sSourPort), this);
#endif // _DEBUG
}
const ETH_PACK* wb_tcp_link::make_pack_syn(int& pack_len)
{
	//1.tcp头,4位首部长度
	short tcp_len = c_tcp_head_len + _tcp_syn_data_len;
	char tl = tcp_len/4;
	memcpy(_p_tcp_h, &_tcp_h, c_tcp_head_len);
	_p_tcp_h->cFlag = TCP_SYN2; //第二次握手包标识
	_p_tcp_h->cHeaderLenAndFlag = (tl << 4) & 0xF0;
	_p_tcp_h->uiSequNum = htonl(_seq);//更新我的下个包字节序号
	_p_tcp_h->uiAcknowledgeNum = htonl(_ack);
	memcpy(_p_tcp_data, _tcp_syn_data, _tcp_syn_data_len);
	//2.生成伪头
	memcpy(_p_f_tcp_h, &_f_tcp_h, c_tcp_fhead_len);
	_p_f_tcp_h->usLen = htons(tcp_len);//tcp长度
	_p_tcp_h->sCheckSum = ip_check_sum((USHORT*)_p_f_tcp_h, tcp_len+ c_tcp_fhead_len);//计算校验和
	//3.ip头
	memcpy(_ip_h, &_m_ip_h, c_ip_head_len);
	_ip_h->sPackId = htons(_pack_id++);

	_ip_h->sTotalLenOfPack = htons(c_ip_head_len+ tcp_len);
	_ip_h->sCheckSum = ip_check_sum((USHORT*)_ip_h, c_ip_head_len);
	pack_len = tcp_len + c_ip_head_size;
	if (pack_len < ETH_MIN_LEN)
		pack_len = ETH_MIN_LEN;
	_seq = _next_seq;
	return  _pack;
}

const ETH_PACK* wb_tcp_link::make_pack_ack(int& pack_len, UINT ack)
{
	//1.tcp头,4位首部长度
	char tl = c_tcp_head_len / 4;
	memcpy(_p_tcp_h, &_tcp_h, c_tcp_head_len);
	_p_tcp_h->cFlag = TCP_ACK; //第二次握手包标识
	_p_tcp_h->cHeaderLenAndFlag = (tl << 4) & 0xF0;
	_p_tcp_h->uiSequNum = htonl(_seq);//更新我的下个包字节序号
	_p_tcp_h->uiAcknowledgeNum = htonl(ack);
	//2.生成伪头
	memcpy(_p_f_tcp_h, &_f_tcp_h, c_tcp_fhead_len);
	_p_f_tcp_h->usLen = htons(c_tcp_head_len);//tcp长度
	_p_tcp_h->sCheckSum = ip_check_sum((USHORT*)_p_f_tcp_h, c_tcp_head_len + c_tcp_fhead_len);//计算校验和
	//3.ip头
	memcpy(_ip_h, &_m_ip_h, c_ip_head_len);
	_ip_h->sPackId = htons(_pack_id++);
	_ip_h->sTotalLenOfPack = htons(c_ip_head_len + c_tcp_head_len);
	_ip_h->sCheckSum = ip_check_sum((USHORT*)_ip_h, c_ip_head_len);
	pack_len = c_tcp_head_len + c_ip_head_size;
	if (pack_len < ETH_MIN_LEN)
		pack_len = ETH_MIN_LEN;
	return  _pack;
}

const ETH_PACK* wb_tcp_link::make_pack_data(const char* buf, int len, int& pack_len, char flag)
{
	//WLOG("mack_data:%d   %x\n", len, flag);
	//1.tcp头,4位首部长度
	_next_seq += len;
	char tl = c_tcp_head_len / 4;
	memcpy(_p_tcp_h, &_tcp_h, c_tcp_head_len);
	_p_tcp_h->cFlag = flag; //
	_p_tcp_h->cHeaderLenAndFlag = (tl << 4) & 0xF0;
	_p_tcp_h->uiSequNum = htonl(_seq);//更新我的下个包字节序号
	_p_tcp_h->uiAcknowledgeNum = htonl(_ack);
	memcpy(_p_tcp_data, buf, len);
	//2.生成伪头
	memcpy(_p_f_tcp_h, &_f_tcp_h, c_tcp_fhead_len);
	_p_f_tcp_h->usLen = htons(c_tcp_head_len+len);//tcp长度
	_p_tcp_h->sCheckSum = ip_check_sum((USHORT*)_p_f_tcp_h, c_tcp_head_len + c_tcp_fhead_len+len);//计算校验和
	//3.ip头
	memcpy(_ip_h, &_m_ip_h, c_ip_head_len);
	_ip_h->sPackId = htons(_pack_id++);
	_ip_h->sTotalLenOfPack = htons(c_ip_head_len + c_tcp_head_len+ len);
	_ip_h->sCheckSum = ip_check_sum((USHORT*)_ip_h, c_ip_head_len);
	pack_len = c_tcp_head_len + c_ip_head_size + len;
	if (pack_len < ETH_MIN_LEN)
		pack_len = ETH_MIN_LEN;
	_seq = _next_seq;
	return  _pack;
}

const ETH_PACK* wb_tcp_link::make_pack_fin(int& pack_len, char flag)
{
	//1.tcp头,4位首部长度
	_next_seq += 1;
	char tl = c_tcp_head_len / 4;
	memcpy(_p_tcp_h, &_tcp_h, c_tcp_head_len);
	_p_tcp_h->cFlag = flag; //
	_p_tcp_h->cHeaderLenAndFlag = (tl << 4) & 0xF0;
	_p_tcp_h->uiSequNum = htonl(_seq);//更新我的下个包字节序号
	_p_tcp_h->uiAcknowledgeNum = htonl(_ack);
	//2.生成伪头
	memcpy(_p_f_tcp_h, &_f_tcp_h, c_tcp_fhead_len);
	_p_f_tcp_h->usLen = htons(c_tcp_head_len);//tcp长度
	_p_tcp_h->sCheckSum = ip_check_sum((USHORT*)_p_f_tcp_h, c_tcp_head_len + c_tcp_fhead_len);//计算校验和
	//3.ip头
	memcpy(_ip_h, &_m_ip_h, c_ip_head_len);
	_ip_h->sPackId = htons(_pack_id++);
	_ip_h->sTotalLenOfPack = htons(c_ip_head_len + c_tcp_head_len);
	_ip_h->sCheckSum = ip_check_sum((USHORT*)_ip_h, c_ip_head_len);
	pack_len = c_tcp_head_len + c_ip_head_size;
	if (pack_len < ETH_MIN_LEN)
		pack_len = ETH_MIN_LEN;
	_seq = _next_seq;
	return  _pack;
}

bool wb_tcp_link::DoOnData(wb_filter_interface* p_fi, void* const lp_link, const char* pdata, int date_len,UINT c_seq, bool bPsh)
{
	assert(date_len > 0);
	//1.如果是当前的提交包，立马发送
#ifdef _DEBUG
	//wb_auto_tm tm("DoOnData");
	//WLOG("tcp %p:数据%d=%s   len=%d   end=%d\n",this, c_seq- _s_ack, bPsh?"紧急":"普通", date_len, c_seq - _s_ack+ date_len);
#endif // _DEBUG
	//AUTO_LOCK(_p_lock);
	if (c_seq < _ack)return true;
	if (c_seq > _ack) //后续包缓存
	{
		if (_p_cache.find(c_seq) == _p_cache.end()) //防止重复缓存相同的包，导致部分内存会泄漏
		{
			auto pk = p_fi->get_data_pack_ex(); //非立马提交数据，进行缓存
			if (pk)
			{
				pk->len = date_len;
				pk->bPsh = bPsh;
				memcpy(pk->data, pdata, date_len);
				_p_cache[c_seq] = pk;
				{
					//_winscal += date_len;
					auto ad = _p_cache.find(_winscal);
					if (ad != _p_cache.end() && _winscal+ ad->second->len == c_seq)
					{
						_winscal = c_seq;
						int out_l = 0;
						auto mk = make_pack_ack(out_l, _winscal);
						p_fi->post_write(this, mk, out_l);
						//WLOG("1tcp %p确认包:%d\n", this, _winscal);
					}
				}

				if (bPsh)_psh_num++;//标记缓存中有需要立马提交的数据,正在等待完整包组装完后，就发送
			}
		}
		//return true;//直接返回，重复的包直接返回
	}
	if (c_seq == _ack)//当前包
	{
		if (!bPsh) 
		{
			if (_p_cache.find(c_seq) == _p_cache.end()) //防止重复缓存相同的包，导致部分内存会泄漏
			{
				auto pk = p_fi->get_data_pack_ex(); //非立马提交数据，进行缓存
				if (pk)
				{
					pk->len = date_len;
					pk->bPsh = bPsh;
					memcpy(pk->data, pdata, date_len);
					_p_cache[c_seq] = pk;
					auto ad = _p_cache.find(_winscal + date_len);
					if (ad != _p_cache.end())
					{
						_winscal += (date_len + ad->second->len);
						int out_l = 0;
						auto mk = make_pack_ack(out_l, _winscal);
						p_fi->post_write(this, mk, out_l);
						//WLOG("2tcp %p确认包:%d\n",this, _winscal);
					}
				}
			}
		}
		else
		{
			if (p_fi->post_send(this, lp_link, pdata, date_len))
			{
				_ack += date_len;
				int out_l = 0;
				auto mk = make_pack_ack(out_l, _ack);
				p_fi->post_write(this, mk, out_l);
			}
			else
			{
				return true;
			}
		}
	}

	if (_psh_num) 
	{
		//需要数据完整检查
		using data_vec = std::vector<wb_data_pack_ex*>;

		auto que = _p_cache;
		auto fd = que.find(_ack);
		if (fd == que.end()) {
			return true;
		}
		auto pk = fd->second;

		//char send_buf[TCP_RECV_SIZE];
		int send_len = 0;// pk->len;
		data_vec del_data;

		while (pk && _psh_num > 0)
		{
			auto fn = [&]() {

				char send_buf[TCP_RECV_SIZE];
				char* sd_buf = send_buf;

				if (send_len > TCP_RECV_SIZE)
				{
					sd_buf = new char[send_len];
					WLOG("tcp %p new OnSend_no_copy buf=%p\n", this, sd_buf);
				}

				int dlt_len = 0;
				for (auto i : del_data)
				{
					memcpy(sd_buf + dlt_len, i->data, i->len);
					dlt_len += i->len;
					p_fi->back_data_pack_ex(i);
				}
				if (send_len > TCP_RECV_SIZE)p_fi->post_send_no_copy(this, lp_link, sd_buf, send_len);
				else p_fi->post_send(this, lp_link, sd_buf, send_len);
				this->_p_cache = que;	//更新缓冲区
				this->_ack += send_len;//更新字节序号
				send_len = 0;
				int out_l = 0;
				auto mk = make_pack_ack(out_l, _ack);
				p_fi->post_write(this, mk, out_l);
				del_data.clear();
			};

			del_data.push_back(pk);
			que.erase(_ack + send_len);
			/*
			if (send_len + pk->len > TCP_RECV_SIZE)
			{
				fn();
			}
			*/
			//memcpy(send_buf+ send_len, pk->data, pk->len);
			send_len += pk->len;
			if (pk->bPsh)
			{
				_psh_num--;
				fn();
			}
			fd = que.find(_ack + send_len);
			if (fd != que.end())
				pk = fd->second;
			else
				break;
		}
	}
	return true;
}
bool wb_tcp_link::DoOnAck(wb_filter_interface* p_fi, void* const lp_link, const ETH_PACK* pg, int len)
{
	//C端对S端数据进行确认或维活
	auto tcp_head_len = (pg->tcp_h.cHeaderLenAndFlag >> 4 & 0x0F) * 4;
	int date_len = ntohs(pg->ip_h.sTotalLenOfPack) - c_ip_head_len - tcp_head_len;//数据长度
	auto c_ack = ntohl(pg->tcp_h.uiAcknowledgeNum);	//c端确认序号
	auto c_seq = ntohl(pg->tcp_h.uiSequNum);		//c端当前的seq
	if (date_len <= 0 || c_seq < _ack)	//没有数据或者是重发前面的数据
	{
		return true;// _ack == c_seq + date_len;//一般情况下这里返回都是tcp协议层的重发机制 
	}
	return DoOnData(p_fi, lp_link,(const char*)&pg->tcp_h + tcp_head_len, date_len, c_seq, false);
}

bool wb_tcp_link::OnRead(wb_filter_interface* p_fi, void* const lp_link, const ETH_PACK* pg, int len)
{
	if (_close_status != 0)return true;
	auto c_ack = ntohl(pg->tcp_h.uiAcknowledgeNum);	//c端确认序号
	AUTO_LOCK(_pack_lock);
	if (c_ack > _last_ack)
	{
		_next_win_max = ntohs(pg->tcp_h.sWindowSize);
		if (_next_win_max < 4096)
			_next_win_max = 4096;
		_last_ack = c_ack;
	}
	_last_op_time = time(0);
	auto ret = true;

	if (_data_len>0)
	{
		if (_data_len > _next_win_max) {
			char buf[TCP_RECV_SIZE * 2];
			memcpy(buf, _data_buf, _next_win_max);
			_data_len -= _next_win_max;
			memmove(_data_buf, _data_buf + _next_win_max, _data_len);
			OnRecv(p_fi, lp_link, buf, _next_win_max, _pol);
		}
		else
		{
			OnRecv(p_fi, lp_link, _data_buf, _data_len, _pol);
			_data_len = 0;
		}
		if (_data_len <= TCP_RECV_SIZE && _pol)
		{
			p_fi->post_recv(this, lp_link, _pol);
			_pol = nullptr;
		}
	}
	switch (TCP_FLAG(pg->tcp_h.cFlag))
	{
	case TCP_DATA_ACK:
	{
		auto tcp_head_len = (pg->tcp_h.cHeaderLenAndFlag >> 4 & 0x0F) * 4;
		int date_len = ntohs(pg->ip_h.sTotalLenOfPack) - c_ip_head_len - tcp_head_len;//数据长度
		UINT c_seq = ntohl(pg->tcp_h.uiSequNum);		//c端当前的seq
		ret = DoOnData(p_fi, lp_link, (const char*)&pg->tcp_h + tcp_head_len, date_len, c_seq, true);
	}break; //有数据，且需要立马提交给应用层
	case TCP_ACK: ret = DoOnAck(p_fi, lp_link, pg, len); break;	//确认包，也可能会携带数据,出现最频繁的，case放在最前面提高效率
	//case TCP_FIN:ret = DoOnFin(p_fi, pg, len); break;//断开握手包
	//case TCP_RST_ACK:ret = false; break;//
	//case TCP_SYN1:assert(0); break;//第一次握手包不会走这里
	}
	return ret;
}

bool wb_tcp_link::OnRecv(wb_filter_interface* p_fi, void* const lp_link, const char* buf, int len, LPOVERLAPPED pol)
{
	_recv_bytes += len;
	AUTO_LOCK(_pack_lock);
	_last_op_time = time(0);
	int dlt = 0, w_len = 0;
	int out_l = 0; 
	do
	{
		//窗口控制
		w_len = len > DEFAULT_RECV_SIZE ? DEFAULT_RECV_SIZE : len;
		if (_next_win_max< w_len)
			w_len = _next_win_max;
		auto mk = make_pack_data(buf + dlt, w_len , out_l, len < DEFAULT_RECV_SIZE ? TCP_DATA_ACK : TCP_ACK);
		p_fi->post_write(this, mk, out_l);
		len -= w_len;
		dlt += w_len;
		_next_win_max -= w_len;

		if (_next_win_max <= 0 && len>0)//对方窗口不够用了，且还有待传输的数据，则存入缓存，且不再投递recv,直到对方确认数据
		{
			assert(_data_len + len <= sizeof(_data_buf));
			memcpy(_data_buf+ _data_len, buf + dlt,  len);
			_data_len += len;
			//if(inet_addr("221.228.195.121")== _m_ip_h.uiSourIp)
				//WLOG("tcp<%p>:_next_win_max=%d nwm=%d len=%d 剩余:%d  [seq=%ud ack=%ud]\n",this, _next_win_max, s_nwm, s_len, pl->wbuf.len,_seq,_ack);
			//assert(len > 0);
			if (_data_len > TCP_RECV_SIZE) {
				_pol = pol;
				return false;
			}
			return true;
		}

	} while (len>0);
	return true;
}
bool wb_tcp_link::connect()
{
	sockaddr_in add = { 0 };
	add.sin_family = AF_INET;
	add.sin_port = _tcp_h.sSourPort;
	add.sin_addr.S_un.S_addr = _m_ip_h.uiSourIp;

	ULONG arg = 1;
	if (NO_ERROR != ::ioctlsocket(_socket, FIONBIO, &arg))
	{
		closesocket(_socket);
		return false;
	}
	if (SOCKET_ERROR != ::connect(_socket, (sockaddr*)&add, sizeof(add)))
	{
		printf("connect[%s   %d] 失败 err=%d \n", _ip.c_str() , ntohs(_tcp_h.sSourPort) , GetLastError());
		return false;
	}
	// 设置select超时值
	TIMEVAL	to;
	fd_set	fsW;

	FD_ZERO(&fsW);
	FD_SET(_socket, &fsW);
	to.tv_sec = 3;
	to.tv_usec = 0;

	int ret = select(0, NULL, &fsW, NULL, &to);
	if (ret > 0)
	{
		// 改回阻塞模式:
		arg = 0;
		ioctlsocket(_socket, FIONBIO, &arg);
		return true;
	}
	else
	{
		//printf("connect  select [%s   %d] 失败 err=%d \n", _ip.c_str(), ntohs(_tcp_h.sSourPort), GetLastError());
		closesocket(_socket);
		return false;
	}
}
