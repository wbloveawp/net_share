#include "QtApplication.h"
#include <QtWidgets/QApplication>
//#include <QTextCodec>  //���ͷ�ļ�

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QtApplication w;
    //QTextCodec::setCodecForLocale(QTextCodec::codecForLocale()); //���ñ���
    w.show();
    return a.exec();
}
