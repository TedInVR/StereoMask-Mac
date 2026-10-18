#pragma once
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QMessageBox>
#include <QKeyEvent>
#include <QUrl>

// Browser links only. This window neither contacts payment sites on opening
// nor handles payments, credentials, membership status or user settings.
class SupportDialog : public QDialog {
public:
    explicit SupportDialog(QWidget *parent=nullptr):QDialog(parent) {
        setObjectName("supportStereoMaskDialog");
        setWindowTitle(tr("Support StereoMask"));
        setAccessibleName(tr("Support StereoMask"));
        setWindowModality(Qt::NonModal);
        setAttribute(Qt::WA_DeleteOnClose,false);
        resize(470,430);
        auto layout=new QVBoxLayout(this);layout->setContentsMargins(24,24,24,20);layout->setSpacing(14);
        auto title=new QLabel(tr("<h2>Support StereoMask</h2>"),this);layout->addWidget(title);
        auto introduction=new QLabel(tr("StereoMask is a collaborative effort by Subbiah Rathinagiri and Theodore (Ted) Whitten. The Mac version is provided free of charge."),this);
        introduction->setWordWrap(true);layout->addWidget(introduction);
        auto purpose=new QLabel(tr("If you enjoy using the app, voluntary contributions through these links support Ted in VR’s work on this Mac version and his other creative projects."),this);
        purpose->setWordWrap(true);layout->addWidget(purpose);
        struct Link {const char *id;const char *label;const char *url;};
        const Link links[]={
            {"supportKofi","Ko-fi — Leave a Tip","https://ko-fi.com/tedwhitten"},
            {"supportPaypal","PayPal — Make a Donation","https://paypal.me/theodorewhitten1"},
            {"supportPatreon","Patreon — Become a Supporter","https://patreon.com/TedWhitten"}
        };
        for(const auto &link:links){
            auto button=new QPushButton(tr(link.label),this);
            button->setObjectName(link.id);button->setAccessibleName(button->text());
            button->setMinimumHeight(38);button->setAutoDefault(false);
            const QUrl destination(QString::fromUtf8(link.url));
            button->setToolTip(destination.toString());
            button->setAccessibleDescription(tr("Opens %1 in your default browser.").arg(destination.toString()));
            layout->addWidget(button);
            connect(button,&QPushButton::clicked,this,[this,destination]{
                if(!QDesktopServices::openUrl(destination))
                    QMessageBox::information(this,tr("Open Support Page"),tr("Could not open your browser. You can visit:\n%1").arg(destination.toString()));
            });
        }
        auto note=new QLabel(tr("Ko-fi and PayPal offer one-time support. Patreon is for ongoing support; available options are shown on its website. Contributions are optional and unlock no app features.\n\nThank you for supporting Ted in VR!"),this);
        note->setWordWrap(true);layout->addWidget(note);
        auto buttons=new QDialogButtonBox(QDialogButtonBox::Close,this);layout->addWidget(buttons);
        connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
        buttons->button(QDialogButtonBox::Close)->setDefault(true);
        buttons->button(QDialogButtonBox::Close)->setFocus();
        // Give wrapped copy its full height at the actual panel width. A
        // short fixed initial dialog can otherwise clip the closing paragraph.
        layout->activate();
        const int height=qMax(430,layout->totalHeightForWidth(470));
        setMinimumSize(470,height);
        resize(470,height);
    }
protected:
    bool event(QEvent *event) override {
        if(event->type()==QEvent::ShortcutOverride){
            auto key=static_cast<QKeyEvent *>(event);
            if(key->key()==Qt::Key_W && key->modifiers()==Qt::ControlModifier){event->accept();return true;}
        }
        return QDialog::event(event);
    }
    void keyPressEvent(QKeyEvent *event) override {
        if(event->key()==Qt::Key_W && event->modifiers()==Qt::ControlModifier){reject();event->accept();return;}
        QDialog::keyPressEvent(event);
    }
};
