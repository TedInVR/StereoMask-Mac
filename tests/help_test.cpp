#include "mainwindow.h"
#include "stereoviewwidget.h"
#include <QApplication>
#include <QComboBox>
#include "stereowindowview.h"
#include <QSlider>
#include <QDesktopServices>
#include <QFile>
#include <QMenu>
#include <QMenuBar>
#include <QStandardPaths>
#include <QUrl>
#include <QToolButton>
#include <QMessageBox>
#include <QLabel>
#include <QTimer>
#include <QTreeWidget>
#include <QTemporaryDir>
#include <QMouseEvent>
#include <QPainter>
#include <QLocale>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QKeyEvent>
#include <QTextBrowser>
#include <QLineEdit>
#include <QTableWidget>
#include <QCheckBox>
#include <QToolBar>
#include <QDockWidget>
#include <QSettings>
#include <QCheckBox>
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
    QLocale::setDefault(QLocale(QLocale::English,QLocale::UnitedStates));
    QStandardPaths::setTestModeEnabled(true);
    BrowserCapture browser;
    QDesktopServices::setUrlHandler("file", &browser, "open");
    MainWindow window;
    QAction *guide = nullptr;
    bool updates = false;
    for (QAction *menuAction : window.menuBar()->actions()) {
        if (menuAction->text() == "&Help") {
            for (QAction *action : menuAction->menu()->actions())
                if (action->text().startsWith("User Guide")) guide = action;
                else if (action->text().startsWith("Check for Updates")) updates = true;
        }
    }
    if (!guide || !updates) { std::cerr << "Help menu lacks User Guide\n"; return 1; }
    guide->trigger();
    QFile file(browser.url.toLocalFile());
    QFile bundled(":/docs/user-guide.html");
    if (!browser.url.isLocalFile() || !file.open(QIODevice::ReadOnly) || !bundled.open(QIODevice::ReadOnly)) return 2;
    const QByteArray html = file.readAll();
    if (!html.contains("First optional step") || !html.contains("Reset to original")) return 34;
    if (html != bundled.readAll() || !html.contains("Subbiah Rathinagiri") || !html.contains("Select Brush") || !html.contains("Tutorial 2")) return 3;
    for (const QByteArray &feature: {QByteArray("Smart polygon lasso"),QByteArray("Paired polygon selections"),QByteArray("History"),QByteArray("Command + Enter"),QByteArray("Command-click lasso anchor"),QByteArray("Shift-click lasso edge"),QByteArray("F8"),QByteArray("Match preview"),QByteArray("Padding/gap color"),QByteArray("QTAGENT Rect Decompose"),QByteArray("Bernd Paksa")})
        if (!html.contains(feature)) {std::cerr<<"Guide lacks current feature: "<<feature.constData()<<'\n';return 3;}
    bool names = false;
    for (QComboBox *combo : window.findChildren<QComboBox *>())
        if (combo->findText("Select Brush") >= 0 && combo->findText("Erase Brush") >= 0 && combo->findText("Reveal brush") < 0) names = true;
    if (!names) return 4;
    bool smartLasso = false;
    for (QComboBox *combo : window.findChildren<QComboBox *>())
        if (combo->findText("Smart polygon lasso") >= 0 &&
            combo->itemData(combo->findText("Smart polygon lasso")).toInt() == 8)
            smartLasso = true;
    if (!smartLasso) { std::cerr << "Smart polygon lasso is unavailable\n"; return 4; }
    auto pairedLasso=window.findChild<QAction *>("pairedLassoAction");
    auto stereoView=window.findChild<StereoViewWidget *>();
    if (!pairedLasso || pairedLasso->icon().isNull() || !pairedLasso->isCheckable() || pairedLasso->isChecked() || !stereoView) return 4;
    pairedLasso->trigger();
    if (!stereoView->pairedLassoEnabled()) return 4;
    pairedLasso->trigger();
    if (stereoView->pairedLassoEnabled()) return 4;
    auto exportBackground=window.findChild<QComboBox *>("exportBackgroundCombo");
    if (!exportBackground || exportBackground->findText("Match preview")!=4 || exportBackground->currentIndex()!=4 || exportBackground->findText("Transparent (PNG)")!=0) return 19;
    auto applyLasso=window.findChild<QAction *>("applyLassoAction");
    auto cleanPreview=window.findChild<QAction *>("cleanPreviewAction");
    if (!applyLasso || !applyLasso->shortcuts().contains(QKeySequence(Qt::CTRL|Qt::Key_Return)) || !cleanPreview || !cleanPreview->isCheckable() || cleanPreview->shortcut()!=QKeySequence(Qt::Key_F8)) return 17;
    cleanPreview->trigger();if (!stereoView->cleanPreviewEnabled()) return 18;
    cleanPreview->trigger();if (stereoView->cleanPreviewEnabled()) return 18;
    bool algorithms = false;
    for (QToolButton *button : window.findChildren<QToolButton *>()) {
        if (button->text() != "Auto" || !button->menu()) continue;
        QStringList labels;
        for (QAction *action : button->menu()->actions()) labels.append(action->text());
        algorithms = labels.contains("QTAGENT Rect Decompose") &&
                     labels.contains("SGBM Contour") && labels.contains("Legacy Smart Mask");
    }
    if (!algorithms) { std::cerr << "AutoMask choices lost during port\n"; return 5; }
    bool credited = false;
    QTimer::singleShot(0, &window, [&] {
        for (QMessageBox *box : window.findChildren<QMessageBox *>()) {
            if (box->windowTitle() != "About StereoMask") continue;
            const QString text = box->text();
            bool externalLink = false;
            for (QLabel *label : box->findChildren<QLabel *>()) {
                if (label->text().contains("https://www.stereomasken.de/index_en.htm"))
                    externalLink = label->openExternalLinks();
            }
            credited = text.contains("Bernd Paksa") && text.contains("creative") && externalLink;
            box->accept();
        }
    });
    QMetaObject::invokeMethod(&window, "showAbout", Qt::DirectConnection);
    if (!credited) { std::cerr << "Original creator attribution or clickable link missing\n"; return 7; }
    auto compact=window.findChild<QToolBar *>("compactControls");
    auto history=window.findChild<QDockWidget *>("selectionObjectsDock");
    if (!compact || !history || history->windowTitle()!=QString("History")) return 40;
    for(int i=0;i<7;++i) {auto toggle=window.findChild<QAction *>(QString("sectionToggle%1").arg(i));if(!toggle)return 41;toggle->setChecked(false);}
    window.resize(1280, 900);
    window.show();
    QApplication::processEvents();
    if (!compact->isVisible()) return 42;
    for(int i=0;i<7;++i) {auto section=window.findChild<QToolBar *>(QString("controlSection%1").arg(i));if(!section || section->isVisible())return 43;}
    auto first=window.findChild<QAction *>("sectionToggle0");first->setChecked(true);
    if (!window.findChild<QToolBar *>("controlSection0")->isVisible() || window.findChild<QToolBar *>("controlSection1")->isVisible())return 44;
    first->setChecked(false);history->toggleViewAction()->trigger();if(!history->isVisible())return 45;history->toggleViewAction()->trigger();
    window.grab().save("compact-controls.png");
    auto list=window.findChild<QTreeWidget *>("selectionObjectsList");
    auto view=qobject_cast<StereoViewWidget *>(window.centralWidget());
    if (!list || !view) return 8;
    QTemporaryDir selectionFixture;
    QImage pair(480,180,QImage::Format_RGB32); pair.fill(QColor(85,130,160));
    {QPainter p(&pair); p.fillRect(QRect(90,20,60,140),QColor(220,170,90));p.fillRect(QRect(330,20,60,140),QColor(220,170,90));}
    const QString imagePath=selectionFixture.path()+"/panel.png";
    if (!pair.save(imagePath)) return 9;
    view->setAutoSave(false); if (!view->loadImage(imagePath)) return 10;

    auto windowAction=window.findChild<QAction *>("stereoWindowAction");
    if (!windowAction || !windowAction->isEnabled()) return 30;
    auto fileControls=window.findChild<QToolBar *>("controlSection0");
    auto editControls=window.findChild<QToolBar *>("controlSection2");
    const QStringList headings={"File","View","Stereo Window","Create Frame","Edit Frame","Out-of-Frame","Help"};
    for(int i=0;i<headings.size();++i)if(window.findChild<QAction *>(QString("sectionToggle%1").arg(i))->text()!=headings[i])return 47;
    if(fileControls->actions().size()!=6)return 48;
    auto outTools=window.findChild<QComboBox *>("outOfFrameToolCombo");if(!outTools || outTools->findData(0)>=0)return 49;
    if(!window.findChild<QSpinBox *>("framePointDepth"))return 50;
    auto closeAction=window.findChild<QAction *>("closeProjectAction");
    if (!fileControls || !editControls || !closeAction || fileControls->actions().contains(windowAction) || !editControls->actions().contains(windowAction) || !fileControls->actions().contains(closeAction)) return 46;
    bool windowPreview=false;
    QTimer::singleShot(0,[&]{
        auto dialog=window.findChild<QDialog *>("stereoWindowDialog");if (!dialog) return;
        auto slider=dialog->findChild<QSlider *>("stereoWindowSlider");
        auto label=dialog->findChild<StereoWindowView *>("stereoWindowPreview");
        auto spin=dialog->findChild<QSpinBox *>("stereoWindowPixels");
        auto back=dialog->findChild<QPushButton *>("stereoWindowPushBack");
        auto forward=dialog->findChild<QPushButton *>("stereoWindowPullForward");
        if (!slider || !spin || !back || !forward || spin->height()<44) {dialog->reject();return;}
        forward->click();back->click();if (spin->value()!=0) {dialog->reject();return;}
        for (QWidget *target:QList<QWidget *>{spin,slider,back,dialog}) {
            QKeyEvent right(QEvent::KeyPress,Qt::Key_Right,Qt::NoModifier);
            QApplication::sendEvent(target,&right);
            if (spin->value()!=1 || slider->value()!=1) {dialog->reject();return;}
            QKeyEvent left(QEvent::KeyPress,Qt::Key_Left,Qt::NoModifier);
            QApplication::sendEvent(target,&left);
            if (spin->value()!=0 || slider->value()!=0) {dialog->reject();return;}
        }
        slider->setValue(7);
        const double fitted=label->transform().m11();
        label->zoomImage(2);if(label->transform().m11()<=fitted){dialog->reject();return;}
        slider->setValue(8);if(label->transform().m11()!=fitted*2){dialog->reject();return;}
        label->fitImage();if(qAbs(label->transform().m11()-fitted)>0.001){dialog->reject();return;}
        windowPreview=slider && label && !label->scene()->items().isEmpty() && view->stereoWindowOffset()==0 && dialog->grab().save("stereo-window-dialog.png");dialog->reject();
    });
    windowAction->trigger();if (!windowPreview || view->stereoWindowOffset()!=0) return 31;
    QTimer::singleShot(0,[&]{auto dialog=window.findChild<QDialog *>("stereoWindowDialog");if (!dialog) return;
        auto slider=dialog->findChild<QSlider *>("stereoWindowSlider");if (slider) slider->setValue(7);dialog->accept();});
    windowAction->trigger();if (view->stereoWindowOffset()!=7) return 32;
    view->undoStack()->undo();if (view->stereoWindowOffset()!=0) return 33;
    auto opacitySpin=window.findChild<QSpinBox *>("maskOpacitySpin");
    auto maskColor=window.findChild<QAction *>("maskColorAction");
    auto picker=window.findChild<QAction *>("pickMaskColorAction");
    if (!opacitySpin || !maskColor || !picker) return 20;
    const QColor sharedColor(31,82,143);
    view->setMaskSettings(sharedColor,0.73f);
    if (opacitySpin->value()!=73 || maskColor->icon().pixmap(20,20).toImage().pixelColor(10,10)!=sharedColor) return 21;
    QAction *settingsAction=nullptr;
    for (auto action:window.findChildren<QAction *>()) if (action->text()=="Background & Export Settings") {settingsAction=action;break;}
    if (!settingsAction) return 22;
    bool settingsSynced=false;
    QTimer::singleShot(0,[&] {
        auto dialog=window.findChild<QDialog *>("maskSettingsDialog");
        if (!dialog) return;
        auto spin=dialog->findChild<QSpinBox *>("settingsMaskOpacitySpin");
        auto colorButton=dialog->findChild<QPushButton *>("settingsMaskColorButton");
        auto buttons=dialog->findChild<QDialogButtonBox *>();
        settingsSynced=spin && spin->value()==73 && colorButton && colorButton->styleSheet().contains(sharedColor.name());
        if (spin && buttons) {spin->setValue(61);buttons->button(QDialogButtonBox::Ok)->click();}
        else dialog->reject();
    });
    settingsAction->trigger();
    if (!settingsSynced || opacitySpin->value()!=61 || qRound(view->maskOpacity()*100)!=61) return 23;
    const auto acceptedColor=view->maskColor();
    QTimer::singleShot(0,[&] {
        auto dialog=window.findChild<QDialog *>("maskSettingsDialog");
        if (!dialog) return;
        auto spin=dialog->findChild<QSpinBox *>("settingsMaskOpacitySpin");
        if (spin) spin->setValue(18);
        dialog->reject();
    });
    settingsAction->trigger();
    if (opacitySpin->value()!=61 || qRound(view->maskOpacity()*100)!=61 || view->maskColor()!=acceptedColor) return 29;
    // Sampling changes mask appearance, not cutout color or export mode.
    const QColor previousCutoutColor=view->cutoutColor();const int previousExportMode=view->cutoutBackground();
    picker->trigger();
    const QPoint samplePoint(100,200);
    QMouseEvent samplePress(QEvent::MouseButtonPress,QPointF(samplePoint),QPointF(samplePoint),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QMouseEvent sampleRelease(QEvent::MouseButtonRelease,QPointF(samplePoint),QPointF(samplePoint),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
    QApplication::sendEvent(view,&samplePress);QApplication::sendEvent(view,&sampleRelease);
    if (view->maskColor()==sharedColor || view->cutoutColor()!=previousCutoutColor || view->cutoutBackground()!=previousExportMode || maskColor->icon().pixmap(20,20).toImage().pixelColor(10,10)!=view->maskColor()) return 24;
    if (!view->saveProject()) return 25;
    const QColor savedColor=view->maskColor();view->setMaskSettings(Qt::magenta,0.25f);
    if (!view->loadProject(selectionFixture.path()+"/panel.msk") || view->maskColor()!=savedColor || opacitySpin->value()!=61) return 26;
    view->setMaskSettings(Qt::white,1.0f);
    for (const QPointF &point:QVector<QPointF>{{65,35},{175,35},{175,115},{65,115}}) view->addPoint(point,0);
    SelectionObject object; object.id="panel-test";object.name="Foreground";
    object.anchors={{95,100},{145,100},{145,155},{95,155}}; object.magnetic.resize(3);
    object.path.addPolygon(QPolygonF(object.anchors)); object.path.closeSubpath();
    SelectionState state; state.objects.append(object); view->restoreSelectionState(state);
    if (list->topLevelItemCount()!=1) return 11;
    auto item=list->topLevelItem(0); list->setCurrentItem(item);
    const QPoint eyePoint(15,list->visualItemRect(item).center().y());
    QMouseEvent eyePress(QEvent::MouseButtonPress,QPointF(eyePoint),QPointF(eyePoint),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QMouseEvent eyeRelease(QEvent::MouseButtonRelease,QPointF(eyePoint),QPointF(eyePoint),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
    QApplication::sendEvent(list->viewport(),&eyePress); QApplication::sendEvent(list->viewport(),&eyeRelease);
    if (view->selectionObjects()[0].visible) return 12;
    view->undoStack()->undo();
    auto invertObject=window.findChild<QAction *>("invertSelectionModeAction");
    if (!invertObject || !invertObject->isEnabled() || invertObject->icon().isNull()) return 14;
    invertObject->trigger();
    if (!view->selectionObjects()[0].subtract || list->currentItem()->text(3)!="Subtract" || list->currentItem()->data(1,Qt::UserRole).toString()!=object.id) return 15;
    invertObject->trigger();
    if (view->selectionObjects()[0].subtract || list->currentItem()->text(3)!="Add") return 16;
    bool edited=false;
    for (QAction *action:window.findChildren<QAction *>()) if (action->objectName()=="editHistoryPolygonAction") {
        action->trigger(); edited=view->hasLasso(); break;
    }
    if (!edited) return 13;
    view->cancelLasso();
    auto movePolygon=window.findChild<QAction *>("movePolygonAction");
    if (!movePolygon || !movePolygon->isEnabled() || !movePolygon->isCheckable() || movePolygon->icon().isNull()) return 27;
    movePolygon->trigger();if (!view->polygonMoveEnabled()) return 28;
    movePolygon->trigger();if (view->polygonMoveEnabled()) return 28;
    QApplication::processEvents();
    if (!window.grab().save("mac-r11-ui.png")) return 6;
    auto mainMenu=window.findChild<QMenu *>("mainControlsMenu");if(!mainMenu)return 51;
    QStringList menuNames;for(auto action:mainMenu->actions())menuNames<<action->text();
    for(const QString &name:QStringList{"File","View","Stereo Window","Create Frame","Edit Frame","Out-of-Frame","History","Layout","Help","Guided Process","Check for Updates…","About"})if(!menuNames.contains(name))return 52;
    auto compactBar=window.findChild<QToolBar *>("compactControls");
    if(!compactBar->actions().contains(windowAction) || compactBar->actions().contains(window.findChild<QAction *>("sectionToggle2")))return 53;
    bool readableList=false;
    QTimer::singleShot(0,[&]{auto dialog=window.findChild<QDialog *>("keyboardShortcutsDialog");if(!dialog)return;auto browser=dialog->findChild<QTextBrowser *>();readableList=browser && browser->toPlainText().contains("Command") && browser->toPlainText().contains("Option") && browser->toPlainText().contains("Control");dialog->grab().save("keyboard-shortcuts.png");dialog->reject();});
    QMetaObject::invokeMethod(&window,"showKeyboardShortcuts",Qt::DirectConnection);if(!readableList)return 54;
    QAction *panShortcut=nullptr;for(auto action:window.findChildren<QAction *>())if(action->property("customShortcutLabel").toString()=="Pan")panShortcut=action;
    if(!panShortcut)return 55;const auto oldPan=panShortcut->shortcut();
    auto changePan=[&](bool save,bool reset){QTimer::singleShot(0,[&,save,reset]{auto dialog=window.findChild<QDialog *>("editShortcutsDialog");if(!dialog)return;auto table=dialog->findChild<QTableWidget *>();auto box=dialog->findChild<QDialogButtonBox *>();if(reset){for(auto button:box->buttons())if(button->text()=="Restore Defaults")button->click();}else for(int row=0;row<table->rowCount();++row)if(table->item(row,0)->text()=="Pan")qobject_cast<QLineEdit *>(table->cellWidget(row,1))->setText("Command+Shift+J");if(save)box->button(QDialogButtonBox::Save)->click();else dialog->reject();});QMetaObject::invokeMethod(&window,"editKeyboardShortcuts",Qt::DirectConnection);};
    changePan(false,false);if(panShortcut->shortcut()!=oldPan)return 56;
    changePan(true,false);if(panShortcut->shortcut()!=QKeySequence("Ctrl+Shift+J"))return 57;
    {MainWindow reloaded;bool restored=false;for(auto action:reloaded.findChildren<QAction *>())if(action->property("customShortcutLabel").toString()=="Pan")restored=action->shortcut()==QKeySequence("Ctrl+Shift+J");if(!restored)return 58;}
    changePan(true,true);if(panShortcut->shortcut()!=oldPan)return 59;
    bool aliasPreserved=false;for(auto action:window.findChildren<QAction *>())if(action->text()=="Zoom +")aliasPreserved=action->shortcuts().contains(QKeySequence("Ctrl+="));if(!aliasPreserved)return 72;
    bool conflictShown=false;
    QTimer::singleShot(0,[&]{auto dialog=window.findChild<QDialog *>("editShortcutsDialog");auto table=dialog->findChild<QTableWidget *>();auto box=dialog->findChild<QDialogButtonBox *>();for(int row=0;row<table->rowCount();++row)if(table->item(row,0)->text()=="Pan")qobject_cast<QLineEdit *>(table->cellWidget(row,1))->setText("Left");QTimer::singleShot(0,[&]{if(auto warning=window.findChild<QMessageBox *>()){conflictShown=true;warning->accept();}dialog->reject();});box->button(QDialogButtonBox::Save)->click();});
    QMetaObject::invokeMethod(&window,"editKeyboardShortcuts",Qt::DirectConnection);if(!conflictShown || panShortcut->shortcut()!=oldPan)return 73;
    {
        MainWindow guidedWindow;auto guidedView=qobject_cast<StereoViewWidget *>(guidedWindow.centralWidget());guidedView->setAutoSave(false);
        QMetaObject::invokeMethod(&guidedWindow,"startGuidedProcess",Qt::DirectConnection);auto dock=guidedWindow.findChild<QDockWidget *>("guidedProcessDock");if(!dock)return 60;
        auto next=dock->findChild<QPushButton *>("guidedNext");auto skip=dock->findChild<QPushButton *>("guidedSkip");auto back=dock->findChild<QPushButton *>("guidedBack");
        next->click();next->click();if(dock->property("step").toInt()!=1 || dock->findChild<QLabel *>("guidedMessage")->text().isEmpty())return 61;
        const QString fresh=selectionFixture.path()+"/guided.png";pair.save(fresh);if(!guidedView->loadImage(fresh))return 62;
        next->click();skip->click();if(dock->property("step").toInt()!=3)return 63;
        next->click();if(dock->property("step").toInt()!=3)return 64;
        for(const QPointF &point:QVector<QPointF>{{30,20},{210,20},{210,160},{30,160}})guidedView->addPoint(point,0);
        next->click();skip->click();guidedWindow.resize(1280,900);guidedWindow.show();QApplication::processEvents();guidedWindow.grab().save("guided-process.png");dock->findChild<QComboBox *>("guidedProtrude")->setCurrentIndex(1);next->click();if(dock->property("step").toInt()!=8)return 65;
        back->click();if(dock->property("step").toInt()!=5)return 66;
        dock->findChild<QComboBox *>("guidedProtrude")->setCurrentIndex(0);next->click();dock->findChild<QComboBox *>("guidedTool")->setCurrentIndex(1);next->click();if(guidedView->brushMode()!=7 || dock->property("step").toInt()!=7)return 67;
        next->click();if(dock->property("step").toInt()!=7)return 68;dock->findChild<QCheckBox *>("guidedBothEyes")->setChecked(true);next->click();skip->click();next->click();if(dock->property("step").toInt()!=10)return 69;
        dock->findChild<QPushButton *>("guidedAction")->click();if(!QFile::exists(selectionFixture.path()+"/guided.msk"))return 70;
        next->click();if(guidedView->points().size()!=4 || !guidedView->isImageLoaded())return 71;
    }
    std::cout<<"Main Menu coverage, direct Stereo Window, readable/custom shortcuts with persistence and defaults, guided gates/branches/back/save: PASS\n";
    QDesktopServices::unsetUrlHandler("file");
    std::cout << "Help menu opens exact embedded guide through file URL; new brush names verified.\n";
    return 0;
}

#include "help_test.moc"

