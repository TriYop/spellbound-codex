#include "MainWindow.h"

#include <QApplication>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("MasterTweak");
    app.setApplicationVersion("0.1.0");

    gui::MainWindow window;
    window.show();

    return app.exec();
}
