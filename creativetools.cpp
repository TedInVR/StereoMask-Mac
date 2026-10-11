#include "mainwindow.h"
#include "stereoviewwidget.h"
#include "stereowindowview.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QSplitter>
#include <QScrollArea>
#include <QComboBox>
#include <QFontComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QTextEdit>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QColorDialog>
#include <QMessageBox>
#include <QShortcut>
#include <QSignalBlocker>
#include <QUuid>
#include <QPainter>
#include <QMouseEvent>

namespace {
class CreativeCanvas : public StereoWindowView {
    int drag=0;bool right=false;QPointF start;StereoText original;
public:
    QSize eyeSize;
    bool editing=true,showTail=true;
    std::function<StereoText()> current;
    std::function<void(StereoText)> changed;
    explicit CreativeCanvas(QWidget *p):StereoWindowView(p){}
    QPointF shifted(QPointF p,bool r)const {if(r)p.rx()+=eyeSize.width()+20-current().depth;return p;}
protected:
    void paintEvent(QPaintEvent *e)override {
        StereoWindowView::paintEvent(e);if(!editing || !current)return;
        auto t=current();QPainter p(viewport());p.setPen(QPen(Qt::cyan,2,Qt::DashLine));
        for(bool r:{false,true}){
            QRectF rect(QPointF(mapFromScene(shifted(t.rect.topLeft(),r))),QPointF(mapFromScene(shifted(t.rect.bottomRight(),r))));
            p.setBrush(Qt::NoBrush);p.drawRect(rect);p.setBrush(Qt::cyan);
            p.drawRect(QRectF(rect.bottomRight()-QPointF(5,5),QSizeF(10,10)));
            if(showTail && t.style){const QPointF tail=mapFromScene(shifted(t.tail,r));p.drawEllipse(tail,6,6);}
        }
    }
    void mousePressEvent(QMouseEvent *e)override {
        if(editing && current && changed && e->button()==Qt::LeftButton && !(e->modifiers()&Qt::AltModifier)){
            original=current();start=mapToScene(e->pos());
            for(bool r:{false,true}){
                const QPointF corner=mapFromScene(shifted(original.rect.bottomRight(),r));
                const QPointF tail=mapFromScene(shifted(original.tail,r));
                QRectF rect(shifted(original.rect.topLeft(),r),shifted(original.rect.bottomRight(),r));
                if(showTail && original.style && QLineF(e->position(),tail).length()<12)drag=3;
                else if(QLineF(e->position(),corner).length()<12)drag=2;
                else if(rect.contains(start))drag=1;
                if(drag){right=r;e->accept();return;}
            }
        }
        StereoWindowView::mousePressEvent(e);
    }
    void mouseMoveEvent(QMouseEvent *e)override {
        if(drag && (e->buttons()&Qt::LeftButton)){
            const QPointF delta=mapToScene(e->pos())-start;auto t=original;
            if(drag==1){t.rect.translate(delta);t.tail+=delta;}
            if(drag==2)t.rect.setSize(QSizeF(qBound(32.0,original.rect.width()+delta.x(),eyeSize.width()*4.0),qBound(32.0,original.rect.height()+delta.y(),eyeSize.height()*4.0)));
            if(drag==3)t.tail+=delta;
            changed(t);e->accept();return;
        }
        StereoWindowView::mouseMoveEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent *e)override {if(drag){drag=0;e->accept();return;}StereoWindowView::mouseReleaseEvent(e);}
};
void canvasControls(QVBoxLayout *layout,QDialog *dialog,StereoWindowView *canvas) {
    auto row=new QHBoxLayout;
    for(auto entry:QList<QPair<QString,double>>{{QObject::tr("Zoom +"),1.25},{QObject::tr("Zoom −"),0.8},{QObject::tr("Fit"),0}}){
        auto button=new QPushButton(entry.first);row->addWidget(button);
        QObject::connect(button,&QPushButton::clicked,dialog,[canvas,f=entry.second]{if(f)canvas->zoomImage(f);else canvas->fitImage();});
    }
    row->addWidget(new QLabel(QObject::tr("Scroll to zoom. Drag inside to move; drag the corner to resize. Option-drag to pan.")));row->addStretch();layout->addLayout(row);
    for(auto entry:QList<QPair<QString,double>>{{"Ctrl++",1.25},{"Ctrl+=",1.25},{"Ctrl+-",0.8},{"Ctrl+0",0}}){
        auto key=new QShortcut(QKeySequence(entry.first),dialog);QObject::connect(key,&QShortcut::activated,dialog,[canvas,f=entry.second]{if(f)canvas->zoomImage(f);else canvas->fitImage();});
    }
}
QImage pairImage(const QImage &left,const QImage &right) {
    QImage pair(left.width()*2+20,left.height(),QImage::Format_ARGB32_Premultiplied);pair.fill(QColor("#333333"));
    QPainter p(&pair);p.drawImage(0,0,left);p.drawImage(left.width()+20,0,right);return pair;
}
}

void MainWindow::createFrameShape() {
    if(!m_viewWidget->isImageLoaded())return;
    if(m_viewWidget->hasLasso() || m_viewWidget->polygonMoveEnabled()){QMessageBox::information(this,tr("Shapes"),tr("Apply or cancel the unfinished lasso and turn Move polygon off first."));return;}
    QDialog dialog(this);dialog.setObjectName("frameShapeDialog");dialog.setWindowTitle(tr("Create Frame — Shapes"));
    auto layout=new QVBoxLayout(&dialog);layout->addWidget(new QLabel(tr("Choose a shape, drag it into position, and resize with its bottom-right handle. Apply creates editable frame dots.")));
    auto canvas=new CreativeCanvas(&dialog);canvas->setObjectName("shapeCanvas");canvas->showTail=false;canvas->eyeSize=m_viewWidget->image().size();layout->addWidget(canvas,1);
    canvasControls(layout,&dialog,canvas);
    StereoText draft;draft.rect=QRectF(canvas->eyeSize.width()*0.15,canvas->eyeSize.height()*0.15,canvas->eyeSize.width()*0.7,canvas->eyeSize.height()*0.7);
    auto row=new QHBoxLayout;auto shape=new QComboBox;shape->setObjectName("frameShapeType");shape->addItems({tr("Rectangle"),tr("Square"),tr("Oval"),tr("Circle")});row->addWidget(shape);
    auto field=[&](const QString &name,int min,int max,int value){row->addWidget(new QLabel(name));auto s=new QSpinBox;s->setRange(min,max);s->setValue(value);row->addWidget(s);return s;};
    auto width=field(tr("Width"),32,canvas->eyeSize.width()*4,int(draft.rect.width()));auto height=field(tr("Height"),32,canvas->eyeSize.height()*4,int(draft.rect.height()));
    auto depth=field(tr("Depth"),-10000,10000,0);depth->setObjectName("shapeDepth");
    layout->addLayout(row);
    auto redraw=[&]{
        const bool equal=shape->currentIndex()==1 || shape->currentIndex()==3;
        if(equal)draft.rect.setHeight(draft.rect.width());
        draft.depth=depth->value();draft.style=shape->currentIndex()>=2 ? 1:2;
        {QSignalBlocker a(width),b(height);width->setValue(qRound(draft.rect.width()));height->setValue(qRound(draft.rect.height()));}height->setEnabled(!equal);
        auto left=m_viewWidget->leftImage(),right=m_viewWidget->rightImage();
        const auto ps=shapePoints(shape->currentIndex(),draft.rect,draft.depth);
        PictureFrameSettings settings;settings.enabled=true;settings.width=3;settings.style=0;settings.color=Qt::cyan;
        QImage empty(left.size(),QImage::Format_ARGB32_Premultiplied);empty.fill(Qt::transparent);
        {QPainter p(&left);p.drawImage(0,0,StereoViewWidget::pictureFrameOverlay(left.size(),ps,false,empty,settings));}
        {QPainter p(&right);p.drawImage(0,0,StereoViewWidget::pictureFrameOverlay(right.size(),ps,true,empty,settings));}
        canvas->setImage(pairImage(left,right));canvas->viewport()->update();
    };
    canvas->current=[&]{return draft;};canvas->changed=[&](StereoText value){draft=value;redraw();};
    connect(shape,QOverload<int>::of(&QComboBox::currentIndexChanged),&dialog,[&]{if(shape->currentIndex()==1 || shape->currentIndex()==3){const double side=qMin(draft.rect.width(),draft.rect.height());draft.rect.setSize(QSizeF(side,side));}redraw();});
    connect(width,QOverload<int>::of(&QSpinBox::valueChanged),&dialog,[&](int v){draft.rect.setWidth(v);redraw();});
    connect(height,QOverload<int>::of(&QSpinBox::valueChanged),&dialog,[&](int v){draft.rect.setHeight(v);redraw();});
    connect(depth,QOverload<int>::of(&QSpinBox::valueChanged),&dialog,[&]{redraw();});
    auto note=new QLabel(tr("Positive depth puts the frame forward. After Apply, use Edit Frame to pull dots or curve handles, add points, scale or move. Replaces the basic frame; foreground selections remain. Undo restores the old frame."));note->setWordWrap(true);layout->addWidget(note);
    auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Ok)->setText(tr("Apply Shape"));layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    redraw();dialog.showMaximized();
    if(dialog.exec()==QDialog::Accepted){
        if(!m_viewWidget->points().isEmpty() && QMessageBox::question(this,tr("Replace Frame"),tr("Replace the current basic frame with this shape? Foreground selections remain; Undo restores the old frame."))!=QMessageBox::Yes)return;
        m_viewWidget->replaceFramePoints(shapePoints(shape->currentIndex(),draft.rect,draft.depth));
        if(auto action=findChild<QAction *>("editFramePointsAction"))action->trigger();
        if(auto toggle=findChild<QAction *>("sectionToggle4"))toggle->setChecked(true);
    }
}

void MainWindow::editStereoText() {
    if(!m_viewWidget->isImageLoaded())return;
    if(m_viewWidget->hasLasso() || m_viewWidget->polygonMoveEnabled()){QMessageBox::information(this,tr("Text & Speech Bubbles"),tr("Apply or cancel the unfinished lasso and turn Move polygon off first."));return;}
    QDialog dialog(this);dialog.setObjectName("stereoTextDialog");dialog.setWindowTitle(tr("Stereo Text & Speech Bubbles"));
    auto layout=new QVBoxLayout(&dialog);auto splitter=new QSplitter;layout->addWidget(splitter,1);
    auto canvas=new CreativeCanvas(&dialog);canvas->setObjectName("textCanvas");canvas->eyeSize=m_viewWidget->image().size();splitter->addWidget(canvas);
    auto panel=new QWidget;auto form=new QFormLayout(panel);form->setRowWrapPolicy(QFormLayout::WrapAllRows);panel->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);auto scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);scroll->setWidget(panel);scroll->setMinimumWidth(310);scroll->setMaximumWidth(400);splitter->addWidget(scroll);splitter->setStretchFactor(0,1);
    QVector<StereoText> draft=m_viewWidget->textObjects();int active=0;bool syncing=false,anaglyph=false;
    auto objects=new QComboBox;objects->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);objects->setMinimumContentsLength(16);objects->setObjectName("textObjects");form->addRow(tr("Text object:"),objects);
    auto tools=new QHBoxLayout;auto add=new QPushButton(tr("Add"));auto duplicate=new QPushButton(tr("Duplicate"));auto remove=new QPushButton(tr("Delete"));tools->addWidget(add);tools->addWidget(duplicate);tools->addWidget(remove);form->addRow(tools);
    auto content=new QTextEdit;content->setObjectName("stereoTextContent");content->setMaximumHeight(100);form->addRow(tr("Text:"),content);
    auto font=new QFontComboBox;font->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);font->setMinimumContentsLength(16);form->addRow(tr("Font:"),font);
    auto spin=[&](const QString &label,int min,int max){auto s=new QSpinBox;s->setRange(min,max);form->addRow(label,s);return s;};
    auto size=spin(tr("Font size (pixels):"),1,1000);
    auto fontStyle=new QHBoxLayout;auto bold=new QCheckBox(tr("Bold"));auto italic=new QCheckBox(tr("Italic"));fontStyle->addWidget(bold);fontStyle->addWidget(italic);form->addRow(fontStyle);
    auto style=new QComboBox;style->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);style->setMinimumContentsLength(16);style->setObjectName("speechBubbleStyle");style->addItems({tr("Text only"),tr("Oval speech bubble"),tr("Rounded rectangle speech bubble"),tr("Thought bubble")});form->addRow(tr("Bubble style:"),style);
    auto alignment=new QComboBox;alignment->addItems({tr("Left"),tr("Center"),tr("Right")});form->addRow(tr("Text alignment:"),alignment);
    auto x=spin(tr("Left-eye X:"),-100000,100000),y=spin(tr("Y:"),-100000,100000);
    auto w=spin(tr("Width:"),32,100000),h=spin(tr("Height:"),32,100000);
    auto tx=spin(tr("Tail X:"),-100000,100000),ty=spin(tr("Tail Y:"),-100000,100000);
    auto depth=spin(tr("Stereo depth (pixels):"),-10000,10000);depth->setObjectName("textDepth");
    auto slider=new QSlider(Qt::Horizontal);slider->setRange(-10000,10000);form->addRow(tr("Back ← depth → Forward"),slider);
    auto fillOpacity=spin(tr("Bubble fill opacity (%):"),0,100);auto outline=spin(tr("Outline width:"),0,100);
    auto visible=new QCheckBox(tr("Visible"));auto covers=new QCheckBox(tr("Foreground covers text"));covers->setToolTip(tr("Depth alone does not hide text behind photo objects. Your preserved foreground selections can cover it."));form->addRow(visible);form->addRow(covers);
    auto textColor=new QPushButton(tr("Text color…")),fill=new QPushButton(tr("Bubble fill color…")),stroke=new QPushButton(tr("Outline color…"));form->addRow(textColor);form->addRow(fill);form->addRow(stroke);
    auto note=new QLabel(tr("Edit once for both eyes. Positive depth pulls forward; negative pushes back. Drag the box to move, its corner to resize, or its tail dot to point a bubble. Fill opacity 0 makes the bubble interior transparent. Text wraps to its box; enlarge the box if text is clipped."));note->setWordWrap(true);form->addRow(note);
    canvasControls(layout,&dialog,canvas);
    auto preview=new QPushButton(tr("Preview Anaglyph"));layout->addWidget(preview);
    auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Ok)->setText(tr("Apply Text & Bubbles"));layout->addWidget(buttons);
    const QImage baseLeft=m_viewWidget->exportEye(false,false),baseRight=m_viewWidget->exportEye(true,false);
    auto redraw=[&]{
        QImage left=baseLeft,right=baseRight;
        {QPainter p(&left);p.drawImage(0,0,textOverlay(left.size(),draft,false,m_viewWidget->revealMask(false)));}
        {QPainter p(&right);p.drawImage(0,0,textOverlay(right.size(),draft,true,m_viewWidget->revealMask(true)));}
        canvas->editing=!anaglyph && !draft.isEmpty();
        if(anaglyph){StereoImageProcessor processor;processor.createAnaglyphFromEyes(left,right);canvas->setImage(processor.anaglyphImage());}
        else canvas->setImage(pairImage(left,right));
        canvas->viewport()->update();
    };
    std::function<void()> sync;
    sync=[&]{
        syncing=true;objects->clear();for(int i=0;i<draft.size();++i)objects->addItem(tr("%1: %2").arg(i+1).arg(draft[i].text.left(30)));
        if(!draft.isEmpty()){
            active=qBound(0,active,int(draft.size())-1);objects->setCurrentIndex(active);const auto &t=draft[active];
            content->setPlainText(t.text);font->setCurrentFont(t.font);size->setValue(t.fontSize);bold->setChecked(t.font.bold());italic->setChecked(t.font.italic());
            style->setCurrentIndex(t.style);alignment->setCurrentIndex(t.alignment);x->setValue(qRound(t.rect.x()));y->setValue(qRound(t.rect.y()));w->setValue(qRound(t.rect.width()));h->setValue(qRound(t.rect.height()));tx->setValue(qRound(t.tail.x()));ty->setValue(qRound(t.tail.y()));
            depth->setValue(t.depth);slider->setValue(t.depth);fillOpacity->setValue(t.opacity);outline->setValue(t.outline);visible->setChecked(t.visible);covers->setChecked(t.foregroundCovers);
            for(auto entry:QList<QPair<QPushButton *,QColor>>{{textColor,t.color},{fill,t.fill},{stroke,t.stroke}})entry.first->setStyleSheet(QString("border:3px solid %1").arg(entry.second.name()));
        }
        for(auto widget:panel->findChildren<QWidget *>())if(widget!=add)widget->setEnabled(!draft.isEmpty());
        add->setEnabled(draft.size()<100);syncing=false;redraw();
    };
    auto update=[&]{
        if(syncing || draft.isEmpty())return;auto &t=draft[active];
        t.text=content->toPlainText().left(10000);t.font=font->currentFont();t.font.setBold(bold->isChecked());t.font.setItalic(italic->isChecked());t.fontSize=size->value();
        t.style=style->currentIndex();t.alignment=alignment->currentIndex();t.rect=QRectF(x->value(),y->value(),w->value(),h->value());t.tail=QPointF(tx->value(),ty->value());t.depth=depth->value();t.opacity=fillOpacity->value();t.outline=outline->value();t.visible=visible->isChecked();t.foregroundCovers=covers->isChecked();
        {QSignalBlocker b(slider);slider->setValue(t.depth);}objects->setItemText(active,tr("%1: %2").arg(active+1).arg(t.text.left(30)));redraw();
    };
    canvas->current=[&]{return draft.isEmpty()?StereoText{}:draft[active];};
    canvas->changed=[&](StereoText t){if(draft.isEmpty())return;draft[active]=t;sync();};
    connect(objects,QOverload<int>::of(&QComboBox::currentIndexChanged),&dialog,[&](int i){if(!syncing && i>=0){active=i;sync();}});
    connect(add,&QPushButton::clicked,&dialog,[&]{StereoText t;t.id=QUuid::createUuid().toString(QUuid::WithoutBraces);t.rect=QRectF(canvas->eyeSize.width()*0.2,canvas->eyeSize.height()*0.2,qMax(64,canvas->eyeSize.width()/2),qMax(48,canvas->eyeSize.height()/4));t.fontSize=qMax(12,canvas->eyeSize.height()/20);t.tail=t.rect.bottomLeft()+QPointF(20,60);draft.append(t);active=draft.size()-1;sync();});
    connect(duplicate,&QPushButton::clicked,&dialog,[&]{if(draft.isEmpty() || draft.size()>=100)return;auto t=draft[active];t.id=QUuid::createUuid().toString(QUuid::WithoutBraces);t.rect.translate(20,20);t.tail+=QPointF(20,20);draft.append(t);active=draft.size()-1;sync();});
    connect(remove,&QPushButton::clicked,&dialog,[&]{if(draft.isEmpty())return;draft.removeAt(active);sync();});
    connect(content,&QTextEdit::textChanged,&dialog,update);connect(font,&QFontComboBox::currentFontChanged,&dialog,[&]{update();});
    for(auto s:{size,x,y,w,h,tx,ty,depth,fillOpacity,outline})connect(s,QOverload<int>::of(&QSpinBox::valueChanged),&dialog,[&]{update();});
    for(auto c:{bold,italic,visible,covers})connect(c,&QCheckBox::toggled,&dialog,[&]{update();});
    for(auto c:{style,alignment})connect(c,QOverload<int>::of(&QComboBox::currentIndexChanged),&dialog,[&]{update();});
    connect(slider,&QSlider::valueChanged,depth,&QSpinBox::setValue);
    auto pick=[&](QPushButton *button,int which){connect(button,&QPushButton::clicked,&dialog,[&,which]{if(draft.isEmpty())return;auto &t=draft[active];QColor &color=which==0?t.color:which==1?t.fill:t.stroke;auto next=QColorDialog::getColor(color,&dialog);if(next.isValid()){color=next;sync();}});};
    pick(textColor,0);pick(fill,1);pick(stroke,2);
    connect(preview,&QPushButton::clicked,&dialog,[&]{anaglyph=!anaglyph;preview->setText(anaglyph?tr("Return to Side-by-Side Editing"):tr("Preview Anaglyph"));canvas->fitImage();redraw();});
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(draft.isEmpty())add->click();else sync();dialog.showMaximized();
    if(dialog.exec()==QDialog::Accepted)m_viewWidget->setTextObjects(draft);
}
