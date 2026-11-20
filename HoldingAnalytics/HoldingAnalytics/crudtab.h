#pragma once
#include <QVariantMap>
#include <QWidget>
#include <functional>
#include "dataservice.h"

class QComboBox;
class QHBoxLayout;
class QLineEdit;
class QTableWidget;

/* Описание поля формы диалога */
struct Field
{
    enum Type { Text, Password, Combo, Date };
    QString key;
    QString label;
    Type type = Text;
    std::function<QList<Option>()> options;   // для Combo
};

Field textField(const QString& key, const QString& label);
Field passwordField(const QString& key, const QString& label);
Field dateField(const QString& key, const QString& label);
Field comboField(const QString& key, const QString& label, std::function<QList<Option>()> options);

/* Действие-кнопка: диалог с полями (или без) + обработчик */
struct Action
{
    QString text;
    QList<Field> fields;
    bool needsSelection = false;
    QString confirmText;     // не пусто – спросить подтверждение (для удаления)
    QString prefillSql;      // SELECT ... WHERE id = ?  (псевдонимы колонок = key полей)
    std::function<Result(int, const QVariantMap&)> handler;
};

/* Универсальная вкладка «таблица + кнопки CRUD».
   Первая колонка запроса – всегда id (скрыта). */
class CrudTab : public QWidget
{
    Q_OBJECT
public:
    CrudTab(const QString& selectSql, const QStringList& headers, QWidget* parent = nullptr);

    void addAction(const Action& action);
    void enableSearch(int column, const QString& emptyMessage);
    void enableFilter(const QString& label, const QList<Option>& options, int column);
    void refresh();

protected:
    void showEvent(QShowEvent* e) override;

private:
    void runAction(const Action& a);
    void applyFilters(bool notify = false);
    int selectedId() const;

    QString m_sql;
    QTableWidget* m_table = nullptr;
    QHBoxLayout* m_topBar = nullptr;
    QHBoxLayout* m_buttons = nullptr;
    QLineEdit* m_search = nullptr;
    QComboBox* m_filter = nullptr;
    int m_searchCol = -1;
    int m_filterCol = -1;
    QString m_emptyMessage;
};
