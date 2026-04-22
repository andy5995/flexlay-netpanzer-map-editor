#include <QApplication>
#include "mainwindow.h"

int main(int argc, char* argv[])
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc, argv);
    app.setApplicationName("netPanzer Map Editor");
    app.setOrganizationName("netpanzer");
    app.setStyleSheet(R"(
QScrollBar:vertical {
    background: #1e1e1e;
    width: 14px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background: #707070;
    min-height: 24px;
    border-radius: 4px;
    margin: 2px 2px;
}
QScrollBar::handle:vertical:hover {
    background: #a0a0a0;
}
QScrollBar::handle:vertical:pressed {
    background: #c8c820;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0;
}
QScrollBar:horizontal {
    background: #1e1e1e;
    height: 14px;
    margin: 0;
}
QScrollBar::handle:horizontal {
    background: #707070;
    min-width: 24px;
    border-radius: 4px;
    margin: 2px 2px;
}
QScrollBar::handle:horizontal:hover {
    background: #a0a0a0;
}
QScrollBar::handle:horizontal:pressed {
    background: #c8c820;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    width: 0;
}
    )");

    MainWindow window;
    window.show();
    return app.exec();
}
