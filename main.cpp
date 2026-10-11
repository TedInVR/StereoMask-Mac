#include <QApplication>
#include <QTimer>
#ifdef Q_OS_MACOS
void activateMacApplication();
#endif
#include <QLocale>
#include <QImageReader>
#include <QDebug>
#include <QLibraryInfo>
#include "mainwindow.h"
#include "appversion.h"

int main(int argc, char *argv[])
{
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    QApplication a(argc, argv);
    a.setApplicationVersion(STEREOMASK_VERSION);

    qDebug() << "--- StereoMask Diagnostics ---";
    qDebug() << "Library Path:" << QCoreApplication::libraryPaths();
    qDebug() << "Supported Formats:" << QImageReader::supportedImageFormats();
    qDebug() << "------------------------------";

    MainWindow w;
    w.setWindowTitle("StereoMask Mac R15 (2.1.5)");
    w.showMaximized();
    QTimer::singleShot(0,&w,[&w] {
#ifdef Q_OS_MACOS
        activateMacApplication();
#endif
        w.raise();w.activateWindow();QApplication::setActiveWindow(&w);
    });
    return a.exec();
}
