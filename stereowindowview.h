#pragma once
#include <cmath>
#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QKeyEvent>

// Full-resolution inspection canvas. Updating depth preserves the inspected area.
class StereoWindowView : public QGraphicsView {
    QGraphicsScene canvas;
    QGraphicsPixmapItem *image;
    bool fitting=true;
public:
    explicit StereoWindowView(QWidget *parent=nullptr):QGraphicsView(parent),canvas(this) {
        setScene(&canvas);image=canvas.addPixmap({});
        setBackgroundBrush(QColor("#202020"));
        setDragMode(QGraphicsView::ScrollHandDrag);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        setResizeAnchor(QGraphicsView::AnchorViewCenter);
        setRenderHint(QPainter::SmoothPixmapTransform);
        setMinimumSize(300,200);
    }
    void setImage(const QImage &value) {
        image->setPixmap(QPixmap::fromImage(value));canvas.setSceneRect(image->boundingRect());
        if(fitting) fitImage();
    }
    void fitImage() {
        fitting=true;resetTransform();
        if(!image->pixmap().isNull()) fitInView(image,Qt::KeepAspectRatio);
    }
    void zoomImage(double factor) {
        const double next=transform().m11()*factor;
        if(next<0.01 || next>64) return;
        fitting=false;scale(factor,factor);
    }
protected:
    void resizeEvent(QResizeEvent *event) override {
        QGraphicsView::resizeEvent(event);if(fitting) fitImage();
    }
    void wheelEvent(QWheelEvent *event) override {
        const double delta=event->angleDelta().isNull() ? event->pixelDelta().y() : event->angleDelta().y();
        if(delta) zoomImage(std::pow(1.2,delta/120.0));
        event->accept();
    }
};
