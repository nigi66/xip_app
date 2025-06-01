#include "mainwindow.h"

#include <QApplication>

#include <QPalette>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(100, 100, 100));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Highlight, QColor(142, 45, 197).lighter());
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);

    app.setPalette(darkPalette);
    app.setStyle("Fusion");  // Optional: consistent dark style

    MainWindow window;
    window.show();

    return app.exec();
}
