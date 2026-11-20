#pragma once
#include <QWidget>
#include "dataservice.h"

class QComboBox;
class QDateEdit;
class QPlainTextEdit;
class QTableWidget;

void fillCombo(QComboBox* combo, const QList<Option>& options);

/* Ввод значений показателей (аналитик) */
class ValuesTab : public QWidget
{
    Q_OBJECT
public:
    explicit ValuesTab(QWidget* parent = nullptr);
protected:
    void showEvent(QShowEvent* e) override;
private:
    void loadValues();
    void save();
    QComboBox* m_ent;
    QComboBox* m_per;
    QTableWidget* m_table;
    bool m_first = true;
};

/* Просмотр курсов валют (аналитик) */
class RatesViewTab : public QWidget
{
    Q_OBJECT
public:
    explicit RatesViewTab(QWidget* parent = nullptr);
protected:
    void showEvent(QShowEvent* e) override;
private:
    void apply();
    QComboBox* m_from;
    QComboBox* m_to;
    QDateEdit* m_d1;
    QDateEdit* m_d2;
    QTableWidget* m_table;
    bool m_first = true;
};

/* Расчёт динамики показателей */
class DynamicsTab : public QWidget
{
    Q_OBJECT
public:
    explicit DynamicsTab(QWidget* parent = nullptr);
protected:
    void showEvent(QShowEvent* e) override;
private:
    void calculate();
    QComboBox* m_ent;
    QComboBox* m_p1;
    QComboBox* m_p2;
    QComboBox* m_cur;
    QTableWidget* m_table;
    bool m_first = true;
};

/* Интегральный рейтинг предприятий */
class RatingTab : public QWidget
{
    Q_OBJECT
public:
    explicit RatingTab(QWidget* parent = nullptr);
protected:
    void showEvent(QShowEvent* e) override;
private:
    void calculate();
    QComboBox* m_p1;
    QComboBox* m_p2;
    QComboBox* m_cur;
    QTableWidget* m_table;
    bool m_first = true;
};

/* Формирование отчёта и сохранение в файл */
class ReportTab : public QWidget
{
    Q_OBJECT
public:
    explicit ReportTab(QWidget* parent = nullptr);
protected:
    void showEvent(QShowEvent* e) override;
private:
    void generate();
    void saveToFile();
    QComboBox* m_type;
    QComboBox* m_ent;
    QComboBox* m_p1;
    QComboBox* m_p2;
    QComboBox* m_cur;
    QPlainTextEdit* m_text;
    bool m_first = true;
};
