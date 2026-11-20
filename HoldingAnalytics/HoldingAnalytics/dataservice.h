#pragma once
#include <QList>
#include <QPair>
#include <QString>
#include <QVariantMap>
#include <QVector>

struct Result
{
    bool ok = true;
    QString message;
    static Result success() { return Result(); }
    static Result fail(const QString& m) { Result r; r.ok = false; r.message = m; return r; }
};

using Option = QPair<QString, QVariant>;   // (подпись, значение)

struct User
{
    int id = 0;
    QString login;
    QString fio;
    QString role;   // "admin" | "analyst"
};

/* Бизнес-логика: валидация и операции над данными. Слой GUI сюда не лезет в SQL. */
namespace DataService
{
bool authenticate(const QString& login, const QString& password, User& user);

QList<Option> currencyOptions(bool withNone = false);
QList<Option> enterpriseOptions();
QList<Option> periodOptions();
QList<Option> roleOptions();

Result addEnterprise(const QVariantMap& v);
Result updateEnterprise(int id, const QVariantMap& v);
Result deleteEnterprise(int id);

Result addIndicator(const QVariantMap& v);
Result updateIndicator(int id, const QVariantMap& v);
Result deleteIndicator(int id);

Result addCurrency(const QVariantMap& v);
Result updateCurrency(int id, const QVariantMap& v);
Result deleteCurrency(int id);

Result addRate(const QVariantMap& v);
Result deleteRate(int id);

Result addPeriod(const QVariantMap& v);
Result deletePeriod(int id);

Result addUser(const QVariantMap& v);
Result deleteUser(int id, int currentUserId);
Result changePassword(int id, const QVariantMap& v);

Result saveValues(int enterpriseId, int periodId, const QVector<QPair<int, QString>>& rows);
}
