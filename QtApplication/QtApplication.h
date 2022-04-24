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

class QtApplication : public QMainWindow ,public wb_link_event
{
    Q_OBJECT

public:
    QtApplication(QWidget *parent = Q_NULLPTR);
    virtual ~QtApplication() {
        if (m)
            delete m;
    }
    void Init();
private:
    Ui::QtApplicationClass ui;
    QTimer* tmer;
    QMenuBar* menuBar;

    wb_lock     _ui_lock;
protected:
    mainer* m;
    bool _bstart = false;

    void AddLink(wb_link_interface* lk);
    void DelLink(wb_link_interface* lk);
public:
    virtual bool OnNewLink(wb_link_interface* lk) {
    
        AddLink(lk);
        return true;
    };
    virtual void OnCloseLink(wb_link_interface* lk) {
        DelLink(lk);
    };
};
