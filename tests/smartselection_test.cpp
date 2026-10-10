#include "smartselection.h"
#include "stereoviewwidget.h"
#include <QApplication>
#include <QPainter>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QThread>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <iostream>
#include <stdexcept>

static void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
static QImage fixture() {
    QImage image(240,180,QImage::Format_RGB32); image.fill(QColor(60,130,65));
    QPainter p(&image); p.setPen(Qt::NoPen);p.setBrush(QColor(220,170,130));p.drawEllipse(QRect(60,28,96,130));p.end();
    // Texture avoids degenerate single-color GMMs and represents a photograph.
    for(int y=0;y<180;++y)for(int x=0;x<240;++x) {
        QColor c=image.pixelColor(x,y); const int n=((x*17+y*11)%15)-7;
        image.setPixelColor(x,y,QColor(c.red()+n,c.green()+n,c.blue()+n));
    }
    return image;
}
static QImage hints(QSize size) {
    QImage h(size,QImage::Format_ARGB32_Premultiplied);h.fill(Qt::transparent);
    QPainter p(&h);p.fillRect(QRect(104,70,8,45),Qt::green);p.fillRect(QRect(177,60,8,50),Qt::red);return h;
}
static void waitFor(bool &finished) {
    QElapsedTimer timer;timer.start();
    while(!finished && timer.elapsed()<20000) { QApplication::processEvents();QThread::msleep(10); }
    require(finished,"async edge selection timed out");
}
static void click(QWidget &view, QPoint pos) {
    QMouseEvent press(QEvent::MouseButtonPress,QPointF(pos),QPointF(pos),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(&view,&press);
    QMouseEvent release(QEvent::MouseButtonRelease,QPointF(pos),QPointF(pos),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
    QApplication::sendEvent(&view,&release);
}
int main(int argc,char **argv) {
 QApplication app(argc,argv);
 try {
    const QImage image=fixture();const QImage seed=hints(image.size());const QRect region(30,12,170,156);
    QImage rough(image.size(),QImage::Format_ARGB32_Premultiplied);rough.fill(Qt::transparent);
    {QPainter p(&rough);p.fillRect(QRect(43,20,126,145),Qt::white);p.fillRect(QRect(229,160,4,4),Qt::white);}
    const auto result=findSmartEdges(image,region,seed,rough);
    require(result.error.isEmpty()&&!result.mask.isNull(),"native GrabCut failed");
    require(qAlpha(result.mask.pixel(108-region.x(),90-region.y()))==255,"Keep seed lost");
    require(qAlpha(result.mask.pixel(180-region.x(),90-region.y()))==0,"Remove seed survived");
    require(qAlpha(result.mask.pixel(45-region.x(),90-region.y()))==0,"sloppy background survived");
    require(qAlpha(result.mask.pixel(65-region.x(),90-region.y()))==255,"object edge failed to expand beyond Keep stroke");
    const auto fresh=findSmartEdges(image,region,seed,QImage());
    require(fresh.error.isEmpty()&&qAlpha(fresh.mask.pixel(65-region.x(),90-region.y()))==255,"fresh selection needs pre-existing reveal");
    QImage distractor=image;{QPainter p(&distractor);p.setPen(Qt::NoPen);p.setBrush(QColor(220,170,130));p.drawEllipse(QRect(165,125,24,24));}
    const auto isolated=findSmartEdges(distractor,region,seed,rough);
    require(isolated.error.isEmpty()&&qAlpha(isolated.mask.pixel(175-region.x(),140-region.y()))==0,"unseeded disconnected object survived");
    require(!findSmartEdges(image,region,QImage(),rough).error.isEmpty(),"missing seeds should be rejected");
    require(!findSmartEdges(image,QRect(-1,0,20,20),seed,rough).error.isEmpty(),"invalid ROI should be rejected");
    QTemporaryDir temp;require(temp.isValid(),"temporary directory failed");
    QImage sbs(480,180,QImage::Format_RGB32);{QPainter p(&sbs);p.drawImage(0,0,image);p.drawImage(240,0,image);}
    const QString file=temp.path()+"/stereo.png";require(sbs.save(file),"fixture save failed");
    StereoViewWidget view;view.setAutoSave(false);view.resize(800,400);require(view.loadImage(file),"fixture load failed");
    view.addPoint(QPointF(20,20),13);view.addPoint(QPointF(210,20),13);view.addPoint(QPointF(210,65),13);view.addPoint(QPointF(20,65),13);
    const auto frame=view.points();
    view.applyReveal(false,rough);view.applySmartHints(false,seed,region);
    bool finished=false;QString status;
    QObject::connect(&view,&StereoViewWidget::smartBusy,[&](bool busy){if(!busy)finished=true;});
    QObject::connect(&view,&StereoViewWidget::smartStatus,[&](const QString &s){status=s;});
    const int undoIndex=view.undoStack()->index();view.findEdges();waitFor(finished);
    require(status.startsWith("Edges refined"),"async job did not apply");
    const QImage refined=view.revealMask(false);
    require(qAlpha(refined.pixel(45,90))==0,"refinement failed to replace sloppy paint");
    require(qAlpha(refined.pixel(230,161))==255,"paint outside ROI was modified");
    require(view.revealMask(true).isNull(),"opposite eye was modified");
    require(view.points().size()==frame.size(),"frame point count changed");
    for(int i=0;i<frame.size();++i) require(view.points()[i].pos==frame[i].pos&&view.points()[i].disparity==13,"frame geometry or depth changed");
    require(view.undoStack()->index()==undoIndex+1,"refinement not one undo operation");
    view.undoStack()->undo();require(view.revealMask(false)==rough,"Undo did not restore rough paint");
    view.undoStack()->redo();require(view.revealMask(false)==refined,"Redo did not restore refinement");
    require(view.saveProject(),"project save failed");
    StereoViewWidget restored;restored.setAutoSave(false);require(restored.loadProject(temp.path()+"/stereo.msk"),"project reload failed");
    require(restored.revealMask(false).convertToFormat(QImage::Format_ARGB32_Premultiplied)==refined.convertToFormat(QImage::Format_ARGB32_Premultiplied),"refined mask lost after reload");
    QFile project(temp.path()+"/stereo.msk");require(project.open(QIODevice::ReadOnly),"read project failed");
    const auto json=QJsonDocument::fromJson(project.readAll()).object();
    require(!json["smartHintsLeftPNG"].toString().isEmpty(),"hints not persisted");
    require(json["smartRegionLeft"].toArray().size()==4,"box not persisted");
    // Swap mapping: painting the right DISPLAY viewport edits the physical left eye.
    view.setSwapSides(true);view.setBrushMode(1);view.setBrushSize(8);
    const QImage rightBefore=view.revealMask(true);click(view,QPoint(726,244));
    require(view.revealMask(true)==rightBefore,"Swap brush edited wrong physical eye");
    require(view.revealMask(false)!=refined,"Swap brush did not paint physical left eye");
    // A stale async result must not overwrite a newer mask.
    view.applySmartHints(false,seed,region);finished=false;view.findEdges();view.applyReveal(false,rough);waitFor(finished);
    require(status.startsWith("Selection changed"),"stale result was not discarded");
    require(view.revealMask(false)==rough,"stale result overwrote newer paint");
    const QString exportFile=temp.path()+"/cutout.png";
    require(StereoViewWidget::saveCutout(exportFile,image,image,frame,refined,QImage(),0,0,0,0),"PNG export failed");
    const QImage exported(exportFile);
    require(qAlpha(exported.pixel(45,90))==0,"export background is opaque");
    require(qAlpha(exported.pixel(108,90))==255,"export lost selected leg");
    require(qAlpha(exported.pixel(108,40))==255,"export lost frame interior");
    // Native event-driven lasso tests: Backspace and toolbar Delete retain previous segments.
    view.setSwapSides(false); view.setBrushMode(7); view.applyReveal(false,QImage());
    click(view,QPoint(120,145)); click(view,QPoint(250,145)); click(view,QPoint(250,260));
    QKeyEvent back(QEvent::KeyPress,Qt::Key_Backspace,Qt::NoModifier);
    QApplication::sendEvent(&view,&back);require(view.hasLasso(),"Backspace erased entire outline");
    click(view,QPoint(240,255)); view.deleteSelectedPoints();require(view.hasLasso(),"Delete action erased entire outline");
    click(view,QPoint(240,255)); click(view,QPoint(120,255));
    const auto beforeLasso=view.revealMask(false);const auto otherEye=view.revealMask(true);const int li=view.undoStack()->index();
    view.closeLasso();view.applyLasso();
    require(!view.hasLasso(),"applied outline was not cleared");
    const QImage lasso=view.revealMask(false);require(!lasso.isNull(),"polygon lasso did not create mask");
    require(qAlpha(lasso.pixel(95,90))>0,"polygon lasso failed to keep interior");
    require(qAlpha(lasso.pixel(220,160))==0,"polygon lasso kept distant exterior");
    require(view.revealMask(true)==otherEye,"lasso modified other eye");
    require(view.undoStack()->index()==li+1,"lasso not a single undo");
    view.undoStack()->undo();require(view.revealMask(false)==beforeLasso,"lasso undo failed");view.undoStack()->redo();
    // Adjust a point by dragging, then replace a sloppy ROI while preserving distant paint.
    view.applyReveal(false,rough); view.applySmartHints(false,seed,region);
    click(view,QPoint(120,145));click(view,QPoint(250,145));click(view,QPoint(250,260));click(view,QPoint(120,260));
    QMouseEvent dp(QEvent::MouseButtonPress,QPointF(250,260),QPointF(250,260),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(&view,&dp);
    QMouseEvent dm(QEvent::MouseMove,QPointF(280,280),QPointF(280,280),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(&view,&dm);
    QMouseEvent dr(QEvent::MouseButtonRelease,QPointF(280,280),QPointF(280,280),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(&view,&dr);
    view.applyLasso(true);const auto replaced=view.revealMask(false);
    require(qAlpha(replaced.pixel(145,133))==0,"dragged point did not enlarge erased polygon");
    require(qAlpha(replaced.pixel(95,90))==0,"Erase lasso retained enclosed reveal");
    require(qAlpha(replaced.pixel(5,5))==qAlpha(rough.pixel(5,5)),"Replace box changed distant paint");
    view.undoStack()->undo();require(view.revealMask(false)==rough,"Replace box undo failed");
    view.applyReveal(false,lasso);
    // Escape discards pending work without changing the painted reveal.
    click(view,QPoint(140,150));QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QApplication::sendEvent(&view,&escape);
    require(!view.hasLasso() && view.revealMask(false)==lasso,"Escape changed committed reveal");
    // Clicking the first anchor closes instead of beginning a drag; inspect status before applying.
    view.applyReveal(false,QImage()); view.fitView(); view.setBrushMode(7);
    click(view,QPoint(120,145));click(view,QPoint(250,145));click(view,QPoint(240,255));click(view,QPoint(120,255));
    click(view,QPoint(120,145));require(status.startsWith("Outline closed"),"first-anchor click did not close outline");
    view.applyLasso();require(view.revealMask(false)==lasso,"first-anchor closure changed polygon mask");
    // A click on the first point of an already closed outline still supports dragging it.
    view.applyReveal(false,QImage());click(view,QPoint(120,145));click(view,QPoint(250,145));click(view,QPoint(240,255));click(view,QPoint(120,255));click(view,QPoint(120,145));
    QMouseEvent fp(QEvent::MouseButtonPress,QPointF(120,145),QPointF(120,145),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(&view,&fp);
    QMouseEvent fm(QEvent::MouseMove,QPointF(90,125),QPointF(90,125),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(&view,&fm);
    QMouseEvent fr(QEvent::MouseButtonRelease,QPointF(90,125),QPointF(90,125),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(&view,&fr);
    view.applyLasso();require(qAlpha(view.revealMask(false).pixel(50,50))>0,"closed first anchor could not be adjusted");
    // Navigate while a lasso is unfinished: source-coordinate geometry must survive.
    view.applyReveal(false,QImage());view.fitView();view.setBrushMode(7);
    click(view,QPoint(120,145));click(view,QPoint(250,145));click(view,QPoint(240,255));click(view,QPoint(120,255));
    QKeyEvent spaceDown(QEvent::KeyPress,Qt::Key_Space,Qt::NoModifier);QApplication::sendEvent(&view,&spaceDown);
    QMouseEvent panPress(QEvent::MouseButtonPress,QPointF(170,180),QPointF(170,180),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(&view,&panPress);
    QMouseEvent panMove(QEvent::MouseMove,QPointF(230,220),QPointF(230,220),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(&view,&panMove);
    QMouseEvent panRelease(QEvent::MouseButtonRelease,QPointF(230,220),QPointF(230,220),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(&view,&panRelease);
    QKeyEvent spaceUp(QEvent::KeyRelease,Qt::Key_Space,Qt::NoModifier);QApplication::sendEvent(&view,&spaceUp);
    QWheelEvent wheel(QPointF(210,210),QPointF(210,210),QPoint(),QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(&view,&wheel);
    require(view.zoom()>1,"wheel did not zoom");require(view.hasLasso(),"pan/zoom discarded unfinished lasso");
    view.fitView();require(view.zoom()==1 && view.hasLasso(),"Fit view lost outline");view.applyLasso();
    require(view.revealMask(false)==lasso,"pan/zoom changed lasso source geometry");
    view.clearPaintedReveal();require(view.revealMask(false).isNull() && view.revealMask(true).isNull(),"clear reveal failed");
    view.undoStack()->undo();require(view.revealMask(false)==lasso && view.revealMask(true)==otherEye,"clear reveal undo failed");
    // Multi-selection regression: closed first contour automatically commits on a new click.
    StereoViewWidget multi;multi.setAutoSave(false);multi.resize(800,400);require(multi.loadImage(file),"multi fixture load failed");multi.fitView();multi.setBrushMode(7);
    click(multi,QPoint(120,145));click(multi,QPoint(250,145));click(multi,QPoint(250,230));click(multi,QPoint(120,230));click(multi,QPoint(120,145));
    require(multi.lassoClosed(),"first multi outline not closed");
    click(multi,QPoint(260,245));
    require(multi.hasLasso() && !multi.lassoClosed(),"new contour could not start after closing first");
    require(qAlpha(multi.revealMask(false).pixel(90,80))==255,"starting new contour lost first mask");
    require(multi.completedLasso(false).contains(QPointF(90,80)),"completed first contour disappeared");
    click(multi,QPoint(320,245));click(multi,QPoint(320,300));click(multi,QPoint(260,300));click(multi,QPoint(260,245));
    QApplication::sendEvent(&multi,&escape);
    require(!multi.hasLasso(),"Escape did not finalize closed contour");
    require(multi.completedLasso(false).contains(QPointF(175,140)) && multi.completedLasso(false).contains(QPointF(90,80)),"two completed outlines not retained");
    const QImage both=multi.revealMask(false);const QPainterPath bothOutline=multi.completedLasso(false);
    multi.setLassoSubtract(true);
    click(multi,QPoint(150,160));click(multi,QPoint(200,160));click(multi,QPoint(200,210));click(multi,QPoint(150,210));multi.closeLasso();multi.finishLasso();
    require(qAlpha(multi.revealMask(false).pixel(100,80))==0,"Subtract did not remove enclosed selection");
    require(qAlpha(multi.revealMask(false).pixel(90,105))==255,"Subtract removed entire previous contour");
    require(qAlpha(multi.revealMask(false).pixel(175,140))==255,"Subtract removed another leg");
    require(!multi.completedLasso(false).contains(QPointF(100,80)),"Subtract boundary still includes removed area");
    multi.undoStack()->undo();require(multi.revealMask(false)==both && multi.completedLasso(false)==bothOutline,"selection undo did not restore mask and outlines together");
    multi.undoStack()->redo();
    require(multi.saveProject(),"multi selection project save failed");
    StereoViewWidget reopen;reopen.setAutoSave(false);require(reopen.loadProject(temp.path()+"/stereo.msk"),"multi project reopen failed");
    require(reopen.completedLasso(false).contains(QPointF(175,140)) && !reopen.completedLasso(false).contains(QPointF(100,80)),"multiple contour persistence lost selection or hole");
    require(reopen.revealMask(false).convertToFormat(QImage::Format_ARGB32_Premultiplied)==multi.revealMask(false).convertToFormat(QImage::Format_ARGB32_Premultiplied),"multiple selection alpha changed after reload");
    const auto retained=multi.completedLasso(false);const auto retainedMask=multi.revealMask(false);
    click(multi,QPoint(330,145));QApplication::sendEvent(&multi,&escape);
    require(multi.completedLasso(false)==retained && multi.revealMask(false)==retainedMask,"Escape unfinished erased completed work");
    multi.setLassoSubtract(false);
    click(multi,QPoint(230,150));click(multi,QPoint(290,150));click(multi,QPoint(290,210));click(multi,QPoint(230,210));multi.closeLasso();multi.finishLasso();
    require(multi.completedLasso(false).contains(QPointF(165,75)),"overlapping Add did not enlarge selection");
    require(multi.revealMask(true).isNull(),"multi-selection changed other eye");
    std::cout<<"Multiple polygon Add/Subtract, closed-Escape retention, contour+alpha undo, holes and project persistence: PASS\n";
    // Magnetic path must bend toward a real edge between its exact clicked endpoints.
    QImage edge(120,100,QImage::Format_RGB32);edge.fill(Qt::black);
    QPainter ep(&edge);ep.fillRect(QRect(0,40,120,60),Qt::white);ep.end();
    const auto trace=magneticLassoPath(edge,QPointF(10,40),QPointF(110,40));
    require(trace.size()>50 && trace.first()==QPointF(10,40) && trace.last()==QPointF(110,40),"magnetic path endpoints failed");
    for(const auto &point:trace) require(std::abs(point.y()-40)<3,"magnetic path left strong edge");
    // Real-photo diagnostic: both physical eyes, several short anchored segments along upper leg/shoe.
    QImage real("/workspace/scratch/edb9f7a24ab9/upload/BelushiAndCheerleaders_InjectZ_SHARP_Parallel_Depth0.060.png");
    if(!real.isNull()) {
        QImage overlay=real.copy(); QPainter rp(&overlay);rp.setPen(QPen(Qt::magenta,1));
        const QVector<QPointF> anchors={QPointF(438,201),QPointF(470,205),QPointF(501,213),QPointF(529,220),QPointF(559,226),QPointF(574,218),QPointF(593,209),QPointF(616,203),QPointF(608,225),QPointF(587,249),QPointF(575,261),QPointF(562,253),QPointF(555,244),QPointF(528,239),QPointF(503,240),QPointF(475,232),QPointF(449,221),QPointF(438,201)};
        for(int eye=0;eye<2;++eye) {
            QImage eyeImage=real.copy(eye*624,0,624,420);
            for(int i=1;i<anchors.size();++i) {
                const auto segment=magneticLassoPath(eyeImage,anchors[i-1],anchors[i]);
                for(int k=1;k<segment.size();++k) rp.drawLine(segment[k-1]+QPointF(eye*624,0),segment[k]+QPointF(eye*624,0));
            }
        }
        rp.end();overlay.save("/tmp/stereomask-r4-real-trace.png");
    }
    std::cout<<"Polygon/magnetic lasso, segment Delete/Backspace, Escape, eye isolation and undo: PASS\n";
    std::cout<<"Native algorithm, async ROI isolation, frame/depth preservation, eye isolation, Swap mapping, Undo/Redo, persistence, stale-job rejection and PNG alpha: PASS\n";
 } catch(const std::exception &e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
 return 0;
}
