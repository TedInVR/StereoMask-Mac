#include "stereoviewwidget.h"
#include "decorations.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QPainter>
#include <QJsonDocument>
#include <iostream>
#include <cstdlib>
static void require(bool value,const char *message){if(!value){std::cerr<<message<<"\n";std::exit(1);}}
int main(int argc,char **argv) {
    QApplication app(argc,argv);QTemporaryDir folder;
    QImage source(720,240,QImage::Format_ARGB32);source.fill(Qt::white);
    const QString file=folder.path()+"/source.png";source.save(file);
    StereoViewWidget view;view.setAutoSave(false);require(view.loadImage(file),"load fixture");
    const QRectF bounds(60,40,140,140);
    view.replaceFramePoints(shapePoints(0,bounds,10));require(view.points().size()==4,"rectangle vertices");
    view.undoStack()->undo();require(view.points().isEmpty(),"shape undo");view.undoStack()->redo();
    const auto ellipse=shapePoints(2,bounds,10);view.replaceFramePoints(ellipse);
    require(ellipse[0].isCurve && ellipse[3].isCurve && ellipse[2].cp1Disparity==10,"ellipse cubic depth");
    const QImage originalOpening=view.openingMask(false);
    const auto a=ellipse[0],b=ellipse[1];const QPointF middle=(a.pos+a.cp1*3+a.cp2*3+b.pos)/8;
    view.addPoint(middle,10,1);require(view.points().size()==5 && view.points()[1].isCurve,"curve point insertion");
    const QImage splitOpening=view.openingMask(false);
    int difference=0;for(int y=0;y<240;++y)for(int x=0;x<360;++x)difference+=qAbs(qAlpha(originalOpening.pixel(x,y))-qAlpha(splitOpening.pixel(x,y)));
    require(difference<600,"curve split preserves ellipse");
    view.undoStack()->undo();require(view.points().size()==4,"curve insert undo");
    view.replaceFramePoints(shapePoints(0,bounds,10));view.setCutoutBackground(0);
    PictureFrameSettings shadow;shadow.shadowEnabled=true;shadow.shadowOpacity=100;shadow.shadowDistance=20;shadow.shadowAngle=0;shadow.shadowBlur=0;shadow.enabled=false;
    view.setPictureFrameSettings(shadow);
    auto layer=StereoViewWidget::shadowOverlay(QSize(360,240),view.points(),false,view.openingMask(false),shadow);
    require(qAlpha(layer.pixel(210,100))==255 && qAlpha(layer.pixel(100,100))==0,"shadow exterior only");
    auto rightShadow=StereoViewWidget::shadowOverlay(QSize(360,240),view.points(),true,view.openingMask(true),shadow);
    require(qAlpha(rightShadow.pixel(200,100))==255 && qAlpha(rightShadow.pixel(215,100))==0,"shadow eye depth");
    shadow.shadowOpacity=40;shadow.shadowBlur=8;view.setPictureFrameSettings(shadow);
    auto blurred=view.exportEye(false);require(qAlpha(blurred.pixel(220,100))>0 && qAlpha(blurred.pixel(210,100))<150,"shadow blur opacity and export");
    shadow.shadowEnabled=false;view.setPictureFrameSettings(shadow);require(qAlpha(view.exportEye(false).pixel(210,100))==0,"shadow cache refresh");
    StereoText t;t.id="fixture";t.text="";t.rect=QRectF(20,20,100,80);t.tail=QPointF(40,140);t.style=2;t.fill=Qt::red;t.outline=0;t.depth=15;
    auto left=textOverlay(QSize(360,240),{t},false),right=textOverlay(QSize(360,240),{t},true);
    require(left.pixelColor(70,60)==QColor(Qt::red) && qAlpha(right.pixel(110,60))==0 && right.pixelColor(55,60)==QColor(Qt::red),"bubble color and stereo placement");
    t.opacity=30;left=textOverlay(QSize(360,240),{t},false);require(qAlpha(left.pixel(70,60))>=75 && qAlpha(left.pixel(70,60))<=78,"translucent bubble");
    t.opacity=0;left=textOverlay(QSize(360,240),{t},false);require(qAlpha(left.pixel(70,60))==0,"transparent bubble");
    t.opacity=100;t.foregroundCovers=true;QImage foreground(360,240,QImage::Format_ARGB32_Premultiplied);foreground.fill(Qt::transparent);
    {QPainter p(&foreground);p.fillRect(QRect(50,40,40,40),Qt::white);}
    left=textOverlay(QSize(360,240),{t},false,foreground);require(qAlpha(left.pixel(70,60))==0 && qAlpha(left.pixel(100,60))==255,"foreground occlusion");
    for(int style:{0,1,2,3}){t.style=style;t.text="Stereo text";t.fontSize=18;auto layer=textOverlay(QSize(360,240),{t},false);require(!layer.isNull(),"bubble styles render");}
    const auto decoded=textFromJson(textToJson(t),QSize(360,240));require(decoded.id==t.id && decoded.depth==15 && decoded.tail==t.tail && decoded.text==t.text && decoded.foregroundCovers,"text JSON roundtrip");
    t.foregroundCovers=false;view.setTextObjects({t});require(view.textObjects().size()==1,"text applied");view.undoStack()->undo();require(view.textObjects().isEmpty(),"text undo");view.undoStack()->redo();
    shadow.shadowEnabled=true;view.setPictureFrameSettings(shadow);require(view.saveProject(),"save new fields");
    StereoViewWidget loaded;loaded.setAutoSave(false);require(loaded.loadProject(folder.path()+"/source.msk"),"reload new fields");
    require(loaded.textObjects().size()==1 && loaded.textObjects()[0].depth==15 && loaded.pictureFrameSettings().shadowEnabled,"project keeps decorations");
    require(loaded.exportEye(false)==view.exportEye(false),"project rendered parity");
    const QString output=folder.path()+"/output.png";require(StereoViewWidget::saveComposedPair(output,view.exportEye(false),view.exportEye(true),0,0,0,Qt::transparent),"decorated PNG export");
    require(QImage(output).size()==source.size(),"export size");
    require(loaded.closeProject() && loaded.textObjects().isEmpty() && !loaded.pictureFrameSettings().shadowEnabled,"close clears decoration state");
    std::cout<<"Shapes, curve splitting, shadow alpha/depth/cache, text/bubbles/occlusion, undo, project and PNG parity: PASS\n";
}
