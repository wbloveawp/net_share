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
            ::sprintf(msg,"±¾»úIP:%s   Íø¿¨Ãû:%s",m->get_card_ip().c_str(), m->get_card_name().c_str());
            ui.label_mac_info->setText(msg);
        }
    });

    connect(ui.actionstop, &QAction::triggered, [=]() {
        this->m->stop();
        for (auto i : _macs)
        {
            delete i.second;
        }
        _macs.clear();
        ui.tableWidget->clear();
        _bstart = false;
    });

    connect(tmer, &QTimer::timeout, [=]() {
        static decltype(m->get_net_info()) last_info = {0};
        if (!_bstart)return;
        auto info = m->get_net_info();
        char msg[128] = { 0 };
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
    tw->setColumnWidth(0, 100);
    tw->setColumnWidth(1, 120);
    tw->setColumnWidth(2, 50);
    tw->setColumnWidth(3, 200);
    tw->setColumnWidth(4, 200);
    tw->setColumnWidth(5, 200);
    tw->setColumnWidth(6, 200);
}