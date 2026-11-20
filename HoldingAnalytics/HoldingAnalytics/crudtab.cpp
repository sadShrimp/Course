#include "crudtab.h"
#include "database.h"
#include <QComboBox>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMap>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlQuery>
#include <QTableWidget>
#include <QVBoxLayout>

Field textField(const QString& key, const QString& label)     { return Field{key, label, Field::Text, nullptr}; }
Field passwordField(const QString& key, const QString& label) { return Field{key, label, Field::Password, nullptr}; }
Field dateField(const QString& key, const QString& label)     { return Field{key, label, Field::Date, nullptr}; }
Field comboField(const QString& key, const QString& label, std::function<QList<Option>()> options)
{
    return Field{key, label, Field::Combo, std::move(options)};
}

CrudTab::CrudTab(const QString& selectSql, const QStringList& headers, QWidget* parent)
    : QWidget(parent), m_sql(selectSql)
{
    auto* lay = new QVBoxLayout(this);

    m_topBar = new QHBoxLayout;
    lay->addLayout(m_topBar);

    m_table = new QTableWidget(0, headers.size() + 1, this);
    QStringList h;
    h << "ID" << headers;
    m_table->setHorizontalHeaderLabels(h);
    m_table->setColumnHidden(0, true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);   // прямое редактирование запрещено
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    lay->addWidget(m_table, 1);

    m_buttons = new QHBoxLayout;
    m_buttons->addStretch();
    lay->addLayout(m_buttons);
}

void CrudTab::addAction(const Action& action)
{
    auto* btn = new QPushButton(action.text, this);
    m_buttons->insertWidget(m_buttons->count() - 1, btn);
    connect(btn, &QPushButton::clicked, this, [this, action] { runAction(action); });
}

void CrudTab::enableSearch(int column, const QString& emptyMessage)
{
    m_searchCol = column + 1;
    m_emptyMessage = emptyMessage;
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText("Часть названия…");
    auto* btn = new QPushButton("Найти", this);
    auto* reset = new QPushButton("Сбросить", this);
    m_topBar->addWidget(new QLabel("Поиск:"));
    m_topBar->addWidget(m_search, 1);
    m_topBar->addWidget(btn);
    m_topBar->addWidget(reset);
    connect(btn, &QPushButton::clicked, this, [this] { applyFilters(true); });
    connect(m_search, &QLineEdit::returnPressed, this, [this] { applyFilters(true); });
    connect(reset, &QPushButton::clicked, this, [this] { m_search->clear(); applyFilters(); });
}

void CrudTab::enableFilter(const QString& label, const QList<Option>& options, int column)
{
    m_filterCol = column + 1;
    m_filter = new QComboBox(this);
    m_filter->addItem("Все", QString());
    for (const Option& o : options)
        m_filter->addItem(o.first, o.first);
    m_topBar->addWidget(new QLabel(label));
    m_topBar->addWidget(m_filter);
    m_topBar->addStretch();
    connect(m_filter, &QComboBox::currentIndexChanged, this, [this] { applyFilters(); });
}

void CrudTab::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    refresh();
}

void CrudTab::refresh()
{
    QString err;
    QSqlQuery q = Database::instance().query(m_sql, {}, &err);
    if (!err.isEmpty()) {
        QMessageBox::critical(this, "Ошибка базы данных", err);
        return;
    }
    m_table->setRowCount(0);
    const int cols = m_table->columnCount();
    int row = 0;
    while (q.next()) {
        m_table->insertRow(row);
        for (int c = 0; c < cols; ++c) {
            const QVariant v = q.value(c);
            QString s;
            if (v.metaType().id() == QMetaType::QDate)
                s = v.toDate().toString("dd.MM.yyyy");
            else if (v.metaType().id() == QMetaType::Double)
                s = QString::number(v.toDouble(), 'g', 10);
            else
                s = v.toString();
            m_table->setItem(row, c, new QTableWidgetItem(s));
        }
        ++row;
    }
    applyFilters();
}

void CrudTab::applyFilters(bool notify)
{
    const QString text = m_search ? m_search->text().trimmed() : QString();
    const QString role = m_filter ? m_filter->currentData().toString() : QString();
    int visible = 0;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        bool show = true;
        if (m_searchCol >= 0 && !text.isEmpty())
            show = m_table->item(r, m_searchCol)->text().contains(text, Qt::CaseInsensitive);
        if (show && m_filterCol >= 0 && !role.isEmpty())
            show = m_table->item(r, m_filterCol)->text() == role;
        m_table->setRowHidden(r, !show);
        if (show) ++visible;
    }
    if (notify && visible == 0 && !m_emptyMessage.isEmpty())
        QMessageBox::information(this, "Поиск", m_emptyMessage);
}

int CrudTab::selectedId() const
{
    const int row = m_table->currentRow();
    if (row < 0 || m_table->isRowHidden(row))
        return 0;
    return m_table->item(row, 0)->text().toInt();
}

void CrudTab::runAction(const Action& a)
{
    int id = 0;
    if (a.needsSelection) {
        id = selectedId();
        if (id <= 0) {
            QMessageBox::information(this, "Выбор записи", "Сначала выберите строку в таблице");
            return;
        }
    }

    // действие без формы (удаление)
    if (a.fields.isEmpty()) {
        if (!a.confirmText.isEmpty() &&
            QMessageBox::question(this, "Подтверждение", a.confirmText) != QMessageBox::Yes)
            return;
        const Result r = a.handler(id, QVariantMap());
        if (!r.ok)
            QMessageBox::warning(this, "Операция невозможна", r.message);
        else
            refresh();
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(a.text);
    dlg.setMinimumWidth(400);
    auto* form = new QFormLayout;
    auto* root = new QVBoxLayout(&dlg);
    root->addLayout(form);

    QMap<QString, QWidget*> editors;
    for (const Field& f : a.fields) {
        QWidget* w = nullptr;
        switch (f.type) {
        case Field::Text:
            w = new QLineEdit(&dlg);
            break;
        case Field::Password: {
            auto* e = new QLineEdit(&dlg);
            e->setEchoMode(QLineEdit::Password);
            w = e;
            break;
        }
        case Field::Combo: {
            auto* c = new QComboBox(&dlg);
            if (f.options)
                for (const Option& o : f.options())
                    c->addItem(o.first, o.second);
            w = c;
            break;
        }
        case Field::Date: {
            auto* d = new QDateEdit(QDate::currentDate(), &dlg);
            d->setCalendarPopup(true);
            d->setDisplayFormat("dd.MM.yyyy");
            w = d;
            break;
        }
        }
        form->addRow(f.label + ":", w);
        editors[f.key] = w;
    }

    // заполнение текущими значениями (редактирование)
    if (!a.prefillSql.isEmpty()) {
        QSqlQuery q = Database::instance().query(a.prefillSql, {id});
        if (q.next()) {
            for (const Field& f : a.fields) {
                const QVariant v = q.value(f.key);
                QWidget* w = editors[f.key];
                if (auto* le = qobject_cast<QLineEdit*>(w))        le->setText(v.toString());
                else if (auto* cb = qobject_cast<QComboBox*>(w))   cb->setCurrentIndex(qMax(0, cb->findData(v)));
                else if (auto* de = qobject_cast<QDateEdit*>(w))   de->setDate(Database::toDate(v));
            }
        }
        q.finish();
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    buttons->button(QDialogButtonBox::Ok)->setText(a.text);
    buttons->button(QDialogButtonBox::Cancel)->setText("Отмена");
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    while (dlg.exec() == QDialog::Accepted) {
        QVariantMap m;
        for (const Field& f : a.fields) {
            QWidget* w = editors[f.key];
            if (auto* le = qobject_cast<QLineEdit*>(w))        m[f.key] = le->text();
            else if (auto* cb = qobject_cast<QComboBox*>(w))   m[f.key] = cb->currentData();
            else if (auto* de = qobject_cast<QDateEdit*>(w))   m[f.key] = de->date();
        }
        const Result r = a.handler(id, m);
        if (r.ok) {
            refresh();
            return;
        }
        QMessageBox::warning(&dlg, "Некорректные данные", r.message);   // окно остаётся открытым
    }
}
