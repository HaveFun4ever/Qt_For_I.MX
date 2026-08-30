#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 设置应用属性
    app.setStyle("Fusion");
    app.setApplicationName("MP3 Controller");
    app.setOrganizationName("AudioPlayer");

    // 设置全局字体
    QFont font("Segoe UI", 9);
    font.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(font);

    MainWindow window;
    window.show();

    return app.exec();
}
