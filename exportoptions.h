#pragma once
#include <QImage>
#include <QStringList>
#include <QPainter>
namespace ExportOptions {
inline QStringList labels(){return {"Parallel","Cross-eye","Anaglyph — Color","Anaglyph — Half Color","Anaglyph — Grayscale","Left chip","Right chip"};}
inline QStringList suffixes(){return {"Parallel","Crossview","Anaglyph_Color","Anaglyph_HalfColor","Anaglyph_Grayscale","Left","Right"};}
inline QImage output(const QImage &left,const QImage &right,int mode){
 if(mode==5)return left;if(mode==6)return right;
 if(mode<2){QImage result(left.width()*2,left.height(),QImage::Format_ARGB32_Premultiplied);result.fill(Qt::transparent);QPainter p(&result);p.drawImage(0,0,mode==0?left:right);p.drawImage(left.width(),0,mode==0?right:left);return result;}
 QImage l=left.convertToFormat(QImage::Format_ARGB32),r=right.convertToFormat(QImage::Format_ARGB32),result(l.size(),QImage::Format_ARGB32);
 for(int y=0;y<l.height();++y)for(int x=0;x<l.width();++x){QRgb a=l.pixel(x,y),b=r.pixel(x,y);result.setPixel(x,y,qRgba(mode==2?qRed(a):qGray(a),mode==4?qGray(b):qGreen(b),mode==4?qGray(b):qBlue(b),qMax(qAlpha(a),qAlpha(b))));}return result;
}
}
