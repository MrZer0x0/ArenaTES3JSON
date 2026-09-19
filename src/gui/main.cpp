#include "mainwindow.h"
#include <QApplication>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ArenaTES3JSON"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    arena::tes3json::MainWindow window;
    window.show();
    return app.exec();
}
