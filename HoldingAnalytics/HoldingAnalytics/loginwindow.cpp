#include "loginwindow.h"
#include "dataservice.h"
#include "rolewindow.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

LoginWindow::LoginWindow(QWidget* parent) : QWidget(parent)
{
    setWindowTitle("Авторизация");
    setFixedSize(380, 220);

    auto* lay = new QVBoxLayout(this);
    auto* title = new QLabel("<h3>Анализ показателей финансовой отчётности холдинга</h3>");
    title->setWordWrap(true);
    title->setAlignment(Qt::AlignCenter);
    lay->addWidget(title);

    auto* form = new QFormLayout;
    m_login = new QLineEdit;
    m_pass = new QLineEdit;
    m_pass->setEchoMode(QLineEdit::Password);
    form->addRow("Логин:", m_login);
    form->addRow("Пароль:", m_pass);
    lay->addLayout(form);

    auto* btn = new QPushButton("Войти");
    btn->setDefault(true);
    lay->addWidget(btn);

    connect(btn, &QPushButton::clicked, this, &LoginWindow::onLogin);
    connect(m_pass, &QLineEdit::returnPressed, this, &LoginWindow::onLogin);
    connect(m_login, &QLineEdit::returnPressed, this, &LoginWindow::onLogin);
}

void LoginWindow::onLogin()
{
    const QString login = m_login->text().trimmed();
    const QString pass = m_pass->text();
    if (login.isEmpty() || pass.isEmpty()) {
        QMessageBox::warning(this, "Ошибка авторизации", "Введите логин и пароль");
        return;
    }

    User user;
    if (!DataService::authenticate(login, pass, user)) {
        QMessageBox::warning(this, "Ошибка авторизации", "Неверный логин или пароль");
        return;
    }

    RoleWindow* w = createRoleWindow(user);
    connect(w, &RoleWindow::loggedOut, this, &LoginWindow::onLoggedOut);
    hide();
    w->show();
}

void LoginWindow::onLoggedOut()
{
    m_login->clear();
    m_pass->clear();
    show();
}
