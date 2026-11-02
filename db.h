#ifndef DB_H
#define DB_H

#include <mysql.h>
#include <QString>
#include <QVector>
#include <QByteArray>

// 轻量 MySQL 访问层：直接用 MySQL C API（libmysql），
// 因为本机 Qt 没有 qsqlmysql 插件、也没装 MySQL ODBC 驱动。
namespace Db {

// 一行结果：每个字段都是原始字节（UTF-8 / BLOB 原样保留）
using Row = QVector<QByteArray>;
using Result = QVector<Row>;

// 连接到 MySQL 服务器（不选库，库由 initSchema 创建/选择）
bool connect(const QString &host, int port,
             const QString &user, const QString &password);
void close();
MYSQL *handle();

QString lastError();

// 转义字符串，用于拼 SQL（仅本项目内部使用；生产环境请用 prepared statement）
QString escape(const QString &s);

// 执行非查询语句（DDL / INSERT / UPDATE / DELETE）
bool exec(const QString &sql);

// 执行查询，返回二维结果表
Result query(const QString &sql);

// 建库建表 + 首次运行时写入种子数据（地区树、示例病人）
bool initSchema();

}

#endif // DB_H
