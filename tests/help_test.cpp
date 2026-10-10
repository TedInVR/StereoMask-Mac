#include "mainwindow.h"
#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QFile>
#include <QMenu>
#include <QMenuBar>
#include <QStandardPaths>
#include <QUrl>
#include <iostream>

class BrowserCapture : public QObject {
    Q_OBJECT
public:
    QUrl url;
public slots:
    void open(const QUrl &value) { url = value; }
};

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    BrowserCapture browser;
    QDesktopServices::setUrlHandler("file", &browser, "open");
    MainWindow window;
    QAction *guide = nullptr;
    for (QAction *menuAction : window.menuBar()->actions()) {
        if (menuAction->text() == "&Help") {
            for (QAction *action : menuAction->menu()->actions())
                if (action->text().startsWith("User Guide")) guide = action;
        }
    }
    if (!guide) { std::cerr << "Help menu lacks User Guide\n"; return 1; }
    guide->trigger();
    QFile file(browser.url.toLocalFile());
    QFile bundled(":/docs/user-guide.html");
    if (!browser.url.isLocalFile() || !file.open(QIODevice::ReadOnly) || !bundled.open(QIODevice::ReadOnly)) return 2;
    const QByteArray html = file.readAll();
    if (html != bundled.readAll() || !html.contains("Subbiah Rathinagiri") || !html.contains("Select Brush") || !html.contains("Tutorial 2")) return 3;
    bool names = false;
    for (QComboBox *combo : window.findChildren<QComboBox *>())
        if (combo->findText("Select Brush") >= 0 && combo->findText("Erase Brush") >= 0 && combo->findText("Reveal brush") < 0) names = true;
    if (!names) return 4;
    QDesktopServices::unsetUrlHandler("file");
    std::cout << "Help menu opens exact embedded guide through file URL; new brush names verified.\n";
    return 0;
}

#include "help_test.moc"
