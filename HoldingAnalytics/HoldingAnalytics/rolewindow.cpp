#include "rolewindow.h"
#include "analysttabs.h"
#include "crudtab.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

RoleWindow::RoleWindow(const User& u, QWidget* parent) : QMainWindow(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QString("Анализ показателей холдинга — %1").arg(u.role == "admin" ? "администратор" : "аналитик"));
    resize(1100, 680);

    auto* central = new QWidget;
    auto* lay = new QVBoxLayout(central);
    auto* header = new QHBoxLayout;
    header->addWidget(new QLabel(QString("Пользователь: <b>%1</b> (%2)").arg(u.fio, u.login)));
    header->addStretch();
    auto* out = new QPushButton("Выйти");
    header->addWidget(out);
    lay->addLayout(header);

    m_tabs = new QTabWidget;
    lay->addWidget(m_tabs, 1);
    setCentralWidget(central);

    connect(out, &QPushButton::clicked, this, [this] {
        emit loggedOut();
        close();
    });
}

namespace {

Action deleteAction(const QString& text, const QString& confirm, std::function<Result(int)> fn)
{
    Action a;
    a.text = text;
    a.needsSelection = true;
    a.confirmText = confirm;
    a.handler = [fn](int id, const QVariantMap&) { return fn(id); };
    return a;
}

void setupAdmin(RoleWindow* w, const User& user)
{
    using namespace DataService;

    /* ---- Предприятия ---- */
    {
        auto* t = new CrudTab("SELECT id, name, requisites, phone, contact_person FROM Enterprises ORDER BY name",
                              {"Название", "Реквизиты", "Телефон", "Контактное лицо"});
        const QList<Field> f = {textField("name", "Название"), textField("requisites", "Реквизиты"),
                                textField("phone", "Телефон"), textField("contact_person", "Контактное лицо")};
        t->addAction({"Добавить", f, false, "", "", [](int, const QVariantMap& v) { return addEnterprise(v); }});
        t->addAction({"Редактировать", f, true, "",
                      "SELECT name, requisites, phone, contact_person FROM Enterprises WHERE id = ?",
                      [](int id, const QVariantMap& v) { return updateEnterprise(id, v); }});
        t->addAction(deleteAction("Удалить", "Удалить выбранное предприятие?", [](int id) { return deleteEnterprise(id); }));
        w->tabs()->addTab(t, "Предприятия");
    }

    /* ---- Показатели ---- */
    {
        auto* t = new CrudTab("SELECT i.id, i.name, i.weight, i.unit, ISNULL(c.code, N'—') "
                              "FROM Indicators i LEFT JOIN Currencies c ON c.id = i.currency_id ORDER BY i.name",
                              {"Название", "Вес", "Ед. измерения", "Валюта"});
        const QList<Field> f = {textField("name", "Название"), textField("weight", "Вес (число > 0)"),
                                textField("unit", "Единица измерения"),
                                comboField("currency_id", "Валюта", [] { return currencyOptions(true); })};
        t->addAction({"Добавить", f, false, "", "", [](int, const QVariantMap& v) { return addIndicator(v); }});
        t->addAction({"Редактировать", f, true, "",
                      "SELECT name, weight, unit, ISNULL(currency_id, 0) AS currency_id FROM Indicators WHERE id = ?",
                      [](int id, const QVariantMap& v) { return updateIndicator(id, v); }});
        t->addAction(deleteAction("Удалить", "Удалить выбранный показатель?", [](int id) { return deleteIndicator(id); }));
        w->tabs()->addTab(t, "Показатели и веса");
    }

    /* ---- Валюты ---- */
    {
        auto* t = new CrudTab("SELECT id, code, name FROM Currencies ORDER BY code", {"Код", "Название"});
        t->addAction({"Добавить", {textField("code", "Код (3 лат. буквы)"), textField("name", "Название")},
                      false, "", "", [](int, const QVariantMap& v) { return addCurrency(v); }});
        t->addAction({"Редактировать", {textField("name", "Название")}, true, "",
                      "SELECT name FROM Currencies WHERE id = ?",
                      [](int id, const QVariantMap& v) { return updateCurrency(id, v); }});
        t->addAction(deleteAction("Удалить", "Удалить выбранную валюту?", [](int id) { return deleteCurrency(id); }));
        w->tabs()->addTab(t, "Валюты");
    }

    /* ---- Курсы валют ---- */
    {
        auto* t = new CrudTab("SELECT r.id, r.rate_date, cf.code, ct.code, r.rate FROM ExchangeRates r "
                              "JOIN Currencies cf ON cf.id = r.from_currency_id "
                              "JOIN Currencies ct ON ct.id = r.to_currency_id "
                              "ORDER BY r.rate_date DESC, cf.code, ct.code",
                              {"Дата", "Из валюты", "В валюту", "Курс"});
        t->addAction({"Добавить курс",
                      {dateField("date", "Дата"),
                       comboField("from", "Из валюты", [] { return currencyOptions(); }),
                       comboField("to", "В валюту", [] { return currencyOptions(); }),
                       textField("rate", "Курс (число > 0)")},
                      false, "", "", [](int, const QVariantMap& v) { return addRate(v); }});
        t->addAction(deleteAction("Удалить", "Удалить выбранный курс?", [](int id) { return deleteRate(id); }));
        w->tabs()->addTab(t, "Курсы валют");
    }

    /* ---- Отчётные периоды ---- */
    {
        auto* t = new CrudTab("SELECT id, name, start_date, end_date FROM Periods ORDER BY start_date",
                              {"Название", "Дата начала", "Дата окончания"});
        t->addAction({"Добавить",
                      {textField("name", "Название"), dateField("start", "Дата начала"), dateField("end", "Дата окончания")},
                      false, "", "", [](int, const QVariantMap& v) { return addPeriod(v); }});
        t->addAction(deleteAction("Удалить", "Удалить выбранный период?", [](int id) { return deletePeriod(id); }));
        w->tabs()->addTab(t, "Отчётные периоды");
    }

    /* ---- Пользователи ---- */
    {
        auto* t = new CrudTab("SELECT id, login, fio, CASE role WHEN 'admin' THEN N'Администратор' ELSE N'Аналитик' END "
                              "FROM Users ORDER BY login",
                              {"Логин", "ФИО", "Роль"});
        t->enableFilter("Роль:", roleOptions(), 2);
        t->addAction({"Добавить",
                      {textField("login", "Логин"), textField("fio", "ФИО"),
                       passwordField("password", "Пароль"), passwordField("password2", "Повтор пароля"),
                       comboField("role", "Роль", [] { return roleOptions(); })},
                      false, "", "", [](int, const QVariantMap& v) { return addUser(v); }});
        t->addAction({"Сменить пароль",
                      {passwordField("password", "Новый пароль"), passwordField("password2", "Повтор пароля")},
                      true, "", "", [](int id, const QVariantMap& v) { return changePassword(id, v); }});
        const int me = user.id;
        t->addAction(deleteAction("Удалить", "Удалить выбранного пользователя?", [me](int id) { return deleteUser(id, me); }));
        w->tabs()->addTab(t, "Пользователи");
    }
}

void setupAnalyst(RoleWindow* w)
{
    auto* ent = new CrudTab("SELECT id, name, requisites, phone, contact_person FROM Enterprises ORDER BY name",
                            {"Название", "Реквизиты", "Телефон", "Контактное лицо"});
    ent->enableSearch(0, "Предприятия не найдены");
    w->tabs()->addTab(ent, "Предприятия");
    w->tabs()->addTab(new ValuesTab, "Ввод значений показателей");
    w->tabs()->addTab(new RatesViewTab, "Курсы валют");
    w->tabs()->addTab(new DynamicsTab, "Динамика показателей");
    w->tabs()->addTab(new RatingTab, "Рейтинг предприятий");
    w->tabs()->addTab(new ReportTab, "Отчёты");
}

} // namespace

RoleWindow* createRoleWindow(const User& user)
{
    auto* w = new RoleWindow(user);
    if (user.role == "admin")
        setupAdmin(w, user);
    else
        setupAnalyst(w);
    return w;
}
