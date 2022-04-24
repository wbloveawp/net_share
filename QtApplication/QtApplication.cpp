#include "QtApplication.h"
#include <qstringlistmodel.h>
#include <QTableView>
#include <QtGui/qstandarditemmodel.h>



class wb_link_item :public QTableWidgetItem
{
    wb_link_interface* _lk = nullptr;
public:
    explicit wb_link_item(const QString& text, wb_link_interface* lk) :QTableWidgetItem(text), _lk(lk) {}
    virtual ~wb_link_item() {
        _lk = nullptr;
    }
    explicit operator wb_link_interface*(){return _lk; }
    const wb_link_interface* get_link() { return _lk; };
};


QtApplication::QtApplication(QWidget *parent)
    : QMainWindow(parent)
{
    m = new mainer(this);
    ui.setupUi(this);
    tmer = new QTimer(this);
    tmer->start(1000);
    ui.label_info->setText("");
    ui.label_speed->setText("");
    Init();
    connect(ui.actionstart,& QAction::triggered, [=]() {
        if (_bstart)return;
        _bstart = this->m->start();
    });

    connect(ui.actionstop, &QAction::triggered, [=]() {
        this->m->stop();
        _bstart = false;
    });

    connect(tmer, &QTimer::timeout, [=]() {
        static decltype(m->get_net_info()) last_info = {0};
        auto info = m->get_net_info();
        char msg[128] = { 0 };
        sprintf(msg, "read_packs:%I64d  write_packs:%I64d  read_bytes:%.2fKB  write_bytes:%.2fKB", 
            info.read_packs , info.write_packs, (double)info.read_bytes/1024.0, (double)info.write_bytes/1024.0);
        this->ui.label_info->setText(msg);
        memset( msg,0,sizeof(msg)) ;
        sprintf(msg, "read_packs:%I64d/s  write_packs:%I64d/s  read_bytes:%.2fKB/s  write_bytes:%.2fKB/s",
            info.read_packs - last_info.read_packs, info.write_packs - last_info.write_packs,
            (double)(info.read_bytes- last_info.read_bytes) / 1024.0, (double)(info.write_bytes- last_info.write_bytes) / 1024.0);
        last_info = info;
        this->ui.label_speed->setText(msg);
    });
}


void QtApplication::AddLink(wb_link_interface* lk)
{
    if (lk->get_Protocol() == PRO_UDP)return;
    auto item = new wb_link_item("TCP", lk);

    if (item)
    {
        auto& ip_info = lk->get_link_info();
        char szMac[20] = {};
        int len = 0;
        for (auto c : ip_info.mac)
        {
            sprintf(szMac + len, "%x ", c);
            len += 3;
        }
        sockaddr_in addr = {};
        addr.sin_addr.S_un.S_addr = ip_info.ip_info.ip_info.dst_ip;
        char szAddr[32] = { 0 };
        sprintf(szAddr,"%s:%d", inet_ntoa(addr.sin_addr), htons(ip_info.ip_info.ip_info.dst_port));
        assert(szAddr[0]);
        lk->set_data(item);
        QTableWidget* tw = ui.tableWidget;
        AUTO_LOCK(_ui_lock);
        tw->insertRow(0);
        tw->setItem(0, 0, item);
        tw->setItem(0, 1, new wb_link_item(szAddr, lk));
        tw->setItem(0, 2, new wb_link_item("",lk));
        tw->setItem(0, 3, new wb_link_item("", lk));
        tw->setItem(0, 4, new wb_link_item(szMac, lk));
    }
}

void QtApplication::DelLink(wb_link_interface* lk)
{
    if (lk->get_Protocol() == PRO_UDP)return;
    auto itm = (wb_link_item*)lk->get_data();
    auto tw = ui.tableWidget;
    auto row = itm->row();
    AUTO_LOCK(_ui_lock);
    delete (wb_link_item*)tw->item(row, 0);
    delete (wb_link_item*)tw->item(row, 1);
    delete (wb_link_item*)tw->item(row, 2);
    delete (wb_link_item*)tw->item(row, 3);
    delete (wb_link_item*)tw->item(row, 4);

    tw->removeRow(row);

}


void QtApplication::Init() {
    QTableWidget* tw = ui.tableWidget;
    tw->setColumnWidth(0, 80);
    tw->setColumnWidth(1, 150);
    tw->setColumnWidth(2, 150);
    tw->setColumnWidth(3, 150);
    tw->setColumnWidth(4, 150);
}