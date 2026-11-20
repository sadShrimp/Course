#pragma once
#include <QWidget>

class QLineEdit;

class LoginWindow : public QWidget
{
    Q_OBJECT
public:
    explicit LoginWindow(QWidget* parent = nullptr);

private slots:
    void onLogin();
    void onLoggedOut();

private:
    QLineEdit* m_login;
    QLineEdit* m_pass;
};
