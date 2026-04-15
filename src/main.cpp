#include <QApplication>
#include "mainwindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("netPanzer Map Editor");
    app.setOrganizationName("netpanzer");

    MainWindow window;
    window.show();
    return app.exec();
}
