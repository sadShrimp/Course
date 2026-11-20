#pragma once
#include <QDate>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

struct DynRow
{
    QString name;
    QString unit;
    QVector<double> values;
    QVector<bool> has;
    bool changeValid = false;
    bool pctValid = false;
    double absChange = 0;
    double pctChange = 0;
};

struct DynResult
{
    bool ok = false;
    QString error;
    QString enterprise;
    QString currency;
    QStringList periods;
    QVector<DynRow> rows;
};

struct RatingRow
{
    QString enterprise;
    double score = 0;
    int rank = 0;
};

struct RatingResult
{
    bool ok = false;
    QString error;
    QString currency;
    QStringList periods;
    QVector<RatingRow> rows;
};

namespace Analytics
{
/* Курс from -> to на дату: прямой, обратный или через промежуточную валюту. */
std::optional<double> findRate(int fromCurrency, int toCurrency, const QDate& date);

DynResult dynamics(int enterpriseId, int periodFrom, int periodTo, int currencyId);
RatingResult rating(int periodFrom, int periodTo, int currencyId);

QString dynamicsReport(const DynResult& r);
QString ratingReport(const RatingResult& r);
bool summaryReport(int periodFrom, int periodTo, int currencyId, QString& text, QString& error);
}
