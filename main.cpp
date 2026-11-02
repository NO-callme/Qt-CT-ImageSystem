#include "db.h"
#include "mainwindow.h"
#include <QApplication>
#include <QMessageBox>
#include <QSettings>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 连接参数放在 exe 同目录的 dbconfig.ini 里，按自己 MySQL 的账号密码修改
    QSettings cfg(QCoreApplication::applicationDirPath() + "/dbconfig.ini",
                  QSettings::IniFormat);
    const QString host = cfg.value("mysql/host", "127.0.0.1").toString();
    const int     port = cfg.value("mysql/port", 3306).toInt();
    const QString user = cfg.value("mysql/user", "root").toString();
    const QString pass = cfg.value("mysql/password", "").toString();

    if (!Db::connect(host, port, user, pass)) {
        QMessageBox::critical(nullptr, u8"数据库连接失败",
                              u8"无法连接 MySQL：\n" + Db::lastError() +
                              u8"\n\n请检查 dbconfig.ini 里的主机/端口/账号密码。");
        return 1;
    }

    if (!Db::initSchema()) {
        QMessageBox::critical(nullptr, u8"数据库初始化失败", Db::lastError());
        return 1;
    }

    MainWindow w;
    w.show();
    return a.exec();
}
