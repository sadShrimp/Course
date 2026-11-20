#include "dataservice.h"
#include "database.h"

#include <QDate>
#include <QRegularExpression>
#include <QSqlQuery>
#include <cmath>

namespace {

Database& db() { return Database::instance(); }

Result dbFail(const QString& e) { return Result::fail("Ошибка базы данных: " + e); }

bool parseNumber(const QString& text, double& out)
{
    QString t = text.trimmed();
    t.replace(',', '.');
    bool ok = false;
    out = t.toDouble(&ok);
    return ok && std::isfinite(out);
}

bool isPersonName(const QString& s)
{
    static const QRegularExpression re(QStringLiteral("^[\\p{L}][\\p{L}\\s.\\-]*$"),
                                       QRegularExpression::UseUnicodePropertiesOption);
    return re.match(s).hasMatch();
}

QList<Option> loadOptions(const QString& sql)
{
    QList<Option> list;
    QSqlQuery q = db().query(sql);
    while (q.next())
        list.append(Option(q.value(1).toString(), q.value(0)));
    return list;
}

Result removeChecked(const QString& table, int id, const QString& depSql,
                     const QVariantList& depParams, const QString& depMessage)
{
    if (db().scalar(depSql, depParams).toInt() > 0)
        return Result::fail(depMessage);
    QString err;
    if (!db().exec("DELETE FROM " + table + " WHERE id = ?", {id}, &err))
        return dbFail(err);
    return Result::success();
}

/* ---------- предприятия ---------- */
Result saveEnterprise(int id, const QVariantMap& v)
{
    const QString name      = v["name"].toString().trimmed();
    const QString requisite = v["requisites"].toString().trimmed();
    const QString phone     = v["phone"].toString().trimmed();
    const QString contact   = v["contact_person"].toString().trimmed();

    if (name.isEmpty() || requisite.isEmpty() || phone.isEmpty() || contact.isEmpty())
        return Result::fail("Заполните все поля формы");
    static const QRegularExpression phoneRe(QStringLiteral("^\\+?[\\d\\s\\-()]{5,20}$"));
    if (!phoneRe.match(phone).hasMatch())
        return Result::fail("Телефон указан в неверном формате");
    if (!isPersonName(contact))
        return Result::fail("Контактное лицо: допустимы только буквы, пробелы, точки и дефисы");
    if (db().scalar("SELECT COUNT(*) FROM Enterprises WHERE name = ? AND id <> ?", {name, id}).toInt() > 0)
        return Result::fail("Предприятие с таким названием уже существует");

    QString err;
    bool ok = (id == 0)
        ? db().exec("INSERT INTO Enterprises(name, requisites, phone, contact_person) VALUES(?,?,?,?)",
                    {name, requisite, phone, contact}, &err)
        : db().exec("UPDATE Enterprises SET name=?, requisites=?, phone=?, contact_person=? WHERE id=?",
                    {name, requisite, phone, contact, id}, &err);
    return ok ? Result::success() : dbFail(err);
}

/* ---------- показатели ---------- */
Result saveIndicator(int id, const QVariantMap& v)
{
    const QString name = v["name"].toString().trimmed();
    const QString unit = v["unit"].toString().trimmed();
    double weight = 0;

    if (name.isEmpty())
        return Result::fail("Название показателя не может быть пустым");
    if (unit.isEmpty())
        return Result::fail("Укажите единицу измерения");
    if (!parseNumber(v["weight"].toString(), weight) || weight <= 0)
        return Result::fail("Вес показателя должен быть положительным числом");
    if (db().scalar("SELECT COUNT(*) FROM Indicators WHERE name = ? AND id <> ?", {name, id}).toInt() > 0)
        return Result::fail("Показатель с таким названием уже существует");

    const int cur = v["currency_id"].toInt();
    const QVariant curValue = cur > 0 ? QVariant(cur) : Database::nullInt();

    QString err;
    bool ok = (id == 0)
        ? db().exec("INSERT INTO Indicators(name, weight, unit, currency_id) VALUES(?,?,?,?)",
                    {name, weight, unit, curValue}, &err)
        : db().exec("UPDATE Indicators SET name=?, weight=?, unit=?, currency_id=? WHERE id=?",
                    {name, weight, unit, curValue, id}, &err);
    return ok ? Result::success() : dbFail(err);
}

} // namespace

namespace DataService {

bool authenticate(const QString& login, const QString& password, User& user)
{
    QSqlQuery q = db().query("SELECT id, login, fio, role FROM Users WHERE login = ? AND password_hash = ?",
                             {login.trimmed(), Database::hashPassword(password)});
    if (!q.next())
        return false;
    user.id    = q.value(0).toInt();
    user.login = q.value(1).toString();
    user.fio   = q.value(2).toString();
    user.role  = q.value(3).toString().trimmed();
    return true;
}

QList<Option> currencyOptions(bool withNone)
{
    QList<Option> list;
    if (withNone)
        list.append(Option("— без валюты —", 0));
    list += loadOptions("SELECT id, code + N' — ' + name FROM Currencies ORDER BY code");
    return list;
}

QList<Option> enterpriseOptions() { return loadOptions("SELECT id, name FROM Enterprises ORDER BY name"); }
QList<Option> periodOptions()     { return loadOptions("SELECT id, name FROM Periods ORDER BY start_date"); }

QList<Option> roleOptions()
{
    return { Option("Администратор", "admin"), Option("Аналитик", "analyst") };
}

/* ---------- предприятия ---------- */
Result addEnterprise(const QVariantMap& v)            { return saveEnterprise(0, v); }
Result updateEnterprise(int id, const QVariantMap& v) { return saveEnterprise(id, v); }
Result deleteEnterprise(int id)
{
    return removeChecked("Enterprises", id,
        "SELECT COUNT(*) FROM IndicatorValues WHERE enterprise_id = ?", {id},
        "Нельзя удалить предприятие: для него введены значения показателей");
}

/* ---------- показатели ---------- */
Result addIndicator(const QVariantMap& v)            { return saveIndicator(0, v); }
Result updateIndicator(int id, const QVariantMap& v) { return saveIndicator(id, v); }
Result deleteIndicator(int id)
{
    return removeChecked("Indicators", id,
        "SELECT COUNT(*) FROM IndicatorValues WHERE indicator_id = ?", {id},
        "Нельзя удалить показатель: для него введены значения");
}

/* ---------- валюты ---------- */
Result addCurrency(const QVariantMap& v)
{
    const QString code = v["code"].toString().trimmed().toUpper();
    const QString name = v["name"].toString().trimmed();
    static const QRegularExpression codeRe(QStringLiteral("^[A-Z]{3}$"));

    if (code.isEmpty() || name.isEmpty())
        return Result::fail("Заполните код и название валюты");
    if (!codeRe.match(code).hasMatch())
        return Result::fail("Код валюты должен состоять из трёх латинских букв (например, USD)");
    if (db().scalar("SELECT COUNT(*) FROM Currencies WHERE code = ?", {code}).toInt() > 0)
        return Result::fail("Валюта с таким кодом уже существует");

    QString err;
    return db().exec("INSERT INTO Currencies(code, name) VALUES(?, ?)", {code, name}, &err)
               ? Result::success() : dbFail(err);
}

Result updateCurrency(int id, const QVariantMap& v)
{
    const QString name = v["name"].toString().trimmed();
    if (name.isEmpty())
        return Result::fail("Название валюты не может быть пустым");
    QString err;
    return db().exec("UPDATE Currencies SET name = ? WHERE id = ?", {name, id}, &err)
               ? Result::success() : dbFail(err);
}

Result deleteCurrency(int id)
{
    return removeChecked("Currencies", id,
        "SELECT (SELECT COUNT(*) FROM Indicators WHERE currency_id = ?) + "
        "(SELECT COUNT(*) FROM ExchangeRates WHERE from_currency_id = ? OR to_currency_id = ?)",
        {id, id, id},
        "Нельзя удалить валюту: она используется в показателях или курсах");
}

/* ---------- курсы ---------- */
Result addRate(const QVariantMap& v)
{
    const QDate date = v["date"].toDate();
    const int from = v["from"].toInt();
    const int to   = v["to"].toInt();
    double rate = 0;

    if (!date.isValid() || from <= 0 || to <= 0)
        return Result::fail("Заполните дату и обе валюты");
    if (from == to)
        return Result::fail("Выберите две разные валюты");
    if (!parseNumber(v["rate"].toString(), rate) || rate <= 0)
        return Result::fail("Курс должен быть положительным числом");
    if (db().scalar("SELECT COUNT(*) FROM ExchangeRates WHERE rate_date = ? AND from_currency_id = ? AND to_currency_id = ?",
                    {date.toString(Qt::ISODate), from, to}).toInt() > 0)
        return Result::fail("Курс этой пары валют на выбранную дату уже введён");

    QString err;
    return db().exec("INSERT INTO ExchangeRates(rate_date, from_currency_id, to_currency_id, rate) VALUES(?,?,?,?)",
                     {date.toString(Qt::ISODate), from, to, rate}, &err)
               ? Result::success() : dbFail(err);
}

Result deleteRate(int id)
{
    QString err;
    return db().exec("DELETE FROM ExchangeRates WHERE id = ?", {id}, &err) ? Result::success() : dbFail(err);
}

/* ---------- периоды ---------- */
Result addPeriod(const QVariantMap& v)
{
    const QString name = v["name"].toString().trimmed();
    const QDate start = v["start"].toDate();
    const QDate end   = v["end"].toDate();

    if (name.isEmpty())
        return Result::fail("Название периода не может быть пустым");
    if (!start.isValid() || !end.isValid() || start > end)
        return Result::fail("Дата начала не может быть позже даты окончания");
    if (db().scalar("SELECT COUNT(*) FROM Periods WHERE name = ?", {name}).toInt() > 0)
        return Result::fail("Период с таким названием уже существует");
    if (db().scalar("SELECT COUNT(*) FROM Periods WHERE start_date <= ? AND end_date >= ?",
                    {end.toString(Qt::ISODate), start.toString(Qt::ISODate)}).toInt() > 0)
        return Result::fail("Период пересекается с уже существующим");

    QString err;
    return db().exec("INSERT INTO Periods(name, start_date, end_date) VALUES(?,?,?)",
                     {name, start.toString(Qt::ISODate), end.toString(Qt::ISODate)}, &err)
               ? Result::success() : dbFail(err);
}

Result deletePeriod(int id)
{
    return removeChecked("Periods", id,
        "SELECT COUNT(*) FROM IndicatorValues WHERE period_id = ?", {id},
        "Нельзя удалить период: для него введены значения показателей");
}

/* ---------- пользователи ---------- */
Result addUser(const QVariantMap& v)
{
    const QString login = v["login"].toString().trimmed();
    const QString fio   = v["fio"].toString().trimmed();
    const QString pass  = v["password"].toString();
    const QString pass2 = v["password2"].toString();
    const QString role  = v["role"].toString();
    static const QRegularExpression loginRe(QStringLiteral("^[A-Za-z0-9_]{3,30}$"));

    if (login.isEmpty() || fio.isEmpty() || pass.isEmpty() || pass2.isEmpty() || role.isEmpty())
        return Result::fail("Заполните все поля формы");
    if (!loginRe.match(login).hasMatch())
        return Result::fail("Логин: 3–30 символов, латинские буквы, цифры и «_»");
    if (!isPersonName(fio))
        return Result::fail("ФИО может содержать только буквы, пробелы, точки и дефисы");
    if (pass.length() < 4)
        return Result::fail("Пароль должен содержать не менее 4 символов");
    if (pass != pass2)
        return Result::fail("Пароли не совпадают");
    if (db().scalar("SELECT COUNT(*) FROM Users WHERE login = ?", {login}).toInt() > 0)
        return Result::fail("Пользователь с таким логином уже существует");

    QString err;
    return db().exec("INSERT INTO Users(login, fio, password_hash, role) VALUES(?,?,?,?)",
                     {login, fio, Database::hashPassword(pass), role}, &err)
               ? Result::success() : dbFail(err);
}

Result deleteUser(int id, int currentUserId)
{
    if (id == currentUserId)
        return Result::fail("Нельзя удалить собственную учётную запись");
    QString err;
    return db().exec("DELETE FROM Users WHERE id = ?", {id}, &err) ? Result::success() : dbFail(err);
}

Result changePassword(int id, const QVariantMap& v)
{
    const QString pass  = v["password"].toString();
    const QString pass2 = v["password2"].toString();
    if (pass.isEmpty() || pass2.isEmpty())
        return Result::fail("Введите новый пароль и подтверждение");
    if (pass.length() < 4)
        return Result::fail("Пароль должен содержать не менее 4 символов");
    if (pass != pass2)
        return Result::fail("Пароли не совпадают");
    QString err;
    return db().exec("UPDATE Users SET password_hash = ? WHERE id = ?", {Database::hashPassword(pass), id}, &err)
               ? Result::success() : dbFail(err);
}

/* ---------- значения показателей ---------- */
Result saveValues(int enterpriseId, int periodId, const QVector<QPair<int, QString>>& rows)
{
    if (enterpriseId <= 0 || periodId <= 0)
        return Result::fail("Выберите предприятие и отчётный период");

    QVector<QPair<int, double>> toSave;
    QVector<int> toDelete;
    for (const auto& r : rows) {
        const QString text = r.second.trimmed();
        if (text.isEmpty()) {
            toDelete.append(r.first);
            continue;
        }
        double value = 0;
        if (!parseNumber(text, value)) {
            const QString name = db().scalar("SELECT name FROM Indicators WHERE id = ?", {r.first}).toString();
            return Result::fail(QString("Показатель «%1»: «%2» не является числом").arg(name, text));
        }
        toSave.append(qMakePair(r.first, value));
    }

    db().transaction();
    QString err;
    for (int indId : toDelete) {
        if (!db().exec("DELETE FROM IndicatorValues WHERE enterprise_id=? AND indicator_id=? AND period_id=?",
                       {enterpriseId, indId, periodId}, &err)) {
            db().rollback();
            return dbFail(err);
        }
    }
    for (const auto& s : toSave) {
        if (!db().exec("DELETE FROM IndicatorValues WHERE enterprise_id=? AND indicator_id=? AND period_id=?",
                       {enterpriseId, s.first, periodId}, &err) ||
            !db().exec("INSERT INTO IndicatorValues(enterprise_id, indicator_id, period_id, value) VALUES(?,?,?,?)",
                       {enterpriseId, s.first, periodId, s.second}, &err)) {
            db().rollback();
            return dbFail(err);
        }
    }
    db().commit();
    return Result::success();
}

} // namespace DataService
