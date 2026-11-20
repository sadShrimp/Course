#include "database.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QFile>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>

Database& Database::instance()
{
    static Database d;
    return d;
}

bool Database::open(QString& error)
{
    if (!QSqlDatabase::isDriverAvailable("QODBC")) {
        error = "Драйвер Qt «QODBC» не найден (qsqlodbc.dll). Установите компонент Qt с SQL-драйверами "
                "или скопируйте plugins/sqldrivers рядом с программой.";
        return false;
    }

    const QString path = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings cfg(path, QSettings::IniFormat);
    if (!QFile::exists(path)) {              // первый запуск: создаём шаблон настроек
        cfg.setValue("Database/Driver", "ODBC Driver 17 for SQL Server");
        cfg.setValue("Database/Server", "localhost\\SQLEXPRESS");
        cfg.setValue("Database/Name", "HoldingAnalytics");
        cfg.setValue("Database/TrustedConnection", true);   // Windows-авторизация
        cfg.setValue("Database/Login", "");
        cfg.setValue("Database/Password", "");
        cfg.sync();
    }

    const QString driver  = cfg.value("Database/Driver", "ODBC Driver 17 for SQL Server").toString();
    const QString server  = cfg.value("Database/Server", "localhost\\SQLEXPRESS").toString();
    const QString name    = cfg.value("Database/Name", "HoldingAnalytics").toString();
    const bool trusted    = cfg.value("Database/TrustedConnection", true).toBool();
    const QString login   = cfg.value("Database/Login").toString();
    const QString pass    = cfg.value("Database/Password").toString();

    QString conn = QString("DRIVER={%1};SERVER=%2;DATABASE=%3;").arg(driver, server, name);
    if (trusted)
        conn += "Trusted_Connection=yes;";
    else
        conn += QString("UID=%1;PWD=%2;").arg(login, pass);
    conn += "TrustServerCertificate=yes;";

    QSqlDatabase db = QSqlDatabase::addDatabase("QODBC");
    db.setDatabaseName(conn);
    if (!db.open()) {
        error = db.lastError().text();
        return false;
    }
    return true;
}

bool Database::exec(const QString& sql, const QVariantList& params, QString* error)
{
    QSqlQuery q(QSqlDatabase::database());
    q.prepare(sql);
    for (const QVariant& p : params)
        q.addBindValue(p);
    if (!q.exec()) {
        if (error) *error = q.lastError().text();
        qWarning() << "SQL error:" << q.lastError().text() << sql;
        return false;
    }
    return true;
}

QSqlQuery Database::query(const QString& sql, const QVariantList& params, QString* error)
{
    QSqlQuery q(QSqlDatabase::database());
    q.prepare(sql);
    for (const QVariant& p : params)
        q.addBindValue(p);
    if (!q.exec()) {
        if (error) *error = q.lastError().text();
        qWarning() << "SQL error:" << q.lastError().text() << sql;
    }
    return q;
}

QVariant Database::scalar(const QString& sql, const QVariantList& params)
{
    QSqlQuery q = query(sql, params);
    QVariant result;
    if (q.next())
        result = q.value(0);
    q.finish();
    return result;
}

bool Database::transaction() { return QSqlDatabase::database().transaction(); }
bool Database::commit()      { return QSqlDatabase::database().commit(); }
bool Database::rollback()    { return QSqlDatabase::database().rollback(); }

QString Database::hashPassword(const QString& password)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex().toUpper());
}

QDate Database::toDate(const QVariant& v)
{
    if (v.metaType().id() == QMetaType::QDate)
        return v.toDate();
    if (v.metaType().id() == QMetaType::QDateTime)
        return v.toDateTime().date();
    return QDate::fromString(v.toString().left(10), Qt::ISODate);
}

QVariant Database::nullInt()
{
    return QVariant(QMetaType::fromType<int>());
}
