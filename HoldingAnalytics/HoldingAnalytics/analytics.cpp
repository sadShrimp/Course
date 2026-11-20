#include "analytics.h"
#include "database.h"

#include <QDateTime>
#include <QHash>
#include <QMap>
#include <algorithm>
#include <cmath>

namespace {

Database& db() { return Database::instance(); }

struct PeriodInfo { int id; QString name; QDate start; QDate end; };
struct IndInfo    { int id; QString name; QString unit; int cur; double weight; };

QString currencyCode(int id)
{
    return db().scalar("SELECT code FROM Currencies WHERE id = ?", {id}).toString().trimmed();
}

std::optional<std::pair<QDate, double>> lastRate(int from, int to, const QDate& date)
{
    QSqlQuery q = db().query(
        "SELECT TOP 1 rate_date, rate FROM ExchangeRates "
        "WHERE from_currency_id = ? AND to_currency_id = ? AND rate_date <= ? ORDER BY rate_date DESC",
        {from, to, date.toString(Qt::ISODate)});
    std::optional<std::pair<QDate, double>> res;
    if (q.next())
        res = std::make_pair(Database::toDate(q.value(0)), q.value(1).toDouble());
    q.finish();
    return res;
}

std::optional<double> directOrInverse(int from, int to, const QDate& d)
{
    auto a = lastRate(from, to, d);
    auto b = lastRate(to, from, d);
    if (a && (!b || a->first >= b->first))
        return a->second;
    if (b && b->second != 0)
        return 1.0 / b->second;
    return std::nullopt;
}

bool convert(double value, int indCurrency, int target, const QDate& date, double& out, QString& err)
{
    if (indCurrency == 0) {            // безразмерный показатель
        out = value;
        return true;
    }
    auto r = Analytics::findRate(indCurrency, target, date);
    if (!r) {
        err = QString("Не найден курс %1 → %2 на %3")
                  .arg(currencyCode(indCurrency), currencyCode(target), date.toString("dd.MM.yyyy"));
        return false;
    }
    out = value * (*r);
    return true;
}

bool loadPeriods(int pFrom, int pTo, QVector<PeriodInfo>& out, QString& err)
{
    const QDate s1 = Database::toDate(db().scalar("SELECT start_date FROM Periods WHERE id = ?", {pFrom}));
    const QDate s2 = Database::toDate(db().scalar("SELECT start_date FROM Periods WHERE id = ?", {pTo}));
    if (!s1.isValid() || !s2.isValid()) {
        err = "Выберите начальный и конечный отчётные периоды";
        return false;
    }
    if (s1 > s2) {
        err = "Начальный период позже конечного";
        return false;
    }
    QSqlQuery q = db().query("SELECT id, name, start_date, end_date FROM Periods "
                             "WHERE start_date >= ? AND start_date <= ? ORDER BY start_date",
                             {s1.toString(Qt::ISODate), s2.toString(Qt::ISODate)});
    while (q.next())
        out.append(PeriodInfo{q.value(0).toInt(), q.value(1).toString(),
                              Database::toDate(q.value(2)), Database::toDate(q.value(3))});
    q.finish();
    return !out.isEmpty();
}

QVector<IndInfo> loadIndicators()
{
    QVector<IndInfo> v;
    QSqlQuery q = db().query("SELECT id, name, unit, ISNULL(currency_id, 0), weight FROM Indicators ORDER BY name");
    while (q.next())
        v.append(IndInfo{q.value(0).toInt(), q.value(1).toString(), q.value(2).toString(),
                         q.value(3).toInt(), q.value(4).toDouble()});
    q.finish();
    return v;
}

QString pad(const QString& s, int w)   { return s.leftJustified(w, ' ', true); }
QString padR(const QString& s, int w)  { return s.rightJustified(w, ' ', true); }
QString num(double v)                  { return QString::number(v, 'f', 2); }

} // namespace

namespace Analytics {

std::optional<double> findRate(int from, int to, const QDate& date)
{
    if (from == to)
        return 1.0;
    if (auto r = directOrInverse(from, to, date))
        return r;

    // кросс-курс через третью валюту
    QList<int> mids;
    QSqlQuery q = db().query("SELECT id FROM Currencies WHERE id <> ? AND id <> ?", {from, to});
    while (q.next())
        mids.append(q.value(0).toInt());
    q.finish();

    for (int m : mids) {
        auto r1 = directOrInverse(from, m, date);
        auto r2 = directOrInverse(m, to, date);
        if (r1 && r2)
            return (*r1) * (*r2);
    }
    return std::nullopt;
}

DynResult dynamics(int enterpriseId, int periodFrom, int periodTo, int currencyId)
{
    DynResult r;
    if (enterpriseId <= 0) { r.error = "Выберите предприятие"; return r; }
    if (currencyId <= 0)   { r.error = "Выберите валюту приведения"; return r; }

    QVector<PeriodInfo> periods;
    if (!loadPeriods(periodFrom, periodTo, periods, r.error)) {
        if (r.error.isEmpty()) r.error = "За выбранный интервал нет отчётных периодов";
        return r;
    }
    r.enterprise = db().scalar("SELECT name FROM Enterprises WHERE id = ?", {enterpriseId}).toString();
    r.currency = currencyCode(currencyId);
    for (const auto& p : periods)
        r.periods << p.name;

    const QVector<IndInfo> inds = loadIndicators();
    r.rows.resize(inds.size());
    for (int i = 0; i < inds.size(); ++i) {
        r.rows[i].name = inds[i].name;
        r.rows[i].unit = inds[i].unit + (inds[i].cur ? " " + r.currency : QString());
        r.rows[i].values.fill(0.0, periods.size());
        r.rows[i].has.fill(false, periods.size());
    }

    bool any = false;
    for (int pi = 0; pi < periods.size(); ++pi) {
        QMap<int, double> raw;
        QSqlQuery q = db().query("SELECT indicator_id, value FROM IndicatorValues WHERE enterprise_id = ? AND period_id = ?",
                                 {enterpriseId, periods[pi].id});
        while (q.next())
            raw[q.value(0).toInt()] = q.value(1).toDouble();
        q.finish();

        for (int i = 0; i < inds.size(); ++i) {
            if (!raw.contains(inds[i].id))
                continue;
            double conv = 0;
            if (!convert(raw[inds[i].id], inds[i].cur, currencyId, periods[pi].end, conv, r.error))
                return r;
            r.rows[i].values[pi] = conv;
            r.rows[i].has[pi] = true;
            any = true;
        }
    }
    if (!any) {
        r.error = "За выбранный период данных по предприятию нет";
        return r;
    }

    r.rows.erase(std::remove_if(r.rows.begin(), r.rows.end(),
                                [](const DynRow& row) { return !row.has.contains(true); }),
                 r.rows.end());

    for (DynRow& row : r.rows) {
        int first = -1, last = -1;
        for (int k = 0; k < row.has.size(); ++k)
            if (row.has[k]) { if (first < 0) first = k; last = k; }
        if (first >= 0 && last > first) {
            row.changeValid = true;
            row.absChange = row.values[last] - row.values[first];
            if (row.values[first] != 0) {
                row.pctValid = true;
                row.pctChange = row.absChange / std::fabs(row.values[first]) * 100.0;
            }
        }
    }
    r.ok = true;
    return r;
}

RatingResult rating(int periodFrom, int periodTo, int currencyId)
{
    RatingResult res;
    if (currencyId <= 0) { res.error = "Выберите валюту приведения"; return res; }

    QVector<PeriodInfo> periods;
    if (!loadPeriods(periodFrom, periodTo, periods, res.error)) {
        if (res.error.isEmpty()) res.error = "За выбранный интервал нет отчётных периодов";
        return res;
    }
    res.currency = currencyCode(currencyId);
    for (const auto& p : periods)
        res.periods << p.name;

    const QVector<IndInfo> inds = loadIndicators();

    struct Ent { int id; QString name; };
    QVector<Ent> ents;
    QSqlQuery qe = db().query("SELECT id, name FROM Enterprises ORDER BY name");
    while (qe.next())
        ents.append(Ent{qe.value(0).toInt(), qe.value(1).toString()});
    qe.finish();

    if (ents.isEmpty() || inds.isEmpty()) {
        res.error = "В системе нет предприятий или показателей";
        return res;
    }

    QHash<int, int> ei, ii;
    for (int e = 0; e < ents.size(); ++e)  ei[ents[e].id] = e;
    for (int i = 0; i < inds.size(); ++i)  ii[inds[i].id] = i;
    const int E = ents.size(), I = inds.size();

    QVector<QVector<double>> sum(E, QVector<double>(I, 0.0));
    QVector<QVector<int>> cnt(E, QVector<int>(I, 0));

    for (const auto& p : periods) {
        struct Raw { int e; int i; double v; };
        QVector<Raw> raws;
        QSqlQuery q = db().query("SELECT enterprise_id, indicator_id, value FROM IndicatorValues WHERE period_id = ?", {p.id});
        while (q.next())
            raws.append(Raw{ei.value(q.value(0).toInt(), -1), ii.value(q.value(1).toInt(), -1), q.value(2).toDouble()});
        q.finish();

        for (const Raw& rw : raws) {
            if (rw.e < 0 || rw.i < 0) continue;
            double conv = 0;
            if (!convert(rw.v, inds[rw.i].cur, currencyId, p.end, conv, res.error))
                return res;
            sum[rw.e][rw.i] += conv;
            cnt[rw.e][rw.i] += 1;
        }
    }

    // средние значения за интервал + min/max по каждому показателю
    QVector<QVector<double>> avg(E, QVector<double>(I, 0.0));
    QVector<double> mn(I, 0.0), mx(I, 0.0);
    QVector<bool> any(I, false);
    for (int e = 0; e < E; ++e)
        for (int i = 0; i < I; ++i) {
            if (cnt[e][i] == 0) continue;
            avg[e][i] = sum[e][i] / cnt[e][i];
            if (!any[i]) { mn[i] = mx[i] = avg[e][i]; any[i] = true; }
            else { mn[i] = std::min(mn[i], avg[e][i]); mx[i] = std::max(mx[i], avg[e][i]); }
        }

    double wTotal = 0;
    for (int i = 0; i < I; ++i)
        if (any[i]) wTotal += inds[i].weight;
    if (wTotal <= 0) {
        res.error = "За выбранный период данных нет";
        return res;
    }

    for (int e = 0; e < E; ++e) {
        bool has = false;
        double s = 0;
        for (int i = 0; i < I; ++i) {
            if (cnt[e][i] == 0) continue;
            has = true;
            const double norm = (mx[i] > mn[i]) ? (avg[e][i] - mn[i]) / (mx[i] - mn[i]) : 1.0;
            s += inds[i].weight * norm;
        }
        if (has)
            res.rows.append(RatingRow{ents[e].name, s / wTotal * 100.0, 0});
    }
    std::sort(res.rows.begin(), res.rows.end(),
              [](const RatingRow& a, const RatingRow& b) { return a.score > b.score; });
    for (int k = 0; k < res.rows.size(); ++k)
        res.rows[k].rank = k + 1;

    res.ok = true;
    return res;
}

QString dynamicsReport(const DynResult& r)
{
    QString s;
    s += "ОТЧЁТ «ДИНАМИКА ПОКАЗАТЕЛЕЙ ПРЕДПРИЯТИЯ»\n";
    s += "Предприятие: " + r.enterprise + "\n";
    s += "Периоды: " + r.periods.first() + " — " + r.periods.last() + "\n";
    s += "Валюта приведения: " + r.currency + "\n";
    s += "Сформирован: " + QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm") + "\n\n";

    QString head = pad("Показатель", 26) + pad("Ед.", 10);
    for (const QString& p : r.periods)
        head += padR(p, 14);
    head += padR("Изменение", 14) + padR("Изм., %", 10);
    s += head + "\n" + QString(head.size(), '-') + "\n";

    for (const DynRow& row : r.rows) {
        QString line = pad(row.name, 26) + pad(row.unit, 10);
        for (int k = 0; k < row.values.size(); ++k)
            line += padR(row.has[k] ? num(row.values[k]) : "—", 14);
        line += padR(row.changeValid ? num(row.absChange) : "—", 14);
        line += padR(row.pctValid ? num(row.pctChange) + "%" : "—", 10);
        s += line + "\n";
    }
    return s;
}

QString ratingReport(const RatingResult& r)
{
    QString s;
    s += "ОТЧЁТ «ИНТЕГРАЛЬНЫЙ РЕЙТИНГ ПРЕДПРИЯТИЙ ХОЛДИНГА»\n";
    s += "Периоды: " + r.periods.first() + " — " + r.periods.last() + "\n";
    s += "Валюта приведения: " + r.currency + "\n";
    s += "Сформирован: " + QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm") + "\n";
    s += "Методика: среднее значение показателя за интервал, min-max нормировка по предприятиям,\n"
         "          взвешенная сумма по весам показателей, шкала 0–100.\n\n";
    s += pad("Место", 8) + pad("Предприятие", 40) + padR("Рейтинг", 10) + "\n";
    s += QString(58, '-') + "\n";
    for (const RatingRow& row : r.rows)
        s += pad(QString::number(row.rank), 8) + pad(row.enterprise, 40) + padR(num(row.score), 10) + "\n";
    return s;
}

bool summaryReport(int periodFrom, int periodTo, int currencyId, QString& text, QString& error)
{
    QList<int> ids;
    QSqlQuery q = db().query("SELECT id FROM Enterprises ORDER BY name");
    while (q.next())
        ids.append(q.value(0).toInt());
    q.finish();
    if (ids.isEmpty()) {
        error = "В системе нет предприятий";
        return false;
    }

    RatingResult rt = rating(periodFrom, periodTo, currencyId);
    if (!rt.ok) {
        error = rt.error;
        return false;
    }
    text = "СВОДНЫЙ ОТЧЁТ ПО ХОЛДИНГУ\n" + QString(60, '=') + "\n\n" + ratingReport(rt);

    for (int id : ids) {
        DynResult d = dynamics(id, periodFrom, periodTo, currencyId);
        text += "\n" + QString(60, '=') + "\n";
        if (d.ok)
            text += dynamicsReport(d);
        else
            text += "Предприятие: " + db().scalar("SELECT name FROM Enterprises WHERE id = ?", {id}).toString()
                    + "\nДанные не получены: " + d.error + "\n";
    }
    return true;
}

} // namespace Analytics
