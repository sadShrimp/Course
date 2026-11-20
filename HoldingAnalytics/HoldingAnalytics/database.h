#pragma once
#include <QString>
#include <QVariant>
#include <QSqlQuery>
#include <QDate>

/* Подключение к MS SQL Server через ODBC (драйвер Qt: QODBC).
   Параметры берутся из config.ini, лежащего рядом с exe. */
class Database
{
public:
    static Database& instance();

    bool open(QString& error);

    bool exec(const QString& sql, const QVariantList& params = {}, QString* error = nullptr);
    QSqlQuery query(const QString& sql, const QVariantList& params = {}, QString* error = nullptr);
    QVariant scalar(const QString& sql, const QVariantList& params = {});

    bool transaction();
    bool commit();
    bool rollback();

    static QString hashPassword(const QString& password);
    static QDate toDate(const QVariant& v);
    static QVariant nullInt();

private:
    Database() = default;
};
