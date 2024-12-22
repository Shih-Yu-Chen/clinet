#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w(false);  // 如果是 server 端設為 true，client 端設為 false
    w.show();
    return a.exec();
}
