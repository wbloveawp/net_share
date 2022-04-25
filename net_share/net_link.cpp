
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
	//1.��̫��֡
	memcpy(_pack->DstAddr, pg->SrcAddr, MAC_ADDR_LEN);
	memcpy(_pack->SrcAddr, pg->DstAddr, MAC_ADDR_LEN);
	_pack->EthType = IP_FLAG;
	//2.ipͷ
	_m_ip_h.cVersionAndHeaderLen = IP_VER_LEN;
	_m_ip_h.cTypeOfService = 0;
	//_ip_h->sTotalLenOfPack = xx ���ʱ��д;
	_m_ip_h.sPackId = 0;//�����ʱ�����ֵ����
	_m_ip_h.sFragFlags = 0;//��Ƭ
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

	//udpͷ��ַ
	_p_udp_h = &_pack->udp_h;

	//udp���ݵ�ַ
	_udp_data = ((char*)&_pack->udp_h) + c_udp_head_len;// sizeof(UDP_HEADER);

	//udpαͷ��ַ
	_p_fudp_h = (PFALS_UDP_HEADER)((char*)(&_pack->udp_h) - c_udp_fhead_len);  // sizeof(FALS_UDP_HEADER));

	//��ʼ��ucpͷ
	_m_udp_h.usSourcePort = pif->ip_info.dst_port;// pg->udp_h.usDestPort;// pBni->m_DestPort;
	_m_udp_h.usDestPort = pif->ip_info.src_port;// pg->udp_h.usSourcePort;// pBni->m_SrcPort;
	//_m_udp_h = *_p_udp_h;
	//_p_udp_h->usLen ���ʱ��д
	//_p_udp_h->sCheckSum = ���ʱ����;
	//��ʼ��udpαͷ��
	_f_udp_h.uiSourceIP = _m_ip_h.uiSourIp;//_ip_h->uiDestIp;
	_f_udp_h.uiDestIP = _m_ip_h.uiDestIp;// _ip_h->uiSourIp;
	_f_udp_h.cProtoolType = PRO_UDP;
	_f_udp_h.mbz = 0;
	memcpy(&_nbi, pif, sizeof(ustrt_net_base_info));
	//_f_udp_h.usLen �ڴ����ʱ����д
};

bool  wb_udp_link::OnRead(wb_filter_interface* p_fi, void* const lp_link, const ETH_PACK* pk, int len)
{
	_last_op_time = time(0);
	return p_fi->post_send(this, lp_link, get_udp_pack_date(pk), get_udp_pack_date_len(pk));//�Ƿ�Ƭ
}

bool wb_udp_link::OnRecv(wb_filter_interface* p_fi, void* const lp_link, const char* buf, int len, LPOVERLAPPED pol)
{
	//���д���ݴ�����д д��ɺ� ��recv
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
			//̫��İ�ֱ�Ӳ�����
			//��ʼ��Ƭ
			int MF_idx = 0;//Ƭ��
			int offset = 0;	//����ƫ��
			int l_len = len; //ʣ������
			int data_len = 0;
			do
			{
				//��Ƭ�µ�һ��������ʵ�ʳ�����1472  ����Ӧ���� l_len>1480
				
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
	USHORT udp_len = len + c_udp_head_len;//udpͷ+���ݳ���
	//Ƭ�׺���udpͷ����д�ĳ���Ϊ���������ȣ�����������udpͷ��Ϣ
	//ʹ�û�����������У���
	int all_len = len + c_udp_head_len + c_udp_fhead_len;
	char tmp_buf[UDP_RECV_SIZE + c_udp_head_len + c_udp_fhead_len];
	//memset(tmp_buf, 0, len + c_udp_head_len + c_udp_fhead_len + 1);
	PFALS_UDP_HEADER p_fh_tmp = (PFALS_UDP_HEADER)tmp_buf;
	PUDP_HEADER p_udp_tmp = (PUDP_HEADER)(tmp_buf+ c_udp_fhead_len);
	memcpy(p_fh_tmp, &_f_udp_h, c_udp_fhead_len);
	memcpy(p_udp_tmp, &_m_udp_h, c_udp_head_len);
	memcpy((char*)p_udp_tmp+ c_udp_head_len, buffer, len);//2.��������
	p_fh_tmp->usLen = p_udp_tmp->usLen = htons(udp_len);//�����ֽ���
	//p_udp_tmp->sCheckSum = 0x0000;	//4.����udp��У���
	//1.��װudpͷ
	memcpy(_p_udp_h, &_m_udp_h, c_udp_head_len);//1.����udpͷ
	memcpy(_udp_data, buffer, c_udp_data_len_max);//2.��������
	_p_udp_h->usLen = htons(udp_len);//�����ֽ���
	_p_udp_h->sCheckSum = ip_check_sum((USHORT*)p_fh_tmp, all_len);
	//_f_udp_h.usLen = 
	//memcpy(_p_fudp_h, &_f_udp_h, c_udp_fhead_len);	//3.����αͷ��
	//_p_udp_h->sCheckSum = 0x0000;	//4.����udp��У���
	//_p_udp_h->sCheckSum = p_udp_tmp->sCheckSum; // ip_check_sum((USHORT*)_p_fudp_h, c_udp_data_len_max + c_udp_fhead_len);
	//5.����������ipͷ
	//_m_ip_h.sPackId++;//����udp�����������
	memcpy(_ip_h, &_m_ip_h, c_ip_head_len);

	USHORT usFlags = 0x2000;
	_ip_h->sFragFlags = htons(usFlags);
	_ip_h->sTotalLenOfPack = htons(c_ip_head_len + c_udp_head_len + c_udp_data_len_max);
	//6.����ip��У���
	_ip_h->sPackId = ntohs(_pack_id);
	_ip_h->sCheckSum = ip_check_sum((USHORT*)_ip_h, c_ip_head_len);
	//WLOG("udp checksum=%d ,ip check_sum=%d\n", (int)_p_udp_h->sCheckSum, (int)_ip_h->sCheckSum);
	//7.���°�����
	pack_len = c_ip_head_size + c_udp_head_len + c_udp_data_len_max;
	//assert(_pack->udp_h.usDestPort == 0xa383);
	return _pack;
}

const ETH_PACK* wb_udp_link::make_pack(const char* buffer, int len, int& pack_len) {
	//1.��װudpͷ
	USHORT udp_len = c_udp_head_len + len;//udpͷ+���ݳ���
	memcpy(_p_udp_h, &_m_udp_h, c_udp_head_len);
	_f_udp_h.usLen = _p_udp_h->usLen = htons(udp_len);//�����ֽ���
	memcpy(_udp_data, buffer, len);//2.��������
	memcpy(_p_fudp_h, &_f_udp_h, c_udp_fhead_len);	//3.����αͷ��
	//_p_udp_h->sCheckSum = 0x0000;
	_p_udp_h->sCheckSum = ip_check_sum((USHORT*)_p_fudp_h, udp_len + c_udp_fhead_len);	//4.����udp��У���
	//5.����������ipͷ
	memcpy(_ip_h, &_m_ip_h, c_ip_head_len);
	_ip_h->sPackId = htons(_pack_id++);
	_ip_h->sTotalLenOfPack = htons(c_ip_head_len + udp_len);
	//6.����ip��У���
	_ip_h->sCheckSum = ip_check_sum((USHORT*)_ip_h, c_ip_head_len);
	//7.���°�����
	pack_len = c_ip_head_size + udp_len;
	return _pack;
}

const ETH_PACK* wb_udp_link::make_pack_mid(USHORT MF_idx, const char* buffer, int len, bool is_end, int& pack_len)
{
	//û����udpͷ
	memcpy(_ip_h, &_m_ip_h, c_ip_head_len);
	_ip_h->sPackId = htons(_pack_id);
	memcpy(_p_udp_h, buffer, len);
	_ip_h->sTotalLenOfPack = htons(c_ip_head_len + len);//ipͷ����+���ݳ��ȼ���
	USHORT usFlags = is_end ? 0x000 : 0x2000;//��3λ
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
		WLOG("udp create socket ʧ��:%d\n",GetLastError());
		return false;
	}
	//���õײ��м仺��������Ϊ0������ֱ�Ӵ�Э���ջ�ﴫ��ʹ�������������
	int nZero = 0;
	setsockopt(_socket, SOL_SOCKET, SO_SNDBUF, (char*)nZero, sizeof(nZero));
	setsockopt(_socket, SOL_SOCKET, SO_RCVBUF, (char*)nZero, sizeof(nZero));
	sockaddr_in add = { 0 };
	add.sin_family = AF_INET;
	add.sin_port = ::htons(0);
	add.sin_addr.S_un.S_addr = INADDR_ANY;
	if (SOCKET_ERROR == ::bind(_socket, (sockaddr*)&add, sizeof(sockaddr_in)))
	{
		WLOG("udp socket bind ʧ��:%d\n", GetLastError());
		closesocket(_socket);
		return false;
	}
	return true;
}

//ICMPЭ��
//TCP
//wb_lock wb_tcp_link::g_lock;
//wb_tcp_link::tcp_link_map   wb_tcp_link::g_map;

wb_tcp_link::wb_tcp_link(const ETH_PACK* p_pack) :wb_net_link(p_pack) {
	//Ŀ���Դ�˿ڵ������������ݻ�д����ʱ����ֱ�Ӹ�������
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
	_winscal = _ack = ntohl(p_pack->tcp_h.uiSequNum)+1;//��ʾ���´η��͵�ȷ�����
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
	//WLOG("tcp[%s   %d]���ض˿�:%d linke ����%p\n", _ip.c_str(), ntohs(p_pack->tcp_h.sDestPort), ntohs(p_pack->tcp_h.sSourPort), this);
#endif // _DEBUG
}
const ETH_PACK* wb_tcp_link::make_pack_syn(int& pack_len)
{
	//1.tcpͷ,4λ�ײ�����
	short tcp_len = c_tcp_head_len + _tcp_syn_data_len;
	char tl = tcp_len/4;
	memcpy(_p_tcp_h, &_tcp_h, c_tcp_head_len);
	_p_tcp_h->cFlag = TCP_SYN2; //�ڶ������ְ���ʶ
	_p_tcp_h->cHeaderLenAndFlag = (tl << 4) & 0xF0;
	_p_tcp_h->uiSequNum = htonl(_seq);//�����ҵ��¸����ֽ����
	_p_tcp_h->uiAcknowledgeNum = htonl(_ack);
	memcpy(_p_tcp_data, _tcp_syn_data, _tcp_syn_data_len);
	//2.����αͷ
	memcpy(_p_f_tcp_h, &_f_tcp_h, c_tcp_fhead_len);
	_p_f_tcp_h->usLen = htons(tcp_len);//tcp����
	_p_tcp_h->sCheckSum = ip_check_sum((USHORT*)_p_f_tcp_h, tcp_len+ c_tcp_fhead_len);//����У���
	//3.ipͷ
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
	//1.tcpͷ,4λ�ײ�����
	char tl = c_tcp_head_len / 4;
	memcpy(_p_tcp_h, &_tcp_h, c_tcp_head_len);
	_p_tcp_h->cFlag = TCP_ACK; //�ڶ������ְ���ʶ
	_p_tcp_h->cHeaderLenAndFlag = (tl << 4) & 0xF0;
	_p_tcp_h->uiSequNum = htonl(_seq);//�����ҵ��¸����ֽ����
	_p_tcp_h->uiAcknowledgeNum = htonl(ack);
	//2.����αͷ
	memcpy(_p_f_tcp_h, &_f_tcp_h, c_tcp_fhead_len);
	_p_f_tcp_h->usLen = htons(c_tcp_head_len);//tcp����
	_p_tcp_h->sCheckSum = ip_check_sum((USHORT*)_p_f_tcp_h, c_tcp_head_len + c_tcp_fhead_len);//����У���
	//3.ipͷ
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
	//1.tcpͷ,4λ�ײ�����
	_next_seq += len;
	char tl = c_tcp_head_len / 4;
	memcpy(_p_tcp_h, &_tcp_h, c_tcp_head_len);
	_p_tcp_h->cFlag = flag; //
	_p_tcp_h->cHeaderLenAndFlag = (tl << 4) & 0xF0;
	_p_tcp_h->uiSequNum = htonl(_seq);//�����ҵ��¸����ֽ����
	_p_tcp_h->uiAcknowledgeNum = htonl(_ack);
	memcpy(_p_tcp_data, buf, len);
	//2.����αͷ
	memcpy(_p_f_tcp_h, &_f_tcp_h, c_tcp_fhead_len);
	_p_f_tcp_h->usLen = htons(c_tcp_head_len+len);//tcp����
	_p_tcp_h->sCheckSum = ip_check_sum((USHORT*)_p_f_tcp_h, c_tcp_head_len + c_tcp_fhead_len+len);//����У���
	//3.ipͷ
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
	//1.tcpͷ,4λ�ײ�����
	_next_seq += 1;
	char tl = c_tcp_head_len / 4;
	memcpy(_p_tcp_h, &_tcp_h, c_tcp_head_len);
	_p_tcp_h->cFlag = flag; //
	_p_tcp_h->cHeaderLenAndFlag = (tl << 4) & 0xF0;
	_p_tcp_h->uiSequNum = htonl(_seq);//�����ҵ��¸����ֽ����
	_p_tcp_h->uiAcknowledgeNum = htonl(_ack);
	//2.����αͷ
	memcpy(_p_f_tcp_h, &_f_tcp_h, c_tcp_fhead_len);
	_p_f_tcp_h->usLen = htons(c_tcp_head_len);//tcp����
	_p_tcp_h->sCheckSum = ip_check_sum((USHORT*)_p_f_tcp_h, c_tcp_head_len + c_tcp_fhead_len);//����У���
	//3.ipͷ
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
	//1.����ǵ�ǰ���ύ����������
#ifdef _DEBUG
	//wb_auto_tm tm("DoOnData");
	//WLOG("tcp %p:����%d=%s   len=%d   end=%d\n",this, c_seq- _s_ack, bPsh?"����":"��ͨ", date_len, c_seq - _s_ack+ date_len);
#endif // _DEBUG
	//AUTO_LOCK(_p_lock);
	if (c_seq < _ack)return true;
	if (c_seq > _ack) //����������
	{
		if (_p_cache.find(c_seq) == _p_cache.end()) //��ֹ�ظ�������ͬ�İ������²����ڴ��й©
		{
			auto pk = p_fi->get_data_pack_ex(); //�������ύ���ݣ����л���
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
						//WLOG("1tcp %pȷ�ϰ�:%d\n", this, _winscal);
					}
				}

				if (bPsh)_psh_num++;//��ǻ���������Ҫ�����ύ������,���ڵȴ���������װ��󣬾ͷ���
			}
		}
		//return true;//ֱ�ӷ��أ��ظ��İ�ֱ�ӷ���
	}
	if (c_seq == _ack)//��ǰ��
	{
		if (!bPsh) 
		{
			if (_p_cache.find(c_seq) == _p_cache.end()) //��ֹ�ظ�������ͬ�İ������²����ڴ��й©
			{
				auto pk = p_fi->get_data_pack_ex(); //�������ύ���ݣ����л���
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
						//WLOG("2tcp %pȷ�ϰ�:%d\n",this, _winscal);
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
		//��Ҫ�����������
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
				this->_p_cache = que;	//���»�����
				this->_ack += send_len;//�����ֽ����
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
	//C�˶�S�����ݽ���ȷ�ϻ�ά��
	auto tcp_head_len = (pg->tcp_h.cHeaderLenAndFlag >> 4 & 0x0F) * 4;
	int date_len = ntohs(pg->ip_h.sTotalLenOfPack) - c_ip_head_len - tcp_head_len;//���ݳ���
	auto c_ack = ntohl(pg->tcp_h.uiAcknowledgeNum);	//c��ȷ�����
	auto c_seq = ntohl(pg->tcp_h.uiSequNum);		//c�˵�ǰ��seq
	if (date_len <= 0 || c_seq < _ack)	//û�����ݻ������ط�ǰ�������
	{
		return true;// _ack == c_seq + date_len;//һ����������ﷵ�ض���tcpЭ�����ط����� 
	}
	return DoOnData(p_fi, lp_link,(const char*)&pg->tcp_h + tcp_head_len, date_len, c_seq, false);
}

bool wb_tcp_link::OnRead(wb_filter_interface* p_fi, void* const lp_link, const ETH_PACK* pg, int len)
{
	if (_close_status != 0)return true;
	auto c_ack = ntohl(pg->tcp_h.uiAcknowledgeNum);	//c��ȷ�����
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
		int date_len = ntohs(pg->ip_h.sTotalLenOfPack) - c_ip_head_len - tcp_head_len;//���ݳ���
		UINT c_seq = ntohl(pg->tcp_h.uiSequNum);		//c�˵�ǰ��seq
		ret = DoOnData(p_fi, lp_link, (const char*)&pg->tcp_h + tcp_head_len, date_len, c_seq, true);
	}break; //�����ݣ�����Ҫ�����ύ��Ӧ�ò�
	case TCP_ACK: ret = DoOnAck(p_fi, lp_link, pg, len); break;	//ȷ�ϰ���Ҳ���ܻ�Я������,������Ƶ���ģ�case������ǰ�����Ч��
	//case TCP_FIN:ret = DoOnFin(p_fi, pg, len); break;//�Ͽ����ְ�
	//case TCP_RST_ACK:ret = false; break;//
	//case TCP_SYN1:assert(0); break;//��һ�����ְ�����������
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
		//���ڿ���
		w_len = len > DEFAULT_RECV_SIZE ? DEFAULT_RECV_SIZE : len;
		if (_next_win_max< w_len)
			w_len = _next_win_max;
		auto mk = make_pack_data(buf + dlt, w_len , out_l, len < DEFAULT_RECV_SIZE ? TCP_DATA_ACK : TCP_ACK);
		p_fi->post_write(this, mk, out_l);
		len -= w_len;
		dlt += w_len;
		_next_win_max -= w_len;

		if (_next_win_max <= 0 && len>0)//�Է����ڲ������ˣ��һ��д���������ݣ�����뻺�棬�Ҳ���Ͷ��recv,ֱ���Է�ȷ������
		{
			assert(_data_len + len <= sizeof(_data_buf));
			memcpy(_data_buf+ _data_len, buf + dlt,  len);
			_data_len += len;
			//if(inet_addr("221.228.195.121")== _m_ip_h.uiSourIp)
				//WLOG("tcp<%p>:_next_win_max=%d nwm=%d len=%d ʣ��:%d  [seq=%ud ack=%ud]\n",this, _next_win_max, s_nwm, s_len, pl->wbuf.len,_seq,_ack);
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
		printf("connect[%s   %d] ʧ�� err=%d \n", _ip.c_str() , ntohs(_tcp_h.sSourPort) , GetLastError());
		return false;
	}
	// ����select��ʱֵ
	TIMEVAL	to;
	fd_set	fsW;

	FD_ZERO(&fsW);
	FD_SET(_socket, &fsW);
	to.tv_sec = 3;
	to.tv_usec = 0;

	int ret = select(0, NULL, &fsW, NULL, &to);
	if (ret > 0)
	{
		// �Ļ�����ģʽ:
		arg = 0;
		ioctlsocket(_socket, FIONBIO, &arg);
		return true;
	}
	else
	{
		//printf("connect  select [%s   %d] ʧ�� err=%d \n", _ip.c_str(), ntohs(_tcp_h.sSourPort), GetLastError());
		closesocket(_socket);
		return false;
	}
}
