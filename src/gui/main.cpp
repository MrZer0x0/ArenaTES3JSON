#include "mainwindow.h"
#include <QApplication>
#include <QIcon>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ArenaTES3JSON"));
    QApplication::setApplicationVersion(QStringLiteral("0.4.0"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/ArenaTES3JSON.png")));
    arena::tes3json::MainWindow window;
    window.show();
    return app.exec();
}
