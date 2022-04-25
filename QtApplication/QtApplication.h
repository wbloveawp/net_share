#pragma once


#if _MSC_VER >= 1600
#pragma execution_character_set("utf-8")
#endif
#include "../net_share/mainer.h"
#include <QtWidgets/QMainWindow>
#include<QtWidgets/qpushbutton.h>
#include <QtWidgets/qlabel.h>
#include "ui_QtApplication.h"
#include <qtimer.h>


constexpr double ONE_KB = 1024.0;
constexpr double HUND_KB = 1024.0 * 100.0;
constexpr double ONE_MB = 1024.0 * 1024.0;



class wb_machine_event_ui : public wb_machine_event
{

    struct mac_info
    {
        INT64 read_packs = 0;
        INT64 write_packs = 0;
              
        INT64 send_bytes = 0;
        INT64 recv_bytes = 0;
    };

    INT                 _i_ip;
    QString             _szIP;
    char                _mac[32] = {};

    QTableWidgetItem*   _item0;
    QTableWidgetItem*   _item1;
    QTableWidgetItem*   _item2;
    QTableWidgetItem*   _item3;
    QTableWidgetItem*   _item4;
    QTableWidgetItem*   _item5;
    QTableWidgetItem*   _item6;
    std::atomic_int     _links=0;

    std::atomic<INT64> _read_packs = 0;
    std::atomic<INT64> _write_packs = 0;

    std::atomic<INT64> _send_bytes = 0;
    std::atomic<INT64> _recv_bytes = 0;


    mac_info _last_info = {};
public:
    wb_machine_event_ui(QTableWidget* tw , INT ip,const unsigned char mac[MAC_ADDR_LEN]):_i_ip(ip){

        in_addr ia = {};
        ia.S_un.S_addr = _i_ip;
        _szIP = inet_ntoa(ia);

        for (int i=0;i< MAC_ADDR_LEN;i++)
        {
            sprintf(_mac + i * 3, "%x ", mac[i]);
        }
        _item0 = new QTableWidgetItem(_szIP);
        tw->insertRow(0);
        tw->setItem(0, 0, _item0);
        tw->setItem(0, 1, _item1 = new QTableWidgetItem(_mac));
        tw->setItem(0, 2, _item2 = new QTableWidgetItem("0"));//连接数
        tw->setItem(0, 3, _item3 = new QTableWidgetItem("0"));//读包
        tw->setItem(0, 4, _item4 = new QTableWidgetItem("0"));//写包
        tw->setItem(0, 5, _item5 = new QTableWidgetItem("0"));//发
        tw->setItem(0, 6, _item6 = new QTableWidgetItem("0"));//收
    }                          

    QString get_ip() { return _szIP; };
    INT get_ip_int() { return _i_ip; };
    const char* get_mac() { return _mac; };

    inline void OnNewLink(wb_link_interface* lk) {
        lk->set_event(this);
        _links++;
    }
    inline void OCloseLink(const wb_link_interface* lk) {
        _links--;
    }

    virtual void OnRead(const wb_link_interface* lk, int len) {
        _read_packs++;
    }
    virtual void OnWrite(const wb_link_interface* lk, int len) {
        _write_packs++;
    }

    virtual void OnSend(const wb_link_interface* lk, int len) {
        _send_bytes += len;
    }
    virtual void OnRecv(const wb_link_interface* lkc, int len) {
        _recv_bytes += len;
    }

    void update(float seconds) {
        _item2->setData(0, _links.load());

        char msg[128] = { 0 };
        sprintf(msg, "总:%I64d 速:%.1f包/S", _read_packs.load(), (double)(_read_packs.load() - _last_info.read_packs)/ seconds);
        _item3->setData(0, msg);

        sprintf(msg, "总:%I64d 速:%.1f包/S", _write_packs.load(), (double)(_write_packs.load() - _last_info.write_packs) / seconds);
        _item4->setData(0, msg);

        if(_send_bytes - _last_info.send_bytes>=HUND_KB)
            sprintf(msg, "%.2fMB :%.2fMB/S", (double)_send_bytes.load() / ONE_MB, (double)(_send_bytes.load() - _last_info.send_bytes) / seconds / ONE_MB);
        else
            sprintf(msg, "%.2fMB :%.2fKB/S", (double)_send_bytes.load() / ONE_MB, (double)(_send_bytes.load() - _last_info.send_bytes) / seconds / ONE_KB);
        _item5->setData(0, msg);

        if (_recv_bytes - _last_info.recv_bytes >= HUND_KB)
            sprintf(msg, "%.2fMB :%.2fMB/S", (double)_recv_bytes.load() / ONE_MB, (double)(_recv_bytes.load() - _last_info.recv_bytes) / seconds / ONE_MB);
        else
            sprintf(msg, "%.2fMB :%.2fKB/S", (double)_recv_bytes.load() / ONE_MB, (double)(_recv_bytes.load() - _last_info.recv_bytes) / seconds / ONE_KB);
        _item6->setData(0, msg);

        _last_info = { _read_packs,_write_packs,_send_bytes,_recv_bytes };
    }
};

class QtApplication : public QMainWindow ,public wb_machine_interface
{
    using wb_mac_map = std::map<INT, wb_machine_event_ui*>;

    Q_OBJECT

public:
    QtApplication(QWidget *parent = Q_NULLPTR);
    virtual ~QtApplication()
    {
        _macs.clear();
        if (m)delete m;

    }
    void Init();
private:
    Ui::QtApplicationClass ui;
    QTimer* tmer;
    QMenuBar* menuBar;

    wb_lock     _ui_lock;
protected:
    mainer * m ;
    bool _bstart = false;
    wb_lock _lock;
    wb_mac_map _macs;
public:
    virtual void AddLink(wb_link_interface* lk) {
        AUTO_LOCK(_lock);
        auto& info = lk->get_link_info();
        auto iter = _macs.find(info.ip_info.ip_info.src_ip);
        if (iter != _macs.end())
        {
            iter->second->OnNewLink(lk);
        }
        else
        {
            auto mac = new wb_machine_event_ui(ui.tableWidget,info.ip_info.ip_info.src_ip, info.mac);
            if (mac)
            {
                mac->OnNewLink(lk);
                _macs[mac->get_ip_int()] = mac;
            }
        }
    };
    virtual void DelLink(const wb_link_interface* lk) {
       auto  me = (wb_machine_event_ui * )lk->get_event();
       me->OCloseLink(lk);
    };
};
