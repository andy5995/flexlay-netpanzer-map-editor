#include <QApplication>
#include <QFile>
#include "mainwindow.h"

int main(int argc, char* argv[])
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc, argv);
    app.setApplicationName("netPanzer Map Editor");
    app.setOrganizationName("netpanzer");

    QFile qss(":/style.qss");
    if (qss.open(QFile::ReadOnly))
        app.setStyleSheet(qss.readAll());

    MainWindow window;
    window.show();
    return app.exec();
}
