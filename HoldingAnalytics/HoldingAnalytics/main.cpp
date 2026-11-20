#include <QApplication>
#include <QCoreApplication>
#include <QMessageBox>

#include "database.h"
#include "loginwindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("HoldingAnalytics");

    QString error;
    if (!Database::instance().open(error)) {
        QMessageBox::critical(nullptr, "Нет подключения к базе данных",
            "Не удалось подключиться к MS SQL Server:\n\n" + error +
            "\n\nПроверьте настройки в файле:\n" + QCoreApplication::applicationDirPath() + "/config.ini"
            "\nи наличие ODBC-драйвера SQL Server.");
        return 1;
    }

    LoginWindow w;
    w.show();
    return app.exec();
}
