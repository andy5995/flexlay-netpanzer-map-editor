#include <QApplication>
#include "mainwindow.h"

int main(int argc, char* argv[])
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc, argv);
    app.setApplicationName("netPanzer Map Editor");
    app.setOrganizationName("netpanzer");

    MainWindow window;
    window.show();
    return app.exec();
}
