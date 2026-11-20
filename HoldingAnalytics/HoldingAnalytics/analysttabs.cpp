#include "analysttabs.h"
#include "analytics.h"
#include "database.h"

#include <QComboBox>
#include <QDateEdit>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>
#include <QFile>

void fillCombo(QComboBox* c, const QList<Option>& opts)
{
    const QVariant cur = c->currentData();
    c->blockSignals(true);
    c->clear();
    for (const Option& o : opts)
        c->addItem(o.first, o.second);
    const int i = c->findData(cur);
    if (i >= 0)
        c->setCurrentIndex(i);
    c->blockSignals(false);
}

namespace {
QTableWidget* makeTable(const QStringList& headers)
{
    auto* t = new QTableWidget(0, headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setAlternatingRowColors(true);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    t->verticalHeader()->setVisible(false);
    return t;
}

QTableWidgetItem* cell(const QString& text, bool right = false)
{
    auto* it = new QTableWidgetItem(text);
    if (right) it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return it;
}

QString num(double v) { return QString::number(v, 'f', 2); }
}

/* ======================= ValuesTab ======================= */
ValuesTab::ValuesTab(QWidget* parent) : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    auto* top = new QHBoxLayout;
    m_ent = new QComboBox;
    m_per = new QComboBox;
    top->addWidget(new QLabel("Предприятие:"));
    top->addWidget(m_ent, 1);
    top->addWidget(new QLabel("Отчётный период:"));
    top->addWidget(m_per, 1);
    lay->addLayout(top);

    m_table = makeTable({"ID", "Показатель", "Ед. изм.", "Валюта", "Значение"});
    m_table->setColumnHidden(0, true);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                             QAbstractItemView::AnyKeyPressed);
    lay->addWidget(m_table, 1);

    auto* hint = new QLabel("Значение вводится двойным щелчком по ячейке. Пустое значение = показатель не задан.");
    lay->addWidget(hint);
    auto* btn = new QPushButton("Сохранить");
    lay->addWidget(btn, 0, Qt::AlignRight);

    connect(m_ent, &QComboBox::currentIndexChanged, this, [this] { loadValues(); });
    connect(m_per, &QComboBox::currentIndexChanged, this, [this] { loadValues(); });
    connect(btn, &QPushButton::clicked, this, [this] { save(); });
}

void ValuesTab::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    fillCombo(m_ent, DataService::enterpriseOptions());
    fillCombo(m_per, DataService::periodOptions());
    if (m_first) {
        m_first = false;
        loadValues();
    }
}

void ValuesTab::loadValues()
{
    m_table->setRowCount(0);
    const int e = m_ent->currentData().toInt();
    const int p = m_per->currentData().toInt();
    if (e <= 0 || p <= 0)
        return;

    QSqlQuery q = Database::instance().query(
        "SELECT i.id, i.name, i.unit, ISNULL(c.code, N'—'), v.value "
        "FROM Indicators i "
        "LEFT JOIN Currencies c ON c.id = i.currency_id "
        "LEFT JOIN IndicatorValues v ON v.indicator_id = i.id AND v.enterprise_id = ? AND v.period_id = ? "
        "ORDER BY i.name", {e, p});
    int row = 0;
    while (q.next()) {
        m_table->insertRow(row);
        for (int c = 0; c < 4; ++c) {
            auto* it = cell(q.value(c).toString());
            it->setFlags(it->flags() & ~Qt::ItemIsEditable);
            m_table->setItem(row, c, it);
        }
        const QVariant v = q.value(4);
        m_table->setItem(row, 4, cell(v.isNull() ? QString() : QString::number(v.toDouble(), 'g', 12), true));
        ++row;
    }
}

void ValuesTab::save()
{
    QVector<QPair<int, QString>> rows;
    for (int r = 0; r < m_table->rowCount(); ++r)
        rows.append(qMakePair(m_table->item(r, 0)->text().toInt(), m_table->item(r, 4)->text()));

    const Result res = DataService::saveValues(m_ent->currentData().toInt(), m_per->currentData().toInt(), rows);
    if (!res.ok)
        QMessageBox::warning(this, "Некорректные данные", res.message);
    else {
        QMessageBox::information(this, "Сохранение", "Значения показателей сохранены");
        loadValues();
    }
}

/* ======================= RatesViewTab ======================= */
RatesViewTab::RatesViewTab(QWidget* parent) : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    auto* top = new QHBoxLayout;
    m_from = new QComboBox;
    m_to = new QComboBox;
    m_d1 = new QDateEdit(QDate::currentDate().addYears(-1));
    m_d2 = new QDateEdit(QDate::currentDate());
    for (QDateEdit* d : {m_d1, m_d2}) {
        d->setCalendarPopup(true);
        d->setDisplayFormat("dd.MM.yyyy");
    }
    auto* btn = new QPushButton("Применить");
    top->addWidget(new QLabel("Из:"));   top->addWidget(m_from, 1);
    top->addWidget(new QLabel("В:"));    top->addWidget(m_to, 1);
    top->addWidget(new QLabel("с:"));    top->addWidget(m_d1);
    top->addWidget(new QLabel("по:"));   top->addWidget(m_d2);
    top->addWidget(btn);
    lay->addLayout(top);

    m_table = makeTable({"Дата", "Из валюты", "В валюту", "Курс"});
    lay->addWidget(m_table, 1);
    connect(btn, &QPushButton::clicked, this, [this] { apply(); });
}

void RatesViewTab::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    fillCombo(m_from, DataService::currencyOptions());
    fillCombo(m_to, DataService::currencyOptions());
    if (m_first) {
        m_first = false;
        if (m_to->count() > 1) m_to->setCurrentIndex(1);
        const QDate mn = Database::toDate(Database::instance().scalar("SELECT MIN(rate_date) FROM ExchangeRates"));
        if (mn.isValid()) m_d1->setDate(mn);
    }
}

void RatesViewTab::apply()
{
    m_table->setRowCount(0);
    if (m_d1->date() > m_d2->date()) {
        QMessageBox::warning(this, "Некорректный период", "Дата начала позже даты окончания");
        return;
    }
    const int f = m_from->currentData().toInt();
    const int t = m_to->currentData().toInt();
    if (f <= 0 || t <= 0 || f == t) {
        QMessageBox::warning(this, "Некорректные данные", "Выберите две разные валюты");
        return;
    }
    QSqlQuery q = Database::instance().query(
        "SELECT r.rate_date, cf.code, ct.code, r.rate FROM ExchangeRates r "
        "JOIN Currencies cf ON cf.id = r.from_currency_id "
        "JOIN Currencies ct ON ct.id = r.to_currency_id "
        "WHERE r.from_currency_id = ? AND r.to_currency_id = ? AND r.rate_date >= ? AND r.rate_date <= ? "
        "ORDER BY r.rate_date",
        {f, t, m_d1->date().toString(Qt::ISODate), m_d2->date().toString(Qt::ISODate)});
    int row = 0;
    while (q.next()) {
        m_table->insertRow(row);
        m_table->setItem(row, 0, cell(Database::toDate(q.value(0)).toString("dd.MM.yyyy")));
        m_table->setItem(row, 1, cell(q.value(1).toString().trimmed()));
        m_table->setItem(row, 2, cell(q.value(2).toString().trimmed()));
        m_table->setItem(row, 3, cell(QString::number(q.value(3).toDouble(), 'f', 4), true));
        ++row;
    }
    if (row == 0)
        QMessageBox::information(this, "Курсы валют", "За выбранный период курсов этой пары нет");
}

/* ======================= DynamicsTab ======================= */
DynamicsTab::DynamicsTab(QWidget* parent) : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    auto* top = new QHBoxLayout;
    m_ent = new QComboBox; m_p1 = new QComboBox; m_p2 = new QComboBox; m_cur = new QComboBox;
    auto* btn = new QPushButton("Рассчитать");
    top->addWidget(new QLabel("Предприятие:")); top->addWidget(m_ent, 2);
    top->addWidget(new QLabel("Период с:"));    top->addWidget(m_p1, 1);
    top->addWidget(new QLabel("по:"));          top->addWidget(m_p2, 1);
    top->addWidget(new QLabel("Валюта:"));      top->addWidget(m_cur, 1);
    top->addWidget(btn);
    lay->addLayout(top);

    m_table = makeTable({"Показатель", "Ед.", "Изменение", "Изм., %"});
    lay->addWidget(m_table, 1);
    connect(btn, &QPushButton::clicked, this, [this] { calculate(); });
}

void DynamicsTab::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    fillCombo(m_ent, DataService::enterpriseOptions());
    fillCombo(m_p1, DataService::periodOptions());
    fillCombo(m_p2, DataService::periodOptions());
    fillCombo(m_cur, DataService::currencyOptions());
    if (m_first) {
        m_first = false;
        if (m_p2->count() > 0) m_p2->setCurrentIndex(m_p2->count() - 1);
    }
}

void DynamicsTab::calculate()
{
    const DynResult r = Analytics::dynamics(m_ent->currentData().toInt(), m_p1->currentData().toInt(),
                                            m_p2->currentData().toInt(), m_cur->currentData().toInt());
    m_table->clear();
    m_table->setRowCount(0);
    if (!r.ok) {
        m_table->setColumnCount(0);
        QMessageBox::warning(this, "Расчёт невозможен", r.error);
        return;
    }

    QStringList headers{"Показатель", "Ед."};
    headers += r.periods;
    headers << "Изменение" << "Изм., %";
    m_table->setColumnCount(headers.size());
    m_table->setHorizontalHeaderLabels(headers);

    int row = 0;
    for (const DynRow& d : r.rows) {
        m_table->insertRow(row);
        m_table->setItem(row, 0, cell(d.name));
        m_table->setItem(row, 1, cell(d.unit));
        int col = 2;
        for (int k = 0; k < d.values.size(); ++k)
            m_table->setItem(row, col++, cell(d.has[k] ? num(d.values[k]) : "—", true));

        auto* abs = cell(d.changeValid ? num(d.absChange) : "—", true);
        auto* pct = cell(d.pctValid ? num(d.pctChange) + " %" : "—", true);
        if (d.changeValid) {
            const QColor c = d.absChange >= 0 ? QColor(0, 140, 0) : QColor(200, 0, 0);
            abs->setForeground(c);
            pct->setForeground(c);
        }
        m_table->setItem(row, col++, abs);
        m_table->setItem(row, col, pct);
        ++row;
    }
}

/* ======================= RatingTab ======================= */
RatingTab::RatingTab(QWidget* parent) : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    auto* top = new QHBoxLayout;
    m_p1 = new QComboBox; m_p2 = new QComboBox; m_cur = new QComboBox;
    auto* btn = new QPushButton("Рассчитать рейтинг");
    top->addWidget(new QLabel("Период с:")); top->addWidget(m_p1, 1);
    top->addWidget(new QLabel("по:"));       top->addWidget(m_p2, 1);
    top->addWidget(new QLabel("Валюта:"));   top->addWidget(m_cur, 1);
    top->addWidget(btn);
    lay->addLayout(top);

    m_table = makeTable({"Место", "Предприятие", "Интегральный рейтинг (0–100)"});
    lay->addWidget(m_table, 1);
    connect(btn, &QPushButton::clicked, this, [this] { calculate(); });
}

void RatingTab::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    fillCombo(m_p1, DataService::periodOptions());
    fillCombo(m_p2, DataService::periodOptions());
    fillCombo(m_cur, DataService::currencyOptions());
    if (m_first) {
        m_first = false;
        if (m_p2->count() > 0) m_p2->setCurrentIndex(m_p2->count() - 1);
    }
}

void RatingTab::calculate()
{
    const RatingResult r = Analytics::rating(m_p1->currentData().toInt(), m_p2->currentData().toInt(),
                                             m_cur->currentData().toInt());
    m_table->setRowCount(0);
    if (!r.ok) {
        QMessageBox::warning(this, "Расчёт невозможен", r.error);
        return;
    }
    int row = 0;
    for (const RatingRow& x : r.rows) {
        m_table->insertRow(row);
        m_table->setItem(row, 0, cell(QString::number(x.rank)));
        m_table->setItem(row, 1, cell(x.enterprise));
        m_table->setItem(row, 2, cell(num(x.score), true));
        ++row;
    }
}

/* ======================= ReportTab ======================= */
ReportTab::ReportTab(QWidget* parent) : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    auto* top = new QHBoxLayout;
    m_type = new QComboBox;
    m_type->addItem("Динамика показателей предприятия", 0);
    m_type->addItem("Интегральный рейтинг предприятий", 1);
    m_type->addItem("Сводный отчёт по холдингу", 2);
    m_ent = new QComboBox; m_p1 = new QComboBox; m_p2 = new QComboBox; m_cur = new QComboBox;
    top->addWidget(new QLabel("Тип отчёта:")); top->addWidget(m_type, 2);
    top->addWidget(new QLabel("Предприятие:")); top->addWidget(m_ent, 2);
    lay->addLayout(top);

    auto* top2 = new QHBoxLayout;
    top2->addWidget(new QLabel("Период с:")); top2->addWidget(m_p1, 1);
    top2->addWidget(new QLabel("по:"));       top2->addWidget(m_p2, 1);
    top2->addWidget(new QLabel("Валюта:"));   top2->addWidget(m_cur, 1);
    auto* gen = new QPushButton("Сформировать");
    auto* save = new QPushButton("Сохранить в файл");
    top2->addWidget(gen);
    top2->addWidget(save);
    lay->addLayout(top2);

    m_text = new QPlainTextEdit;
    m_text->setReadOnly(true);
    m_text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_text->setLineWrapMode(QPlainTextEdit::NoWrap);
    lay->addWidget(m_text, 1);

    connect(m_type, &QComboBox::currentIndexChanged, this, [this] { m_ent->setEnabled(m_type->currentData().toInt() == 0); });
    connect(gen, &QPushButton::clicked, this, [this] { generate(); });
    connect(save, &QPushButton::clicked, this, [this] { saveToFile(); });
}

void ReportTab::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    fillCombo(m_ent, DataService::enterpriseOptions());
    fillCombo(m_p1, DataService::periodOptions());
    fillCombo(m_p2, DataService::periodOptions());
    fillCombo(m_cur, DataService::currencyOptions());
    if (m_first) {
        m_first = false;
        if (m_p2->count() > 0) m_p2->setCurrentIndex(m_p2->count() - 1);
    }
}

void ReportTab::generate()
{
    const int type = m_type->currentData().toInt();
    const int p1 = m_p1->currentData().toInt();
    const int p2 = m_p2->currentData().toInt();
    const int cur = m_cur->currentData().toInt();

    if (p1 <= 0 || p2 <= 0 || cur <= 0 || (type == 0 && m_ent->currentData().toInt() <= 0)) {
        QMessageBox::warning(this, "Параметры отчёта", "Выберите все параметры отчёта");
        return;
    }

    QString text, error;
    if (type == 0) {
        const DynResult r = Analytics::dynamics(m_ent->currentData().toInt(), p1, p2, cur);
        if (r.ok) text = Analytics::dynamicsReport(r); else error = r.error;
    } else if (type == 1) {
        const RatingResult r = Analytics::rating(p1, p2, cur);
        if (r.ok) text = Analytics::ratingReport(r); else error = r.error;
    } else {
        Analytics::summaryReport(p1, p2, cur, text, error);
    }

    if (!error.isEmpty()) {
        m_text->clear();
        QMessageBox::warning(this, "Отчёт не сформирован", error);
        return;
    }
    m_text->setPlainText(text);
}

void ReportTab::saveToFile()
{
    if (m_text->toPlainText().isEmpty()) {
        QMessageBox::information(this, "Сохранение", "Сначала сформируйте отчёт");
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, "Сохранить отчёт", "report.txt", "Текстовые файлы (*.txt)");
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Ошибка", "Не удалось открыть файл для записи");
        return;
    }
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out.setGenerateByteOrderMark(true);
    out << m_text->toPlainText();
    QMessageBox::information(this, "Сохранение", "Отчёт сохранён в файл:\n" + path);
}
