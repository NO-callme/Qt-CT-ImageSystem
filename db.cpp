#include "db.h"
#include <QDebug>

namespace Db {

static MYSQL *g_conn = nullptr;

// 建立到 MySQL 服务器的连接：mysql_init → 设 utf8mb4 字符集 + 5 秒超时 → mysql_real_connect。
// 成功返回 true；失败返回 false，具体原因用 lastError() 获取。
bool connect(const QString &host, int port,
             const QString &user, const QString &password)
{
    close();

    g_conn = mysql_init(nullptr);
    if (!g_conn)
        return false;

    // 统一使用 utf8mb4，保证中文正常存取
    mysql_options(g_conn, MYSQL_SET_CHARSET_NAME, "utf8mb4");
    unsigned int timeout = 5;
    mysql_options(g_conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

    if (!mysql_real_connect(g_conn,
                            host.toUtf8().constData(),
                            user.toUtf8().constData(),
                            password.toUtf8().constData(),
                            nullptr, port, nullptr, 0))
        return false;

    return true;
}

// 关闭当前连接并清空句柄；内部判空，可重复调用。
void close()
{
    if (g_conn) {
        mysql_close(g_conn);
        g_conn = nullptr;
    }
}

// 返回底层 MYSQL* 句柄，供需要直接调 C API 的地方（如 mysql_insert_id）使用。
MYSQL *handle()
{
    return g_conn;
}

// 返回最近一次 MySQL 错误信息；尚未连接时返回提示文本。
QString lastError()
{
    if (!g_conn)
        return QStringLiteral("尚未连接数据库");
    return QString::fromUtf8(mysql_error(g_conn));
}

// 用 mysql_real_escape_string 转义字符串，用于安全拼接 SQL（防注入、防特殊字符破坏语句）。
QString escape(const QString &s)
{
    if (!g_conn)
        return s;

    const QByteArray utf8 = s.toUtf8();
    QByteArray out(static_cast<int>(utf8.size()) * 2 + 1, '\0');
    unsigned long len = mysql_real_escape_string(
        g_conn, out.data(), utf8.constData(), utf8.size());
    return QString::fromUtf8(out.constData(), static_cast<int>(len));
}

// 执行非查询语句（DDL / INSERT / UPDATE / DELETE），失败打印日志并返回 false。
bool exec(const QString &sql)
{
    if (!g_conn)
        return false;
    const QByteArray utf8 = sql.toUtf8();
    if (mysql_real_query(g_conn, utf8.constData(), utf8.size()) != 0) {
        qWarning() << "SQL 执行失败:" << sql << "=>" << lastError();
        return false;
    }
    return true;
}

// 执行 SELECT 并把结果集读成二维表 Result。每字段按 mysql_fetch_lengths 的长度
// 原样保留 UTF-8 / BLOB 字节（不依赖 '\0' 结尾），查询失败返回空表。
Result query(const QString &sql)
{
    Result result;
    if (!g_conn)
        return result;

    const QByteArray utf8 = sql.toUtf8();
    if (mysql_real_query(g_conn, utf8.constData(), utf8.size()) != 0) {
        qWarning() << "SQL 查询失败:" << sql << "=>" << lastError();
        return result;
    }

    MYSQL_RES *res = mysql_store_result(g_conn);
    if (!res)
        return result;

    unsigned int cols = mysql_num_fields(res);
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        unsigned long *lengths = mysql_fetch_lengths(res);
        Row r;
        r.reserve(static_cast<int>(cols));
        for (unsigned int i = 0; i < cols; ++i) {
            if (row[i] == nullptr)
                r.append(QByteArray());
            else
                r.append(QByteArray(row[i], static_cast<int>(lengths[i])));
        }
        result.append(r);
    }
    mysql_free_result(res);
    return result;
}

// 建库（utf8mb4，库名 telemedicine_repro，与 v3 的 telemedicine 隔离、互不干扰）
// + 建 region/patient/medical_record 三张表 + 首次运行时写入种子数据；
// 全程幂等（IF NOT EXISTS + region 为空才播种），可反复调用。
bool initSchema()
{
    if (!g_conn)
        return false;

    // 1) 建库并选中
    if (!exec(QStringLiteral(
            "CREATE DATABASE IF NOT EXISTS telemedicine_repro "
            "DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci")))
        return false;
    if (mysql_select_db(g_conn, "telemedicine_repro") != 0)
        return false;

    // 2) 建表
    if (!exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS region ("
            " id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,"
            " name VARCHAR(64) NOT NULL,"
            " parent_id INT NULL,"
            " CONSTRAINT fk_region_parent FOREIGN KEY (parent_id)"
            "   REFERENCES region(id) ON DELETE CASCADE"
            ") ENGINE=InnoDB")))
        return false;

    if (!exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS patient ("
            " id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,"
            " ssn VARCHAR(32) NOT NULL DEFAULT '',"
            " name VARCHAR(64) NOT NULL,"
            " sex VARCHAR(4) NOT NULL DEFAULT '男',"
            " ethnic VARCHAR(32) NOT NULL DEFAULT '汉',"
            " birth DATE NOT NULL,"
            " region_id INT NULL,"
            " CONSTRAINT fk_patient_region FOREIGN KEY (region_id)"
            "   REFERENCES region(id) ON DELETE SET NULL"
            ") ENGINE=InnoDB")))
        return false;

    if (!exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS medical_record ("
            " id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,"
            " patient_id INT NOT NULL UNIQUE,"
            " content TEXT NULL,"
            " photo LONGBLOB NULL,"
            " updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP"
            "   ON UPDATE CURRENT_TIMESTAMP,"
            " CONSTRAINT fk_record_patient FOREIGN KEY (patient_id)"
            "   REFERENCES patient(id) ON DELETE CASCADE"
            ") ENGINE=InnoDB")))
        return false;

    // 3) 首次运行：地区树为空时写入种子数据
    Result cnt = query(QStringLiteral("SELECT COUNT(*) FROM region"));
    if (!cnt.isEmpty() && cnt[0][0].toInt() == 0) {
        if (!exec(QStringLiteral(
                "INSERT INTO region (id, name, parent_id) VALUES"
                " (1,'鼓楼医院',NULL),"
                " (2,'六合区',1),(3,'浦口区',1),(4,'溧水区',1),(5,'高淳区',1),"
                " (6,'神经内科',2),(7,'肾内科',2),(8,'内分泌科',2),"
                " (9,'呼吸科',2),(10,'消化科',2),(11,'妇科',2),"
                " (12,'肝胆外科',2),(13,'血管外科',2),(14,'心胸外科',2),"
                " (15,'耳鼻咽喉科',2),(16,'急诊科',2),"
                " (17,'妇产科',3),(18,'老年医学科',3),(19,'儿科',3),"
                " (20,'骨科',4),(21,'血管外科',4),(22,'心脏科',4),"
                " (23,'脑科',4),(24,'神经内科',4),"
                " (25,'呼吸科',5),(26,'消化科',5),"
                " (27,'泌尿外科',5),(28,'中医科',5)")))
            return false;

        // 示例病人（分布在不同地区/科室）
        if (!exec(QStringLiteral(
                "INSERT INTO patient (ssn,name,sex,ethnic,birth,region_id) VALUES"
                " ('123456789012345678','张三','男','汉','2000-01-01',6),"
                " ('320199101011806212','赵国庆','男','汉','1976-12-05',22),"
                " ('987654321098765432','李四','女','回','1995-03-15',17)")))
            return false;

        if (!exec(QStringLiteral(
                "INSERT INTO medical_record (patient_id, content) VALUES"
                " (1,'感冒，主诉头痛、发热三天。'),"
                " (2,'高血压病史，定期复查。'),"
                " (3,'反复头痛一月余，建议进一步检查。')")))
            return false;
    }

    return true;
}

}
