#include "QtApplication.h"
#include <qstringlistmodel.h>
#include <QTableView>
#include <QtGui/qstandarditemmodel.h>


QtApplication::QtApplication(QWidget *parent)
    : QMainWindow(parent)
{
    m = new mainer(this);
    ui.setupUi(this);
    tmer = new QTimer(this);
    tmer->start(1000);
    ui.label_mac_info->setText("");
    ui.label_flow_info->setText("");
    ui.label_speed->setText("");
    Init();
    connect(ui.actionstart,& QAction::triggered, [=]() {
        if (_bstart)return;
        _bstart = this->m->start();
        if (_bstart)
        {
            char msg[256] = { 0 };
            ::sprintf(msg,"本机IP:%s   网卡名:%s",m->get_card_ip().c_str(), m->get_card_name().c_str());
            ui.label_mac_info->setText(msg);
        }
    });

    connect(ui.actionstop, &QAction::triggered, [=]() {
        this->m->stop();
        _bstart = false;
    });

    connect(tmer, &QTimer::timeout, [=]() {

        FILETIME  fKernel, fUser;
        FILETIME ct, et;
        GetProcessTimes(_pHandle, &ct, &et, &fKernel, &fUser);
        ULARGE_INTEGER    lv_Large_k, lv_Large_u , lst_lv_Large_k, lst_lv_Large_u;

        lv_Large_k.LowPart = fKernel.dwLowDateTime;
        lv_Large_k.HighPart = fKernel.dwHighDateTime;

        lv_Large_u.LowPart = fUser.dwLowDateTime;
        lv_Large_u.HighPart = fUser.dwHighDateTime;

        auto val = lv_Large_k.QuadPart + lv_Large_u.QuadPart;

        auto flow = m->get_net_info(PRO_TCP);
        char msg[256] = { 0 };
        ::sprintf(msg, "本机IP:%s   网卡名:%s   cpu:%.2f%%   tcp:%.3fMB  read:%I64d  write:%I64d", m->get_card_ip().c_str(), m->get_card_name().c_str(), (double)(val- _last_time)/1000000.0 , 
                (double)flow.recv_bytes/ONE_MB ,flow.read_packs , flow.write_packs);
        _last_time = val;
        ui.label_mac_info->setText(msg);

        //GetProcessTimes()
        static decltype(m->get_net_info()) last_info = {0};
        if (!_bstart)return;
        auto info = m->get_net_info();
        sprintf(msg, "read_packs:%I64d  write_packs:%I64d  read_bytes:%.2fKB  write_bytes:%.2fKB", 
            info.read_packs , info.write_packs, (double)info.read_bytes/1024.0, (double)info.write_bytes/1024.0);
        this->ui.label_flow_info->setText(msg);

        memset( msg,0,sizeof(msg)) ;
        sprintf(msg, "read_packs:%I64d/s  write_packs:%I64d/s  read_bytes:%.2fKB/s  write_bytes:%.2fKB/s",
            info.read_packs - last_info.read_packs, info.write_packs - last_info.write_packs,
            (double)(info.read_bytes- last_info.read_bytes) / 1024.0, (double)(info.write_bytes- last_info.write_bytes) / 1024.0);
        last_info = info;
        this->ui.label_speed->setText(msg);

        AUTO_LOCK(_lock);
        for (auto i: _macs)
        {
            i.second->update(1);
        }
        
    });
}


void QtApplication::Init() {
    QTableWidget* tw = ui.tableWidget;
    tw->setColumnWidth(0, 120);
    tw->setColumnWidth(1, 120);
    tw->setColumnWidth(2, 50);
    tw->setColumnWidth(3, 200);
    tw->setColumnWidth(4, 200);
    tw->setColumnWidth(5, 200);
    tw->setColumnWidth(6, 200);

    //self.tableWidget.verticalHeader().setVisible(False) 
    tw->verticalHeader()->setVisible(false);

    _pHandle = OpenProcess(PROCESS_QUERY_INFORMATION, false, getpid());

    FILETIME ct, et,kt,ut;
    ULARGE_INTEGER    lv_Large_k, lv_Large_u;
    GetProcessTimes(_pHandle,&ct,&et, &kt, &ut);
    lv_Large_k.LowPart = kt.dwLowDateTime;
    lv_Large_k.HighPart = kt.dwHighDateTime;

    lv_Large_u.LowPart = et.dwLowDateTime;
    lv_Large_u.HighPart = et.dwHighDateTime;

    _last_time = lv_Large_k.QuadPart + lv_Large_u.QuadPart;

}