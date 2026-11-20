#pragma once
#include <QMainWindow>
#include "dataservice.h"

class QTabWidget;

/* Главное окно роли: шапка с именем пользователя, кнопка «Выйти», вкладки */
class RoleWindow : public QMainWindow
{
    Q_OBJECT
public:
    RoleWindow(const User& user, QWidget* parent = nullptr);
    QTabWidget* tabs() const { return m_tabs; }

signals:
    void loggedOut();

private:
    QTabWidget* m_tabs;
};

/* Создаёт окно администратора или аналитика в зависимости от роли */
RoleWindow* createRoleWindow(const User& user);
