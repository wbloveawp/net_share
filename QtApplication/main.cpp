#include "QtApplication.h"
#include <QtWidgets/QApplication>
//#include <QTextCodec>  //添加头文件

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QtApplication w;
    //QTextCodec::setCodecForLocale(QTextCodec::codecForLocale()); //设置编码
    w.show();
    return a.exec();
}
