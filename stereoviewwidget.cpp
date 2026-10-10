#include <QBuffer>
#include "smartselection.h"
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QKeyEvent>
#include <QSaveFile>
#include <QUndoCommand>
#include "stereoviewwidget.h"
#include "undocommand.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QPainterPath>
#include <QDebug>
#include <QWheelEvent>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QtMath>
#include <algorithm>
#include <limits>
#include <numeric>
#include <stack>
#include <vector>

#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

static QPainterPath createImageMaskPath(const QVector<MaskPoint> &pts, const QRectF &v, float scale, bool isRight, bool includeRect)
{
    QPainterPath path;
    path.setFillRule(Qt::OddEvenFill);
    if (pts.isEmpty()) return path;
    if (includeRect) path.addRect(v);

    auto toTarget = [&](const QPointF &pos, float disp) {
        return QPointF(v.left() + (pos.x() - disp) * scale, v.top() + pos.y() * scale);
    };

    for (int i = 0; i < pts.count(); ++i) {
        const auto &p = pts[i];
        QPointF pW = toTarget(p.pos, isRight ? p.disparity : 0);
        if (i == 0) {
            path.moveTo(pW);
        } else {
            const auto &prev = pts[i - 1];
            if (prev.isCurve) {
                path.cubicTo(toTarget(prev.cp1, isRight ? prev.cp1Disparity : 0),
                             toTarget(prev.cp2, isRight ? prev.cp2Disparity : 0),
                             pW);
            } else {
                path.lineTo(pW);
            }
        }
    }

    if (pts.count() > 2) {
        const auto &last = pts.last();
        QPointF startW = toTarget(pts[0].pos, isRight ? pts[0].disparity : 0);
        if (last.isCurve) {
            path.cubicTo(toTarget(last.cp1, isRight ? last.cp1Disparity : 0),
                         toTarget(last.cp2, isRight ? last.cp2Disparity : 0),
                         startW);
        } else {
            path.lineTo(startW);
        }
    }

    path.closeSubpath();
    return path;
}

static void boxBlurAlpha(QImage &image, int radius)
{
    radius = qBound(1, radius, 200);
    const int w = image.width();
    const int h = image.height();
    if (w <= 0 || h <= 0) return;

    QVector<int> horizontal(w * h);
    for (int y = 0; y < h; ++y) {
        const QRgb *line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        int sum = 0;
        for (int x = -radius; x <= radius; ++x) {
            int sx = qBound(0, x, w - 1);
            sum += qAlpha(line[sx]);
        }
        for (int x = 0; x < w; ++x) {
            horizontal[y * w + x] = sum / (radius * 2 + 1);
            int removeX = qBound(0, x - radius, w - 1);
            int addX = qBound(0, x + radius + 1, w - 1);
            sum += qAlpha(line[addX]) - qAlpha(line[removeX]);
        }
    }

    for (int x = 0; x < w; ++x) {
        int sum = 0;
        for (int y = -radius; y <= radius; ++y) {
            int sy = qBound(0, y, h - 1);
            sum += horizontal[sy * w + x];
        }
        for (int y = 0; y < h; ++y) {
            QRgb *line = reinterpret_cast<QRgb*>(image.scanLine(y));
            int alpha = sum / (radius * 2 + 1);
            QColor c = QColor::fromRgba(line[x]);
            c.setAlpha(alpha);
            line[x] = c.rgba();

            int removeY = qBound(0, y - radius, h - 1);
            int addY = qBound(0, y + radius + 1, h - 1);
            sum += horizontal[addY * w + x] - horizontal[removeY * w + x];
        }
    }
}

static void drawFeatheredImagePath(QPainter &painter, const QPainterPath &path, const QColor &color, int feather)
{
    if (feather <= 0) {
        painter.fillPath(path, color);
        return;
    }

    feather = qMin(feather, 200);
    QRectF clip = painter.hasClipping()
        ? painter.clipBoundingRect()
        : QRectF(0, 0, painter.device()->width(), painter.device()->height());
    QRectF br = path.boundingRect().adjusted(-feather * 2, -feather * 2, feather * 2, feather * 2).intersected(clip);

    if (br.width() < 1 || br.height() < 1 || br.width() > 8000 || br.height() > 8000) return;

    QImage img(br.size().toSize(), QImage::Format_ARGB32);
    if (img.isNull()) return;
    img.fill(Qt::transparent);

    QPainter imgPainter(&img);
    imgPainter.setRenderHint(QPainter::Antialiasing);
    imgPainter.translate(-br.topLeft());
    imgPainter.fillPath(path, color);
    imgPainter.end();

    boxBlurAlpha(img, feather);
    painter.drawImage(br.topLeft(), img);
}

StereoViewWidget::StereoViewWidget(QWidget *parent)
    : QWidget(parent), m_panOffset(0, 0), m_isPanning(false), m_panMode(false)
{
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    m_undoStack = new QUndoStack(this);
    m_autoMaskDebugTimer = new QTimer(this);
    m_autoMaskDebugTimer->setInterval(1000);
    connect(m_autoMaskDebugTimer, &QTimer::timeout, this, [this]() {
        if (m_autoMaskDebugFrameIndex + 1 < m_autoMaskDebugFrames.size()) {
            ++m_autoMaskDebugFrameIndex;
        } else {
            m_autoMaskDebugTimer->stop();
            m_autoMaskDebugFrames.clear();
            m_autoMaskDebugFrameIndex = -1;
            if (!m_pendingAutoMaskPoints.isEmpty() && m_points.isEmpty()) {
                QList<int> indices;
                for (int i = 0; i < m_pendingAutoMaskPoints.size(); ++i) indices.append(i);
                m_undoStack->push(new InsertPointsCommand(this, indices, m_pendingAutoMaskPoints, m_pendingAutoMaskText));
                if (m_autoSave) saveProject();
                emit maskEmptyChanged(false);
            }
            m_pendingAutoMaskPoints.clear();
            m_pendingAutoMaskText.clear();
        }
        update();
    });
}

namespace {
struct CvAutoMaskPoint {
    QPointF point;
    float disparity = 0.0f;
};

struct CvAutoMaskResult {
    bool ok = false;
    float disparity = 0.0f;
    QVector<CvAutoMaskPoint> points;
    QString method;
};

static cv::Mat qImageToBgrMat(const QImage &image)
{
    QImage rgb32 = image.convertToFormat(QImage::Format_RGB32);
    cv::Mat bgra(rgb32.height(), rgb32.width(), CV_8UC4,
                 const_cast<uchar*>(rgb32.constBits()),
                 static_cast<size_t>(rgb32.bytesPerLine()));
    cv::Mat bgr;
    cv::cvtColor(bgra, bgr, cv::COLOR_BGRA2BGR);
    return bgr;
}

static QVector<QPointF> polygonFromMaskContour(const cv::Mat &mask, double scale, int maxPoints)
{
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    if (contours.empty()) return {};

    auto largest = std::max_element(contours.begin(), contours.end(), [](const auto &a, const auto &b) {
        return cv::contourArea(a) < cv::contourArea(b);
    });
    if (largest == contours.end() || cv::contourArea(*largest) < 100.0) return {};

    std::vector<cv::Point> approx;
    double epsilon = std::max(2.0, 0.004 * cv::arcLength(*largest, true));
    cv::approxPolyDP(*largest, approx, epsilon, true);

    while ((int)approx.size() > maxPoints && epsilon < 80.0) {
        epsilon *= 1.35;
        cv::approxPolyDP(*largest, approx, epsilon, true);
    }

    QVector<QPointF> polygon;
    polygon.reserve(static_cast<int>(approx.size()));
    for (const cv::Point &p : approx) {
        polygon.append(QPointF(p.x * scale, p.y * scale));
    }
    return polygon;
}

static float medianDisparityNear(const cv::Mat &disp, const QPointF &fullPoint, double scale, float fallback)
{
    const int cx = qBound(0, qRound(fullPoint.x() / scale), disp.cols - 1);
    const int cy = qBound(0, qRound(fullPoint.y() / scale), disp.rows - 1);
    std::vector<float> values;
    values.reserve(81);
    for (int dy = -4; dy <= 4; ++dy) {
        int y = cy + dy;
        if (y < 0 || y >= disp.rows) continue;
        const float *row = disp.ptr<float>(y);
        for (int dx = -4; dx <= 4; ++dx) {
            int x = cx + dx;
            if (x < 0 || x >= disp.cols) continue;
            if (row[x] > 0.0f) values.push_back(row[x]);
        }
    }
    if (values.empty()) return fallback;
    std::nth_element(values.begin(), values.begin() + values.size() / 2, values.end());
    return values[values.size() / 2] * scale;
}

static CvAutoMaskResult computeCvAutoMask(const QImage &leftImage, const QImage &rightImage)
{
    const cv::Mat left = qImageToBgrMat(leftImage);
    const cv::Mat right = qImageToBgrMat(rightImage);

    const int w = left.cols;
    const int h = left.rows;
    const int analysisW = std::min(720, w);
    const int analysisH = std::max(1, qRound(h * (double)analysisW / w));
    const double scale = (double)w / analysisW;

    cv::Mat leftS;
    cv::Mat rightS;
    cv::resize(left, leftS, cv::Size(analysisW, analysisH), 0, 0, cv::INTER_AREA);
    cv::resize(right, rightS, cv::Size(analysisW, analysisH), 0, 0, cv::INTER_AREA);

    cv::Mat grayL;
    cv::Mat grayR;
    cv::cvtColor(leftS, grayL, cv::COLOR_BGR2GRAY);
    cv::cvtColor(rightS, grayR, cv::COLOR_BGR2GRAY);

    int numDisp = std::max(16, ((analysisW / 5) / 16) * 16);
    auto matcherL = cv::StereoSGBM::create(0, numDisp, 5, 8 * 5 * 5, 32 * 5 * 5, 1, 0, 8, 80, 24, cv::StereoSGBM::MODE_SGBM_3WAY);
    auto matcherR = cv::StereoSGBM::create(-numDisp, numDisp, 5, 8 * 5 * 5, 32 * 5 * 5, 1, 0, 8, 80, 24, cv::StereoSGBM::MODE_SGBM_3WAY);

    cv::Mat dispL16;
    cv::Mat dispR16;
    matcherL->compute(grayL, grayR, dispL16);
    matcherR->compute(grayR, grayL, dispR16);
    cv::Mat dispL;
    cv::Mat dispR;
    dispL16.convertTo(dispL, CV_32F, 1.0 / 16.0);
    dispR16.convertTo(dispR, CV_32F, 1.0 / 16.0);

    cv::Mat validMask = cv::Mat::zeros(analysisH, analysisW, CV_8U);
    for (int y = 0; y < analysisH; ++y) {
        const float *dl = dispL.ptr<float>(y);
        uchar *mr = validMask.ptr<uchar>(y);
        for (int x = 0; x < analysisW; ++x) {
            float d = dl[x];
            int xr = qRound(x - d);
            if (d > 0 && xr >= 0 && xr < analysisW && std::abs(d + dispR.at<float>(y, xr)) <= 1.75f) {
                mr[x] = 1;
            }
        }
    }
    cv::morphologyEx(validMask, validMask, cv::MORPH_CLOSE, cv::Mat::ones(9, 9, CV_8U));
    cv::morphologyEx(validMask, validMask, cv::MORPH_OPEN, cv::Mat::ones(5, 5, CV_8U));

    std::vector<float> valid;
    for (int y = 0; y < dispL.rows; ++y) {
        const float *row = dispL.ptr<float>(y);
        for (int x = 0; x < dispL.cols; ++x) {
            if (row[x] > 0) valid.push_back(row[x]);
        }
    }
    float disparity = 0.0f;
    if (!valid.empty()) {
        std::nth_element(valid.begin(), valid.begin() + valid.size() / 2, valid.end());
        disparity = valid[valid.size() / 2] * scale;
    }

    QVector<QPointF> polygon = polygonFromMaskContour(validMask, scale, 28);
    CvAutoMaskResult result;
    result.ok = polygon.size() >= 3;
    result.disparity = disparity;
    result.method = QStringLiteral("sgbm-contour");
    for (const QPointF &point : polygon) {
        result.points.append({point, medianDisparityNear(dispL, point, scale, disparity)});
    }
    return result;
}
}


namespace {
class SmartHintsCommand : public QUndoCommand {
    StereoViewWidget *view; bool right; QImage before, after; QRect beforeRect, afterRect;
public:
    SmartHintsCommand(StereoViewWidget *v, bool r, const QImage &b, const QImage &a, const QRect &br, const QRect &ar)
        : QUndoCommand("Smart selection hints/box"), view(v), right(r), before(b), after(a), beforeRect(br), afterRect(ar) {}
    void undo() override { view->applySmartHints(right, before, beforeRect); }
    void redo() override { view->applySmartHints(right, after, afterRect); }
};
class RevealStrokeCommand : public QUndoCommand {
    StereoViewWidget *view; bool right; QImage before, after;
public:
    RevealStrokeCommand(StereoViewWidget *v, bool r, const QImage &b, const QImage &a)
        : QUndoCommand("Paint outside frame"), view(v), right(r), before(b), after(a) {}
    void undo() override { view->applyReveal(right, before); }
    void redo() override { view->applyReveal(right, after); }
};
class LassoSelectionCommand : public QUndoCommand {
    StereoViewWidget *view; bool right; QImage before,after; QPainterPath beforePath,afterPath;
public:
    LassoSelectionCommand(StereoViewWidget *v,bool r,const QImage &b,const QImage &a,const QPainterPath &bp,const QPainterPath &ap)
        : QUndoCommand("Lasso selection"),view(v),right(r),before(b),after(a),beforePath(bp),afterPath(ap) {}
    void undo() override { view->applyLassoSelection(right,before,beforePath); }
    void redo() override { view->applyLassoSelection(right,after,afterPath); }
};
QJsonObject encodeLassoPath(const QPainterPath &path) {
    QJsonArray polygons;
    for (const auto &polygon:path.toSubpathPolygons()) {
        QJsonArray points;
        for (const auto &p:polygon) points.append(QJsonArray{p.x(),p.y()});
        polygons.append(points);
    }
    return QJsonObject{{"rule",int(path.fillRule())},{"polygons",polygons}};
}
QPainterPath decodeLassoPath(const QJsonObject &obj) {
    QPainterPath path;
    path.setFillRule(obj["rule"].toInt()==int(Qt::WindingFill) ? Qt::WindingFill : Qt::OddEvenFill);
    for (const auto &value:obj["polygons"].toArray()) {
        const auto points=value.toArray(); if (points.size()<3 || points.size()>20000) continue;
        QPolygonF poly;
        for (const auto &item:points) { const auto p=item.toArray(); if(p.size()==2) poly.append(QPointF(p[0].toDouble(),p[1].toDouble())); }
        if(poly.size()>=3) {path.addPolygon(poly);path.closeSubpath();}
    }
    return path;
}
QString encodeReveal(const QImage &img) {
    if (img.isNull()) return {};
    QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG"); return QString::fromLatin1(bytes.toBase64());
}
}
void StereoViewWidget::applyLassoSelection(bool right,const QImage &mask,const QPainterPath &outline) {
    (right ? m_completedRight : m_completedLeft)=outline;
    applyReveal(right,mask);
}
void StereoViewWidget::applyReveal(bool right, const QImage &mask) {
    (right ? m_revealRight : m_revealLeft) = mask;
    ++m_smartRevision;
    update();
    if (m_autoSave) saveProject();
}
void StereoViewWidget::applySmartHints(bool right, const QImage &hints, const QRect &region) {
    (right ? m_smartHintsRight : m_smartHintsLeft) = hints;
    (right ? m_smartRegionRight : m_smartRegionLeft) = region;
    m_smartRight = right; ++m_smartRevision;
    update(); if (m_autoSave) saveProject();
}
void StereoViewWidget::clearSmartHints() {
    if (m_refining || !m_processor.isValid()) return;
    const QImage before = m_smartRight ? m_smartHintsRight : m_smartHintsLeft;
    const QRect region = m_smartRight ? m_smartRegionRight : m_smartRegionLeft;
    if (before.isNull() || region.isEmpty()) return;
    QImage after = before;
    QPainter p(&after); p.setCompositionMode(QPainter::CompositionMode_Clear);
    p.fillRect(region, Qt::transparent); p.end();
    m_undoStack->push(new SmartHintsCommand(this, m_smartRight, before, after, region, region));
    emit smartStatus(tr("Hints cleared in the active box. Your revealed image is unchanged."));
}
void StereoViewWidget::findEdges() {
    if (m_refining || !m_processor.isValid()) return;
    const bool right = m_smartRight;
    const QRect region = right ? m_smartRegionRight : m_smartRegionLeft;
    if (region.isEmpty()) {
        emit smartStatus(tr("Select Smart box, then drag a loose box around one leg in one eye.")); return;
    }
    if (m_points.size() < 3) { emit smartStatus(tr("Set your frame first.")); return; }
    const QImage source = right ? m_processor.rightImage() : m_processor.leftImage();
    const QImage hints = right ? m_smartHintsRight : m_smartHintsLeft;
    const QImage before = revealMask(right);
    const int revision = m_smartRevision;
    m_refining = true; emit smartBusy(true);
    emit smartStatus(tr("Finding edges in the active box…"));
    auto watcher = new QFutureWatcher<SmartSelectionResult>(this);
    connect(watcher, &QFutureWatcher<SmartSelectionResult>::finished, this, [this, watcher, right, region, before, revision, source] {
        const SmartSelectionResult result = watcher->result(); watcher->deleteLater();
        m_refining = false; emit smartBusy(false);
        if (revision != m_smartRevision) { emit smartStatus(tr("Selection changed during processing; result discarded. Click Find edges again.")); return; }
        if (!result.error.isEmpty() || result.mask.isNull()) { emit smartStatus(result.error.isEmpty() ? tr("No edge selection was produced.") : result.error); return; }
        QImage after = before;
        if (after.isNull()) { after = QImage(source.size(), QImage::Format_ARGB32_Premultiplied); after.fill(Qt::transparent); }
        QPainter p(&after); p.setCompositionMode(QPainter::CompositionMode_Source);
        p.drawImage(region.topLeft(), result.mask); p.end();
        m_cutout = true;
        m_undoStack->push(new RevealStrokeCommand(this, right, before, after));
        emit smartStatus(tr("Edges refined. Green is preserved; Keep/Remove strokes can correct it. Undo restores the previous paint."));
    });
    watcher->setFuture(QtConcurrent::run([source, region, hints, before] { return findSmartEdges(source, region, hints, before); }));
}
QImage StereoViewWidget::cutoutEye(const QImage &image, const QVector<MaskPoint> &points, bool right, const QImage &reveal, int background, const QColor &color) {
    QImage alpha(image.size(), QImage::Format_ARGB32_Premultiplied);
    alpha.fill(points.isEmpty() ? Qt::white : Qt::transparent);
    QPainter ap(&alpha); ap.setRenderHint(QPainter::Antialiasing);
    if (!points.isEmpty()) ap.fillPath(createImageMaskPath(points, QRectF(image.rect()), 1.0f, right, false), Qt::white);
    if (!reveal.isNull()) ap.drawImage(0, 0, reveal);
    ap.end();
    QImage eye = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter ep(&eye); ep.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    ep.drawImage(0, 0, alpha); ep.end();
    if (background != 0) {
        QImage solid(image.size(), QImage::Format_ARGB32_Premultiplied);
        solid.fill(background == 1 ? QColor(Qt::black) : background == 2 ? QColor(Qt::white) : color);
        QPainter sp(&solid); sp.drawImage(0, 0, eye); sp.end(); return solid;
    }
    return eye;
}
bool StereoViewWidget::saveCutout(const QString &file, const QImage &left, const QImage &right,
 const QVector<MaskPoint> &points, const QImage &rl, const QImage &rr, int background, int px, int py, int gap, const QColor &color) {
    if (left.isNull() || right.isNull() || left.size() != right.size()) return false;
    QImage result(2*left.width()+2*px+gap, left.height()+2*py, QImage::Format_ARGB32_Premultiplied);
    if (result.isNull()) return false;
    result.fill(background == 0 ? QColor(Qt::transparent) : background == 1 ? QColor(Qt::black) : background == 2 ? QColor(Qt::white) : color);
    QPainter p(&result);
    p.drawImage(px, py, cutoutEye(left, points, false, rl, background, color));
    p.drawImage(px+left.width()+gap, py, cutoutEye(right, points, true, rr, background, color));
    p.end(); return result.save(file);
}
void StereoViewWidget::brushAt(const QPoint &pos) {
    QRect l,r; float scale; calculateLayout(l,r,scale);
    bool right = m_brushRight;
    QRect viewport = (right != m_swapSides) ? r : l;
    if (!viewport.contains(pos)) { m_hasBrushPoint = false; return; }
    QPointF ip = widgetToImage(pos, viewport, scale);
    QImage &mask = m_brushMode >= 4 ? (right ? m_smartHintsRight : m_smartHintsLeft) : (right ? m_revealRight : m_revealLeft);
    if (mask.isNull()) { mask=QImage(m_processor.leftImage().size(), QImage::Format_ARGB32_Premultiplied); mask.fill(Qt::transparent); }
    QPainter p(&mask); p.setRenderHint(QPainter::Antialiasing, m_brushMode < 4);
    if (m_brushMode>=4) p.setClipRect(right ? m_smartRegionRight : m_smartRegionLeft);
    const QColor strokeColor = m_brushMode == 5 ? QColor(255,0,0) : m_brushMode >= 4 ? QColor(0,255,0) : QColor(Qt::white);
    p.setCompositionMode((m_brushMode==2 || m_brushMode==6) ? QPainter::CompositionMode_Clear : QPainter::CompositionMode_SourceOver);
    p.setPen(QPen(strokeColor, m_brushSize, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(m_hasBrushPoint ? m_lastBrushPoint : ip, ip);
    p.setPen(Qt::NoPen); p.setBrush(strokeColor);
    p.drawEllipse(ip, m_brushSize/2.0, m_brushSize/2.0);
    p.end(); m_lastBrushPoint=ip; m_hasBrushPoint=true; update();
}

void StereoViewWidget::loadBrushSettings(const QJsonObject &obj) {
    cancelLasso();
    m_completedLeft=decodeLassoPath(obj["lassoOutlineLeft"].toObject());
    m_completedRight=decodeLassoPath(obj["lassoOutlineRight"].toObject());
    m_lassoSubtract=false;
    auto decodeRect = [&](const char *key) { const QJsonArray a=obj[key].toArray(); return a.size()==4 ? QRect(a[0].toInt(),a[1].toInt(),a[2].toInt(),a[3].toInt()).intersected(m_processor.leftImage().rect()) : QRect(); };
    m_smartRegionLeft=decodeRect("smartRegionLeft"); m_smartRegionRight=decodeRect("smartRegionRight");
    m_smartHintsLeft=QImage::fromData(QByteArray::fromBase64(obj["smartHintsLeftPNG"].toString().toLatin1()), "PNG");
    m_smartHintsRight=QImage::fromData(QByteArray::fromBase64(obj["smartHintsRightPNG"].toString().toLatin1()), "PNG");
    if (m_smartHintsLeft.size()!=m_processor.leftImage().size()) m_smartHintsLeft=QImage();
    if (m_smartHintsRight.size()!=m_processor.rightImage().size()) m_smartHintsRight=QImage();
    m_smartRight=obj["smartActiveRight"].toBool(); ++m_smartRevision;
    m_cutout = obj["cutout"].toBool();
    m_cutoutBackground = qBound(0, obj["cutoutBackground"].toInt(), 3);
    m_revealLeft = QImage::fromData(QByteArray::fromBase64(obj["revealLeftPNG"].toString().toLatin1()), "PNG");
    m_revealRight = QImage::fromData(QByteArray::fromBase64(obj["revealRightPNG"].toString().toLatin1()), "PNG");
    if (m_revealLeft.size() != m_processor.leftImage().size()) m_revealLeft = QImage();
    if (m_revealRight.size() != m_processor.rightImage().size()) m_revealRight = QImage();
    QColor color(obj["cutoutColor"].toString());
    m_cutoutColor = color.isValid() ? color : QColor(Qt::black);
    m_smartRegionLeft=QRect(); m_smartRegionRight=QRect();
    m_smartHintsLeft=QImage(); m_smartHintsRight=QImage();
    m_brushMode = 0; m_pickingColor = false; setCursor(Qt::ArrowCursor);
    emit brushSettingsLoaded(m_brushMode, m_cutoutBackground);
}

bool StereoViewWidget::loadImage(const QString &fileName)
{
    m_lastError = QString();
    if (m_refining) { m_lastError=tr("Wait for Find edges to finish before opening another image."); return false; }
    cancelLasso();
    if (m_processor.loadSideBySide(fileName)) {
        m_completedLeft=QPainterPath(); m_completedRight=QPainterPath(); m_lassoSubtract=false;
        m_smartHintsLeft=QImage(); m_smartHintsRight=QImage(); m_smartRegionLeft=QRect(); m_smartRegionRight=QRect(); m_smartRight=false; ++m_smartRevision;
        m_revealLeft = QImage(); m_revealRight = QImage(); m_cutout = false; m_brushMode = 0; m_pickingColor = false; m_cutoutBackground = 0; m_cutoutColor = Qt::black; setCursor(Qt::ArrowCursor);
        emit brushSettingsLoaded(0, 0);
        m_imagePath = fileName;
        m_panOffset = QPointF(0, 0);
        
        QFileInfo info(fileName);
        QString mskPath = info.absolutePath() + "/" + info.completeBaseName() + ".msk";
        if (QFile::exists(mskPath)) {
            QFile file(mskPath);
            if (file.open(QIODevice::ReadOnly)) {
                QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
                if (!doc.isNull()) {
                    QJsonObject obj = doc.object();
                    loadBrushSettings(obj);
                    QJsonArray pts = obj["points"].toArray();
                    m_undoStack->clear();
                    m_points.clear();
                    m_selectedIndices.clear();
                    for (int i = 0; i < pts.size(); ++i) {
                        QJsonObject pObj = pts[i].toObject();
                        MaskPoint mp;
                        mp.pos = QPointF(pObj["x"].toDouble(), pObj["y"].toDouble());
                        mp.disparity = (float)pObj["d"].toDouble();
                        if (pObj["isCurve"].toBool()) {
                            mp.isCurve = true;
                            mp.cp1 = QPointF(pObj["cp1x"].toDouble(), pObj["cp1y"].toDouble());
                            mp.cp1Disparity = (float)pObj["cp1d"].toDouble();
                            mp.cp2 = QPointF(pObj["cp2x"].toDouble(), pObj["cp2y"].toDouble());
                            mp.cp2Disparity = (float)pObj["cp2d"].toDouble();
                        }
                        m_points.append(mp);
                    }
                }
            }
        } else {
            m_undoStack->clear();
            m_points.clear();
            m_selectedIndices.clear();
        }
        m_zoom = 1.0f;
        updateAnaglyphIfActive();
        notifySelectionState();
        notifyMaskState();
        update();
        return true;
    }
    m_lastError = m_processor.lastError();
    return false;
}

bool StereoViewWidget::saveProject()
{
    if (m_imagePath.isEmpty()) return false;
    QFileInfo info(m_imagePath);
    QString mskPath = info.absolutePath() + "/" + info.completeBaseName() + ".msk";
    QJsonArray pts;
    for (const auto &p : m_points) {
        QJsonObject pObj;
        pObj["x"] = p.pos.x(); pObj["y"] = p.pos.y(); pObj["d"] = p.disparity;
        if (p.isCurve) {
            pObj["isCurve"] = true;
            pObj["cp1x"] = p.cp1.x(); pObj["cp1y"] = p.cp1.y(); pObj["cp1d"] = p.cp1Disparity;
            pObj["cp2x"] = p.cp2.x(); pObj["cp2y"] = p.cp2.y(); pObj["cp2d"] = p.cp2Disparity;
        }
        pts.append(pObj);
    }
    QJsonObject obj; 
    obj["brushVersion"] = 4;
    obj["lassoOutlineLeft"]=encodeLassoPath(m_completedLeft);
    obj["lassoOutlineRight"]=encodeLassoPath(m_completedRight);
    obj["smartHintsLeftPNG"] = encodeReveal(m_smartHintsLeft);
    obj["smartHintsRightPNG"] = encodeReveal(m_smartHintsRight);
    auto rectJson=[](const QRect &r) { return QJsonArray{r.x(),r.y(),r.width(),r.height()}; };
    obj["smartRegionLeft"]=rectJson(m_smartRegionLeft); obj["smartRegionRight"]=rectJson(m_smartRegionRight);
    obj["smartActiveRight"]=m_smartRight;
    obj["cutoutColor"] = m_cutoutColor.name(QColor::HexArgb);
    obj["revealLeftPNG"] = encodeReveal(m_revealLeft);
    obj["revealRightPNG"] = encodeReveal(m_revealRight);
    obj["cutout"] = m_cutout;
    obj["cutoutBackground"] = m_cutoutBackground;
    obj["points"] = pts;
    obj["imagePath"] = m_imagePath;
    
    // Project specific settings
    obj["maskColor"] = m_maskColor.name();
    obj["maskOpacity"] = (double)m_maskOpacity;
    obj["padx"] = m_padx;
    obj["pady"] = m_pady;
    obj["bgColor"] = m_bgColor.name();
    obj["interleavingSpace"] = m_interleavingSpace;
    obj["featherAmount"] = m_featherAmount;

    if (QFile::exists(mskPath) && !QFile::exists(mskPath + ".pre-brush-backup"))
        if (!QFile::copy(mskPath, mskPath + ".pre-brush-backup")) return false;
    QSaveFile file(mskPath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    if (file.write(QJsonDocument(obj).toJson()) < 0) return false;
    return file.commit();
}

bool StereoViewWidget::loadProject(const QString &path)
{
    if (m_refining) { m_lastError=tr("Wait for Find edges to finish before opening another project."); return false; }
    QFileInfo info(path);
    if (info.suffix().toLower() != "msk") {
        return loadImage(path);
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = tr("Could not open file: %1").arg(path);
        return false;
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull()) {
        m_lastError = tr("Invalid JSON in mask file.");
        return false;
    }
    QJsonObject obj = doc.object();
    
    QString storedImgPath = obj["imagePath"].toString();
    bool imgLoaded = false;

    // 1. Try stored path
    if (!storedImgPath.isEmpty() && QFile::exists(storedImgPath)) {
        imgLoaded = loadImage(storedImgPath);
    }

    // 2. Try same folder with common extensions
    if (!imgLoaded) {
        QString base = info.absolutePath() + "/" + info.completeBaseName();
        QStringList extensions = {".jpg", ".jpeg", ".png", ".bmp", ".JPG", ".JPEG", ".PNG", ".BMP"};
        for (const QString &ext : extensions) {
            if (QFile::exists(base + ext)) {
                if (loadImage(base + ext)) {
                    imgLoaded = true;
                    break;
                }
            }
        }
    }

    if (!imgLoaded) {
        m_lastError = tr("Associated image not found for this mask project.");
        return false;
    }

    loadBrushSettings(obj);
    // Load project specific settings if they exist
    if (obj.contains("maskColor")) m_maskColor = QColor(obj["maskColor"].toString());
    if (obj.contains("maskOpacity")) m_maskOpacity = (float)obj["maskOpacity"].toDouble();
    if (obj.contains("padx")) m_padx = obj["padx"].toInt();
    if (obj.contains("pady")) m_pady = obj["pady"].toInt();
    if (obj.contains("bgColor")) m_bgColor = QColor(obj["bgColor"].toString());
    if (obj.contains("interleavingSpace")) m_interleavingSpace = obj["interleavingSpace"].toInt();
    if (obj.contains("featherAmount")) m_featherAmount = obj["featherAmount"].toInt();

    // Reload points from THIS msk file
    QJsonArray pts = obj["points"].toArray();
    m_undoStack->clear();
    m_points.clear();
    m_selectedIndices.clear();
    for (int i = 0; i < pts.size(); ++i) {
        QJsonObject pObj = pts[i].toObject();
        MaskPoint mp;
        mp.pos = QPointF(pObj["x"].toDouble(), pObj["y"].toDouble());
        mp.disparity = (float)pObj["d"].toDouble();
        if (pObj["isCurve"].toBool()) {
            mp.isCurve = true;
            mp.cp1 = QPointF(pObj["cp1x"].toDouble(), pObj["cp1y"].toDouble());
            mp.cp1Disparity = (float)pObj["cp1d"].toDouble();
            mp.cp2 = QPointF(pObj["cp2x"].toDouble(), pObj["cp2y"].toDouble());
            mp.cp2Disparity = (float)pObj["cp2d"].toDouble();
        }
        m_points.append(mp);
    }
    m_zoom = 1.0f;
    m_panOffset = QPointF(0, 0);
    notifySelectionState();
    notifyMaskState();
    update();
    return true;
}

void StereoViewWidget::drawFeatheredPath(QPainter &painter, const QPainterPath &path, const QColor &color, int feather)
{
    drawFeatheredImagePath(painter, path, color, feather);
}

bool StereoViewWidget::saveImage(const QString &fileName, const QColor &maskColor, float opacity, int padx, int pady, const QColor &bgColor, int interleavingSpace, std::function<void(int)> progressCallback)
{
    return saveImageData(fileName, m_processor.leftImage(), m_processor.rightImage(), m_points, maskColor, opacity,
                         padx, pady, bgColor, interleavingSpace, m_featherAmount, progressCallback);
}

bool StereoViewWidget::saveImageData(const QString &fileName, const QImage &leftImage, const QImage &rightImage, const QVector<MaskPoint> &points, const QColor &maskColor, float opacity, int padx, int pady, const QColor &bgColor, int interleavingSpace, int featherAmount, std::function<void(int)> progressCallback)
{
    if (leftImage.isNull() || rightImage.isNull() || leftImage.size() != rightImage.size()) return false;
    int iw = leftImage.width();
    int ih = leftImage.height();
    
    int totalW = (iw * 2) + (padx * 2) + interleavingSpace;
    int totalH = ih + (pady * 2);
    
    QImage result(totalW, totalH, QImage::Format_ARGB32);
    if (result.isNull()) return false;
    result.fill(bgColor);
    
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);

    if (progressCallback) progressCallback(10);

    auto drawEye = [&](const QImage &img, int offsetX, bool isRight) {
        QRect target(offsetX, pady, iw, ih);
        painter.drawImage(target, img);
        
        if (!points.isEmpty()) {
            QImage maskImg(target.size(), QImage::Format_ARGB32);
            maskImg.fill(Qt::transparent);
            
            QPainter maskPainter(&maskImg);
            maskPainter.setRenderHint(QPainter::Antialiasing);
            
            QColor fill = maskColor;
            fill.setAlphaF(opacity);
            maskPainter.fillRect(maskImg.rect(), fill);
            
            QPainterPath polyPath = createImageMaskPath(points, QRectF(0, 0, iw, ih), 1.0f, isRight, false);
            
            maskPainter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
            drawFeatheredImagePath(maskPainter, polyPath, Qt::black, featherAmount);
            maskPainter.end();
            
            painter.drawImage(target.topLeft(), maskImg);
        }
    };

    drawEye(leftImage, padx, false);
    if (progressCallback) progressCallback(50);
    
    drawEye(rightImage, padx + iw + interleavingSpace, true);
    if (progressCallback) progressCallback(90);
    
    painter.end();
    if (!result.save(fileName)) return false;
    if (progressCallback) progressCallback(100);
    return true;
}

void StereoViewWidget::setAnaglyphMode(bool enabled) { 
    m_anaglyphMode = enabled; 
    updateAnaglyphIfActive();
    update(); 
}
void StereoViewWidget::setSwapSides(bool enabled) { 
    m_swapSides = enabled; 
    updateAnaglyphIfActive();
    update(); 
}

void StereoViewWidget::updateAnaglyphIfActive()
{
    if (m_anaglyphMode && m_processor.isValid()) {
        m_processor.createAnaglyph(m_swapSides, &m_points, m_maskColor, m_maskOpacity);
    }
}

void StereoViewWidget::setZoom(float zoom) { m_zoom = qBound(0.1f, zoom, 20.0f); update(); }

bool StereoViewWidget::hasCurveSelection() const
{
    for (int idx : m_selectedIndices) {
        if (idx >= 0 && idx < m_points.count() && m_points[idx].isCurve) {
            return true;
        }
    }
    return false;
}

void StereoViewWidget::notifySelectionState()
{
    emit curveSelectionChanged(hasCurveSelection());
}

void StereoViewWidget::notifyMaskState()
{
    emit maskEmptyChanged(m_points.isEmpty());
}

void StereoViewWidget::setFreehandMode(bool enabled)
{
    m_freehandMode = enabled && m_processor.isValid() && m_points.isEmpty();
    if (!m_freehandMode) {
        m_isFreehandDrawing = false;
        m_freehandPoints.clear();
    }
    setCursor(m_freehandMode ? Qt::CrossCursor : (m_panMode ? Qt::OpenHandCursor : Qt::ArrowCursor));
    emit freehandModeChanged(m_freehandMode);
    update();
}

double StereoViewWidget::pointLineDistance(const QPointF &point, const QPointF &lineStart, const QPointF &lineEnd) const
{
    QPointF segment = lineEnd - lineStart;
    double lengthSquared = QPointF::dotProduct(segment, segment);
    if (lengthSquared <= 0.0001) return QLineF(point, lineStart).length();
    double t = QPointF::dotProduct(point - lineStart, segment) / lengthSquared;
    t = qBound(0.0, t, 1.0);
    return QLineF(point, lineStart + segment * t).length();
}

QVector<QPointF> StereoViewWidget::simplifyFreehandPath(const QVector<QPointF> &points, double tolerance) const
{
    if (points.size() < 3) return points;
    double maxDistance = 0.0;
    int splitIndex = 0;
    for (int i = 1; i < points.size() - 1; ++i) {
        double distance = pointLineDistance(points[i], points.first(), points.last());
        if (distance > maxDistance) {
            maxDistance = distance;
            splitIndex = i;
        }
    }
    if (maxDistance <= tolerance) return {points.first(), points.last()};
    QVector<QPointF> left = simplifyFreehandPath(points.mid(0, splitIndex + 1), tolerance);
    QVector<QPointF> right = simplifyFreehandPath(points.mid(splitIndex), tolerance);
    if (!left.isEmpty()) left.removeLast();
    left += right;
    return left;
}

int StereoViewWidget::edgeStrengthAt(const QImage &image, int x, int y) const
{
    if (x <= 0 || y <= 0 || x >= image.width() - 1 || y >= image.height() - 1) return 0;
    auto gray = [&](int px, int py) { return qGray(image.pixel(px, py)); };
    int gx = -gray(x - 1, y - 1) + gray(x + 1, y - 1)
             -2 * gray(x - 1, y) + 2 * gray(x + 1, y)
             -gray(x - 1, y + 1) + gray(x + 1, y + 1);
    int gy = -gray(x - 1, y - 1) - 2 * gray(x, y - 1) - gray(x + 1, y - 1)
             +gray(x - 1, y + 1) + 2 * gray(x, y + 1) + gray(x + 1, y + 1);
    return qAbs(gx) + qAbs(gy);
}

QVector<QPointF> StereoViewWidget::snapFreehandPathToEdges(const QVector<QPointF> &points) const
{
    return points;
}

QVector<QPointF> StereoViewWidget::smoothFreehandPath(const QVector<QPointF> &points) const
{
    if (points.size() < 5) return points;
    QVector<QPointF> smoothed;
    smoothed.reserve(points.size());
    for (int i = 0; i < points.size(); ++i) {
        const QPointF prev = points[(i - 1 + points.size()) % points.size()];
        const QPointF current = points[i];
        const QPointF next = points[(i + 1) % points.size()];
        smoothed.append(prev * 0.2 + current * 0.6 + next * 0.2);
    }
    return smoothed;
}

QVector<QPointF> StereoViewWidget::limitFreehandPointCount(const QVector<QPointF> &points, int maxPoints) const
{
    if (points.size() <= maxPoints || maxPoints < 3) return points;
    QVector<QPointF> limited;
    limited.reserve(maxPoints);
    for (int i = 0; i < maxPoints; ++i) {
        int index = qRound((double)i * (points.size() - 1) / (maxPoints - 1));
        limited.append(points[index]);
    }
    return limited;
}

int StereoViewWidget::estimateGlobalDisparity() const
{
    if (!m_processor.isValid()) return 0;
    const QImage left = m_processor.leftImage().scaledToWidth(320, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB32);
    const QImage right = m_processor.rightImage().scaled(left.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB32);
    const int maxShift = qMax(8, left.width() / 6);
    double bestScore = std::numeric_limits<double>::max();
    int bestShift = 0;
    for (int shift = -maxShift; shift <= maxShift; ++shift) {
        int xStart = qMax(0, shift);
        int xEnd = qMin(left.width(), left.width() + shift);
        if (xEnd - xStart < left.width() / 2) continue;
        qint64 sad = 0;
        int samples = 0;
        for (int y = 0; y < left.height(); y += 3) {
            const QRgb *leftLine = reinterpret_cast<const QRgb*>(left.constScanLine(y));
            const QRgb *rightLine = reinterpret_cast<const QRgb*>(right.constScanLine(y));
            for (int x = xStart; x < xEnd; x += 3) {
                sad += qAbs(qGray(leftLine[x]) - qGray(rightLine[x - shift]));
                ++samples;
            }
        }
        if (samples > 0 && (double)sad / samples < bestScore) {
            bestScore = (double)sad / samples;
            bestShift = shift;
        }
    }
    return qRound(bestShift * (double)m_processor.leftImage().width() / left.width());
}

bool StereoViewWidget::createAutoMaskPoints()
{
    if (!canAutoMask()) return false;
    if (m_autoMaskDebugTimer) m_autoMaskDebugTimer->stop();
    m_autoMaskDebugFrames.clear();
    m_pendingAutoMaskPoints.clear();
    m_pendingAutoMaskText.clear();
    m_autoMaskDebugFrameIndex = -1;

    const int iw = m_processor.leftImage().width();
    const int ih = m_processor.leftImage().height();
    if (iw <= 1 || ih <= 1) return false;

    CvAutoMaskResult cvMask;
    try {
        cvMask = computeCvAutoMask(m_processor.leftImage(), m_processor.rightImage());
    } catch (const cv::Exception &e) {
        qWarning() << "OpenCV auto mask failed:" << e.what();
    }
    if (cvMask.ok && cvMask.points.size() >= 3) {
        const double border = qMax(4.0, qMin(iw, ih) * 0.015);
        auto clampBoth = [&](QPointF p, float pointDisparity) {
            double minX = qMax(border, border + pointDisparity);
            double maxX = qMin(iw - border, iw - border + pointDisparity);
            if (minX > maxX) minX = maxX = qBound(border, iw / 2.0 + pointDisparity * 0.5, iw - border);
            p.setX(qBound(minX, p.x(), maxX));
            p.setY(qBound(border, p.y(), ih - border));
            return p;
        };

        QVector<MaskPoint> autoPoints;
        QList<int> indices;
        QVector<QPointF> debugPolygon;
        for (const CvAutoMaskPoint &point : cvMask.points) {
            QPointF clamped = clampBoth(point.point, point.disparity);
            debugPolygon.append(clamped);
            autoPoints.append({clamped, point.disparity});
            indices.append(autoPoints.size() - 1);
        }

        if (autoPoints.size() >= 3) {
            m_selectedIndices.clear();
            m_selectedPointIndex = -1;
            m_undoStack->push(new InsertPointsCommand(this, indices, autoPoints, tr("OpenCV Auto Mask")));
            if (m_autoSave) saveProject();
            notifySelectionState();
            notifyMaskState();
            update();
            return true;
        }
    }

    const int disparity = estimateGlobalDisparity();
    const int analysisW = qMin(480, iw);
    const int analysisH = qMax(1, qRound(ih * (double)analysisW / iw));
    const double scaleToFull = (double)iw / analysisW;
    const QImage left = m_processor.leftImage().scaled(analysisW, analysisH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB32);
    const QImage right = m_processor.rightImage().scaled(analysisW, analysisH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB32);
    const int shift = qRound(disparity / scaleToFull);

    QVector<uchar> coverage(analysisW * analysisH, 0);
    QVector<double> scores;
    scores.reserve(analysisW * analysisH / 4);
    for (int y = 0; y < analysisH; y += 2) {
        const QRgb *ll = reinterpret_cast<const QRgb*>(left.constScanLine(y));
        const QRgb *rr = reinterpret_cast<const QRgb*>(right.constScanLine(y));
        for (int x = 0; x < analysisW; x += 2) {
            int rx = x - shift;
            if (rx < 0 || rx >= analysisW) continue;
            scores.append(qAbs(qGray(ll[x]) - qGray(rr[rx])));
        }
    }
    if (scores.isEmpty()) return false;
    std::sort(scores.begin(), scores.end());
    const double threshold = qBound(18.0, scores[scores.size() / 2] * 1.6 + 10.0, 64.0);
    for (int y = 0; y < analysisH; ++y) {
        const QRgb *ll = reinterpret_cast<const QRgb*>(left.constScanLine(y));
        const QRgb *rr = reinterpret_cast<const QRgb*>(right.constScanLine(y));
        for (int x = 0; x < analysisW; ++x) {
            int rx = x - shift;
            if (rx < 0 || rx >= analysisW) continue;
            if (qAbs(qGray(ll[x]) - qGray(rr[rx])) <= threshold) coverage[y * analysisW + x] = 1;
        }
    }

    struct RectI { int x = 0; int y = 0; int w = 0; int h = 0; int area() const { return w * h; } };
    auto largestRectInMask = [&](const QVector<uchar> &mask, int w, int h) {
        QVector<int> heights(w, 0);
        RectI best;
        int bestArea = 0;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) heights[x] = mask[y * w + x] ? heights[x] + 1 : 0;
            std::stack<int> stack;
            for (int x = 0; x <= w; ++x) {
                int currentHeight = (x == w) ? 0 : heights[x];
                while (!stack.empty() && heights[stack.top()] > currentHeight) {
                    int height = heights[stack.top()];
                    stack.pop();
                    int width = stack.empty() ? x : x - stack.top() - 1;
                    int area = height * width;
                    if (area > bestArea) {
                        int leftX = stack.empty() ? 0 : stack.top() + 1;
                        best = {leftX, y - height + 1, width, height};
                        bestArea = area;
                    }
                }
                stack.push(x);
            }
        }
        return best;
    };
    auto transposeMask = [](const QVector<uchar> &mask, int w, int h) {
        QVector<uchar> out(w * h, 0);
        for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) out[x * h + y] = mask[y * w + x];
        return out;
    };
    auto clearRect = [](QVector<uchar> &mask, int w, const RectI &r) {
        for (int y = r.y; y < r.y + r.h; ++y) for (int x = r.x; x < r.x + r.w; ++x) mask[y * w + x] = 0;
    };
    auto toPoly = [&](const RectI &r) {
        QRectF rf(r.x * scaleToFull, r.y * scaleToFull, r.w * scaleToFull, r.h * scaleToFull);
        return QVector<QPointF>{rf.topLeft(), rf.bottomLeft(), rf.bottomRight(), rf.topRight()};
    };

    QVector<RectI> tiles;
    QVector<uchar> work = coverage;
    RectI seed = largestRectInMask(work, analysisW, analysisH);
    const int minArea = qMax(300, analysisW * analysisH / 350);
    if (seed.area() < minArea) return false;
    tiles.append(seed);
    clearRect(work, analysisW, seed);
    m_autoMaskDebugFrames.append({{}, {}, {toPoly(seed)}, {(float)disparity}, (float)disparity, tr("Seed rectangle")});

    bool horizontal = true;
    for (int iter = 0; iter < 50; ++iter) {
        RectI candidate;
        if (horizontal) {
            QVector<uchar> t = transposeMask(work, analysisW, analysisH);
            RectI tr = largestRectInMask(t, analysisH, analysisW);
            candidate = {tr.y, tr.x, tr.h, tr.w};
        } else {
            candidate = largestRectInMask(work, analysisW, analysisH);
        }
        if (candidate.area() < minArea) break;
        tiles.append(candidate);
        clearRect(work, analysisW, candidate);
        QVector<QVector<QPointF>> framePolys;
        QVector<float> frameDisps;
        for (const RectI &tile : tiles) {
            framePolys.append(toPoly(tile));
            frameDisps.append((float)disparity);
        }
        m_autoMaskDebugFrames.append({{}, {}, framePolys, frameDisps, (float)disparity,
                                      tr("%1 rectangle %2").arg(horizontal ? tr("Horizontal") : tr("Vertical")).arg(iter + 2)});
        horizontal = !horizontal;
    }

    QVector<double> ySamples;
    for (const RectI &tile : tiles) {
        ySamples << tile.y * scaleToFull << (tile.y + tile.h * 0.5) * scaleToFull << (tile.y + tile.h) * scaleToFull;
    }
    std::sort(ySamples.begin(), ySamples.end());
    QVector<QPointF> leftBoundary;
    QVector<QPointF> rightBoundary;
    for (double y : ySamples) {
        double leftX = iw;
        double rightX = 0;
        bool any = false;
        double ay = y / scaleToFull;
        for (const RectI &tile : tiles) {
            if (ay < tile.y || ay > tile.y + tile.h) continue;
            leftX = qMin(leftX, tile.x * scaleToFull);
            rightX = qMax(rightX, (tile.x + tile.w) * scaleToFull);
            any = true;
        }
        if (any && rightX > leftX) {
            leftBoundary.append(QPointF(leftX, y));
            rightBoundary.append(QPointF(rightX, y));
        }
    }
    QVector<QPointF> polygon = leftBoundary;
    for (int i = rightBoundary.size() - 1; i >= 0; --i) polygon.append(rightBoundary[i]);
    polygon = limitFreehandPointCount(polygon, 18);
    if (polygon.size() < 4) return false;

    const double border = qMax(4.0, qMin(iw, ih) * 0.015);
    auto clampBoth = [&](QPointF p) {
        double minX = qMax(border, border + disparity);
        double maxX = qMin(iw - border, iw - border + disparity);
        if (minX > maxX) minX = maxX = qBound(border, iw / 2.0 + disparity * 0.5, iw - border);
        p.setX(qBound(minX, p.x(), maxX));
        p.setY(qBound(border, p.y(), ih - border));
        return p;
    };
    QVector<MaskPoint> autoPoints;
    QList<int> indices;
    for (int i = 0; i < polygon.size(); ++i) {
        autoPoints.append({clampBoth(polygon[i]), (float)disparity});
        indices.append(i);
    }
    m_pendingAutoMaskPoints = autoPoints;
    m_pendingAutoMaskText = tr("Smart Auto Mask");
    m_autoMaskDebugFrames.append({{}, polygon, {}, {}, (float)disparity, tr("Final editable mask")});
    startAutoMaskDebugPlayback();
    return true;
}

void StereoViewWidget::appendFreehandPoint(const QPoint &widgetPos)
{
    QPointF imagePos = widgetToImage(widgetPos, m_freehandViewport, m_freehandScale);
    if (imagePos.x() < 0 || imagePos.y() < 0 || imagePos.x() > m_processor.leftImage().width() || imagePos.y() > m_processor.leftImage().height()) return;
    if (m_freehandPoints.isEmpty() || QLineF(m_freehandPoints.last(), imagePos).length() >= 5.0) {
        m_freehandPoints.append(imagePos);
        update();
    }
}

void StereoViewWidget::finishFreehandDrawing()
{
    m_isFreehandDrawing = false;
    QVector<QPointF> simplified = limitFreehandPointCount(simplifyFreehandPath(smoothFreehandPath(m_freehandPoints), 10.0), 40);
    m_freehandPoints.clear();
    if (simplified.size() < 3) { update(); return; }
    QVector<MaskPoint> newPoints;
    QList<int> indices;
    for (int i = 0; i < simplified.size(); ++i) {
        newPoints.append({simplified[i], 0});
        indices.append(i);
    }
    m_undoStack->push(new InsertPointsCommand(this, indices, newPoints, tr("Freehand Mask")));
    setFreehandMode(false);
    if (m_autoSave) saveProject();
}

void StereoViewWidget::startAutoMaskDebugPlayback()
{
    if (m_autoMaskDebugFrames.isEmpty() || !m_autoMaskDebugTimer) return;
    m_autoMaskDebugFrameIndex = 0;
    m_autoMaskDebugTimer->start();
    update();
}

void StereoViewWidget::drawAutoMaskDebugOverlay(QPainter &painter, const QRect &viewport, float scale, bool isRight)
{
    if (m_autoMaskDebugFrameIndex < 0 || m_autoMaskDebugFrameIndex >= m_autoMaskDebugFrames.size()) return;
    const AutoMaskDebugFrame &frame = m_autoMaskDebugFrames[m_autoMaskDebugFrameIndex];
    QVector<QVector<QPointF>> polygons = frame.imagePolygons;
    if (polygons.isEmpty() && !frame.imagePolygon.isEmpty()) polygons.append(frame.imagePolygon);
    if (polygons.isEmpty() && !frame.imageRect.isEmpty()) {
        const QRectF &r = frame.imageRect;
        polygons.append({r.topLeft(), r.bottomLeft(), r.bottomRight(), r.topRight()});
    }
    painter.save();
    painter.setClipRect(viewport);
    painter.setRenderHint(QPainter::Antialiasing);
    QPointF labelPoint;
    bool hasLabel = false;
    for (int pi = 0; pi < polygons.size(); ++pi) {
        const QVector<QPointF> &poly = polygons[pi];
        if (poly.size() < 3) continue;
        float d = pi < frame.polygonDisparities.size() ? frame.polygonDisparities[pi] : frame.disparity;
        QPainterPath path;
        QPointF first = imageToWidget(poly.first(), viewport, scale, isRight ? d : 0);
        path.moveTo(first);
        if (!hasLabel) { labelPoint = first; hasLabel = true; }
        for (int i = 1; i < poly.size(); ++i) path.lineTo(imageToWidget(poly[i], viewport, scale, isRight ? d : 0));
        path.closeSubpath();
        painter.fillPath(path, QColor(34, 197, 94, 42));
        painter.setPen(QPen(QColor("#22c55e"), 2));
        painter.drawPath(path);
    }
    if (hasLabel) {
        QRectF labelRect(labelPoint + QPointF(8, 8), QSizeF(250, 24));
        labelRect = labelRect.intersected(QRectF(viewport).adjusted(4, 4, -4, -4));
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 180));
        painter.drawRoundedRect(labelRect, 3, 3);
        painter.setPen(Qt::white);
        painter.drawText(labelRect.adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft,
                         tr("Auto Mask: %1/%2  %3").arg(m_autoMaskDebugFrameIndex + 1).arg(m_autoMaskDebugFrames.size()).arg(frame.label));
    }
    painter.restore();
}

void StereoViewWidget::addPoint(const QPointF &pos, float disparity, int index)
{
    m_undoStack->push(new AddPointCommand(this, {pos, disparity}, index));
    updateAnaglyphIfActive();
    notifySelectionState();
    notifyMaskState();
    if (m_autoSave) saveProject();
}

void StereoViewWidget::deleteSelectedPoints()
{
    if (m_brushMode >= 7) { removeLassoSegment(); return; }
    if (m_selectedIndices.isEmpty()) return;
    QList<int> sortedIndices = m_selectedIndices;
    std::sort(sortedIndices.begin(), sortedIndices.end(), std::greater<int>());
    QVector<MaskPoint> removedPoints;
    for (int idx : sortedIndices) removedPoints.append(m_points[idx]);
    m_undoStack->push(new DeletePointsCommand(this, sortedIndices, removedPoints));
    m_selectedIndices.clear(); m_selectedPointIndex = -1;
    updateAnaglyphIfActive();
    notifySelectionState();
    notifyMaskState();
    if (m_autoSave) saveProject();
}

void StereoViewWidget::updatePointInternal(int index, const MaskPoint &p) { if (index >= 0 && index < m_points.count()) { m_points[index] = p; updateAnaglyphIfActive(); notifySelectionState(); notifyMaskState(); update(); } }
void StereoViewWidget::updatePointsInternal(const QList<int> &indices, const QVector<MaskPoint> &ps) { for (int i = 0; i < indices.count(); ++i) { if (indices[i] >= 0 && indices[i] < m_points.count()) m_points[indices[i]] = ps[i]; } updateAnaglyphIfActive(); notifySelectionState(); notifyMaskState(); update(); }
void StereoViewWidget::addPointInternal(const MaskPoint &p, int index) { if (index >= 0 && index <= m_points.count()) m_points.insert(index, p); else m_points.append(p); updateAnaglyphIfActive(); notifySelectionState(); notifyMaskState(); update(); }
void StereoViewWidget::removePointInternal(int index) { if (!m_points.isEmpty()) { if (index >= 0 && index < m_points.count()) m_points.removeAt(index); else m_points.removeLast(); } updateAnaglyphIfActive(); notifySelectionState(); notifyMaskState(); update(); }
void StereoViewWidget::insertPointsInternal(const QList<int> &indices, const QVector<MaskPoint> &ps) { for (int i = 0; i < indices.count(); ++i) m_points.insert(indices[i], ps[i]); updateAnaglyphIfActive(); notifySelectionState(); notifyMaskState(); update(); }
void StereoViewWidget::removePointsInternal(const QList<int> &indices) { for (int idx : indices) { if (idx >= 0 && idx < m_points.count()) m_points.removeAt(idx); } updateAnaglyphIfActive(); notifySelectionState(); notifyMaskState(); update(); }

void StereoViewWidget::clearPoints() { m_points.clear(); m_selectedIndices.clear(); m_undoStack->clear(); updateAnaglyphIfActive(); notifySelectionState(); notifyMaskState(); update(); if (m_autoSave) saveProject(); }

void StereoViewWidget::alignSelectedPoints(AlignSide side)
{
    if (m_brushMode >= 7) { removeLassoSegment(); return; }
    if (m_selectedIndices.isEmpty()) return;
    int pivotIdx = (m_selectedPointIndex != -1) ? m_selectedPointIndex : m_selectedIndices.first();
    float targetVal = (side == AlignLeft || side == AlignRight) ? m_points[pivotIdx].pos.x() : m_points[pivotIdx].pos.y();
    float targetDisp = m_points[pivotIdx].disparity;
    
    QVector<MaskPoint> oldPs, newPs;
    for (int idx : m_selectedIndices) {
        oldPs << m_points[idx];
        MaskPoint p = m_points[idx];
        if (side == AlignLeft || side == AlignRight) {
            p.pos.setX(targetVal);
            if (p.isCurve) { p.cp1.setX(targetVal); p.cp2.setX(targetVal); }
        } else if (side == AlignTop || side == AlignBottom) {
            p.pos.setY(targetVal);
            if (p.isCurve) { p.cp1.setY(targetVal); p.cp2.setY(targetVal); }
        } else if (side == AlignDepth) {
            p.disparity = targetDisp;
            if (p.isCurve) { p.cp1Disparity = targetDisp; p.cp2Disparity = targetDisp; }
        }
        newPs << p;
    }
    m_undoStack->push(new BatchMoveCommand(this, m_selectedIndices, oldPs, newPs));
    updateAnaglyphIfActive();
    notifySelectionState();
    if (m_autoSave) saveProject();
}

void StereoViewWidget::calculateLayout(QRect &rL, QRect &rR, float &scale)
{
    if (!m_processor.isValid()) return;
    int iw = m_processor.leftImage().width();
    int ih = m_processor.leftImage().height();
    
    if (m_anaglyphMode) {
        float s = qMin((float)(width() - 40) / iw, (float)(height() - 40) / ih);
        rL = QRect((width() - iw*s)/2, (height() - ih*s)/2, iw*s, ih*s);
        rR = rL;
        scale = s * m_zoom;
    } else {
        float s = qMin((float)(width()/2.0 - 40) / iw, (float)(height() - 40) / ih);
        rL = QRect((width()/4.0 - (iw*s)/2.0), (height() - (ih*s))/2.0, iw*s, ih*s);
        rR = QRect((3.0*width()/4.0 - (iw*s)/2.0), (height() - (ih*s))/2.0, iw*s, ih*s);
        scale = s * m_zoom;
    }
}

QPointF StereoViewWidget::imageToWidget(const QPointF &pos, const QRect &v, float scale, float disparity)
{
    float sw = m_processor.leftImage().width() * scale;
    float sh = m_processor.leftImage().height() * scale;
    float left = v.center().x() - sw/2.0 + m_panOffset.x()*scale;
    float top = v.center().y() - sh/2.0 + m_panOffset.y()*scale;
    return QPointF(left + (pos.x() - disparity) * scale, top + pos.y() * scale);
}

QPointF StereoViewWidget::widgetToImage(const QPoint &pos, const QRect &v, float scale) 
{
    float sw = m_processor.leftImage().width() * scale;
    float sh = m_processor.leftImage().height() * scale;
    float left = v.center().x() - sw/2.0 + m_panOffset.x()*scale;
    float top = v.center().y() - sh/2.0 + m_panOffset.y()*scale;
    return QPointF((pos.x() - left) / scale, (pos.y() - top) / scale);
}

QPainterPath StereoViewWidget::createMaskPath(const QVector<MaskPoint> &pts, const QRectF &v, float scale, bool isRight, bool includeRect, bool ignorePan)
{
    QPainterPath path;
    path.setFillRule(Qt::OddEvenFill);
    if (pts.isEmpty()) return path;
    if (includeRect) path.addRect(v);

    auto toWidget = [&](const QPointF &pos, float disp) {
        if (ignorePan) {
            return QPointF(v.left() + (pos.x() - disp) * scale, v.top() + pos.y() * scale);
        } else {
            return imageToWidget(pos, v.toRect(), scale, disp);
        }
    };

    for (int i = 0; i < pts.count(); ++i) {
        const auto &p = pts[i];
        QPointF pW = toWidget(p.pos, isRight ? p.disparity : 0);
        
        if (i == 0) {
            path.moveTo(pW);
        } else {
            const auto &prev = pts[i-1];
            if (prev.isCurve) {
                QPointF cp1W = toWidget(prev.cp1, isRight ? prev.cp1Disparity : 0);
                QPointF cp2W = toWidget(prev.cp2, isRight ? prev.cp2Disparity : 0);
                path.cubicTo(cp1W, cp2W, pW);
            } else {
                path.lineTo(pW);
            }
        }
    }
    
    // Close path with last segment
    if (pts.count() > 2) {
        const auto &last = pts.last();
        QPointF startW = toWidget(pts[0].pos, isRight ? pts[0].disparity : 0);
        if (last.isCurve) {
            QPointF cp1W = toWidget(last.cp1, isRight ? last.cp1Disparity : 0);
            QPointF cp2W = toWidget(last.cp2, isRight ? last.cp2Disparity : 0);
            path.cubicTo(cp1W, cp2W, startW);
        } else {
            path.lineTo(startW);
        }
    }

    path.closeSubpath();
    return path;
}

void StereoViewWidget::toggleCurve()
{
    if (m_brushMode >= 7) { removeLassoSegment(); return; }
    if (m_selectedIndices.isEmpty()) return;
    QVector<MaskPoint> oldPs, newPs;
    for (int idx : m_selectedIndices) {
        oldPs << m_points[idx];
        MaskPoint p = m_points[idx];
        p.isCurve = !p.isCurve;
        if (p.isCurve) {
            // Initialize handles between this point and the next
            int nextIdx = (idx + 1) % m_points.count();
            p.cp1 = p.pos + (m_points[nextIdx].pos - p.pos) * 0.33f;
            p.cp1Disparity = p.disparity + (m_points[nextIdx].disparity - p.disparity) * 0.33f;
            p.cp2 = p.pos + (m_points[nextIdx].pos - p.pos) * 0.66f;
            p.cp2Disparity = p.disparity + (m_points[nextIdx].disparity - p.disparity) * 0.66f;
        }
        newPs << p;
    }
    m_undoStack->push(new BatchMoveCommand(this, m_selectedIndices, oldPs, newPs));
    updateAnaglyphIfActive();
    notifySelectionState();
    if (m_autoSave) saveProject();
}

void StereoViewWidget::rotateSelectedPoints(float angleDegrees, RotationAxis axis)
{
    QList<int> indices = m_selectedIndices;
    if (indices.isEmpty()) for (int i = 0; i < m_points.count(); ++i) indices << i;
    if (indices.isEmpty()) return;

    // Calculate 3D center (X, Y, Disparity)
    QPointF centerPos(0, 0);
    float centerDisp = 0;
    for (int idx : indices) {
        centerPos += m_points[idx].pos;
        centerDisp += m_points[idx].disparity;
    }
    centerPos /= (float)indices.count();
    centerDisp /= (float)indices.count();

    float rad = qDegreesToRadians(angleDegrees);
    float cosA = qCos(rad);
    float sinA = qSin(rad);

    QVector<MaskPoint> oldPs, newPs;
    for (int idx : indices) {
        oldPs << m_points[idx];
        MaskPoint p = m_points[idx];

        auto rotate3D = [&](QPointF &pos, float &disp) {
            float dx = pos.x() - centerPos.x();
            float dy = pos.y() - centerPos.y();
            float dz = disp - centerDisp;

            if (axis == AxisX) {
                float newY = dy * cosA - dz * sinA;
                float newZ = dy * sinA + dz * cosA;
                pos.setY(centerPos.y() + newY);
                disp = centerDisp + newZ;
            } else if (axis == AxisY) {
                float newX = dx * cosA + dz * sinA;
                float newZ = -dx * sinA + dz * cosA;
                pos.setX(centerPos.x() + newX);
                disp = centerDisp + newZ;
            } else if (axis == AxisZ) {
                float newX = dx * cosA - dy * sinA;
                float newY = dx * sinA + dy * cosA;
                pos.setX(centerPos.x() + newX);
                pos.setY(centerPos.y() + newY);
            }
        };

        rotate3D(p.pos, p.disparity);
        if (p.isCurve) {
            rotate3D(p.cp1, p.cp1Disparity);
            rotate3D(p.cp2, p.cp2Disparity);
        }
        newPs << p;
    }
    m_undoStack->push(new BatchMoveCommand(this, indices, oldPs, newPs));
    updateAnaglyphIfActive();
    if (m_autoSave) saveProject();
}

void StereoViewWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    if (!m_processor.isValid()) return;

    QRect vL, vR; float scale; calculateLayout(vL, vR, scale);

    auto drawInViewport = [&](const QRect &v, const QImage &img, bool isRight) {
        painter.save();
        painter.setClipRect(v);
        float sw = img.width() * scale;
        float sh = img.height() * scale;
        QRectF target(v.center().x() - sw/2.0 + m_panOffset.x()*scale, v.center().y() - sh/2.0 + m_panOffset.y()*scale, sw, sh);
        painter.drawImage(target, img);
        // Editing always shows the source photo. A translucent green overlay
        // identifies painted reveal strokes without obscuring the subject.
        const QImage reveal = revealMask(isRight);
        if (!m_anaglyphMode && !reveal.isNull()) {
            QImage &tint = isRight ? m_revealTintRight : m_revealTintLeft;
            qint64 &key = isRight ? m_tintKeyRight : m_tintKeyLeft;
            if (key != reveal.cacheKey()) {
                tint = QImage(reveal.size(), QImage::Format_ARGB32_Premultiplied);
                tint.fill(QColor(0, 255, 100, 75));
                QPainter tp(&tint);
                tp.setCompositionMode(QPainter::CompositionMode_DestinationIn);
                tp.drawImage(0, 0, reveal); tp.end();
                key = reveal.cacheKey();
            }
            painter.drawImage(target, tint);
        }
        if (!m_anaglyphMode) {
            const QImage hints = isRight ? m_smartHintsRight : m_smartHintsLeft;
            if (!hints.isNull() && m_brushMode >= 3 && m_brushMode <= 6) { painter.setOpacity(0.6); painter.drawImage(target, hints); painter.setOpacity(1.0); }
            const QRect region = isRight ? m_smartRegionRight : m_smartRegionLeft;
            if (!region.isEmpty() && m_brushMode >= 3 && m_brushMode <= 6) {
                painter.setPen(QPen(isRight==m_smartRight ? Qt::cyan : Qt::gray, 2, Qt::DashLine));
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(QRectF(imageToWidget(region.topLeft(),v,scale), imageToWidget(QPointF(region.x()+region.width(),region.y()+region.height()),v,scale)));
            }
        }
        if (!m_anaglyphMode && m_brushMode>=7) {
            const QPainterPath completed=completedLasso(isRight);
            const QPointF origin=imageToWidget(QPointF(),v,scale);
            const QPainterPath screen=QTransform(scale,0,0,scale,origin.x(),origin.y()).map(completed);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(Qt::black,3,Qt::DashLine));painter.drawPath(screen);
            painter.setPen(QPen(Qt::yellow,1,Qt::DashLine));painter.drawPath(screen);
        }
        if (!m_anaglyphMode && isRight == m_lassoRight && !m_lassoAnchors.isEmpty()) {
            painter.setRenderHint(QPainter::Antialiasing);
            QPainterPath outline;
            outline.moveTo(imageToWidget(m_lassoAnchors.first(), v, scale));
            for (const auto &segment : m_lassoSegments)
                for (const QPointF &p : segment) outline.lineTo(imageToWidget(p, v, scale));
            if (m_lassoClosed) {
                for (const QPointF &p : m_lassoClosing) outline.lineTo(imageToWidget(p, v, scale));
                outline.closeSubpath();
            }
            painter.setBrush(m_lassoClosed ? QBrush(QColor(255,210,0,35)) : QBrush(Qt::NoBrush));
            painter.setPen(QPen(Qt::black, 4)); painter.drawPath(outline);
            painter.setPen(QPen(Qt::yellow, 2)); painter.drawPath(outline);
            if (!m_lassoClosed && m_cursorVisible && v.contains(m_brushCursor)) {
                painter.setPen(QPen(Qt::yellow, 1, Qt::DashLine));
                painter.drawLine(imageToWidget(m_lassoAnchors.last(), v, scale), QPointF(m_brushCursor));
            }
            for (const auto &p : m_lassoAnchors) {
                painter.setBrush(Qt::yellow); painter.setPen(QPen(Qt::black, 1));
                painter.drawEllipse(imageToWidget(p,v,scale), 4, 4);
            }
        }
        if (!m_points.isEmpty()) {
            painter.setPen(QPen(Qt::white, 1, Qt::DashLine));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(createMaskPath(m_points, v, scale, isRight, false));
        }

        if (!m_points.isEmpty()) {
            if (!m_anaglyphMode && !m_cutout && !m_brushMode && !m_pickingColor) {
                QRectF visible = target.intersected(QRectF(v));
                if (!visible.isEmpty()) {
                    QRect visibleRect = visible.toAlignedRect();
                    QRectF localVisible = visible.translated(-target.topLeft());

                    QImage maskImg(visibleRect.size(), QImage::Format_ARGB32);
                    if (!maskImg.isNull()) {
                        maskImg.fill(Qt::transparent);

                        QPainter maskPainter(&maskImg);
                        maskPainter.setRenderHint(QPainter::Antialiasing);

                        QColor fill = m_maskColor;
                        fill.setAlphaF(m_maskOpacity);
                        maskPainter.fillRect(maskImg.rect(), fill);

                        QPainterPath polyPath = createImageMaskPath(m_points, QRectF(0, 0, sw, sh), scale, isRight, false);

                        maskPainter.translate(-localVisible.topLeft());
                        maskPainter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
                        int previewFeather = m_previewFeatherEnabled ? qRound(m_featherAmount * scale) : 0;
                        drawFeatheredPath(maskPainter, polyPath, Qt::black, previewFeather);
                        maskPainter.end();

                        painter.drawImage(visibleRect.topLeft(), maskImg);
                    }
                }
            } else if (m_anaglyphMode) {
                QPainterPath path = createMaskPath(m_points, v, scale, isRight, false);
                painter.setPen(QPen(Qt::white, 1, Qt::DotLine));
                painter.drawPath(path);
            }

            for (int i=0; i<m_points.count(); ++i) {
                const auto &p = m_points[i];
                QPointF pW = imageToWidget(p.pos, v, scale, isRight ? p.disparity : 0);
                
                // Draw Bezier handles if curve
                if (p.isCurve) {
                    int nextIdx = (i + 1) % m_points.count();
                    QPointF nextW = imageToWidget(m_points[nextIdx].pos, v, scale, isRight ? m_points[nextIdx].disparity : 0);
                    QPointF cp1W = imageToWidget(p.cp1, v, scale, isRight ? p.cp1Disparity : 0);
                    QPointF cp2W = imageToWidget(p.cp2, v, scale, isRight ? p.cp2Disparity : 0);
                    
                    painter.setPen(QPen(Qt::gray, 1, Qt::DashLine));
                    painter.drawLine(pW, cp1W);
                    painter.drawLine(nextW, cp2W);
                    
                    painter.setBrush(Qt::green);
                    painter.setPen(m_selectedPointIndex == i && m_hitType == HitCP1 ? QPen(Qt::cyan, 2) : QPen(Qt::white, 1));
                    painter.drawRect(QRectF(cp1W.x()-3, cp1W.y()-3, 6, 6));
                    
                    painter.setPen(m_selectedPointIndex == i && m_hitType == HitCP2 ? QPen(Qt::cyan, 2) : QPen(Qt::white, 1));
                    painter.drawRect(QRectF(cp2W.x()-3, cp2W.y()-3, 6, 6));
                }

                painter.setBrush(m_selectedIndices.contains(i) ? Qt::yellow : Qt::red);
                painter.setPen(i == m_selectedPointIndex && m_hitType == HitPoint ? QPen(Qt::cyan, 2) : QPen(Qt::white, 1));
                painter.drawEllipse(pW, 5, 5);
            }
        }
        painter.restore();
    };

    if (m_anaglyphMode) {
        drawInViewport(vL, m_processor.anaglyphImage(), false);
    } else {
        drawInViewport(vL, m_swapSides ? m_processor.rightImage() : m_processor.leftImage(), m_swapSides);
        drawInViewport(vR, m_swapSides ? m_processor.leftImage() : m_processor.rightImage(), !m_swapSides);
    }

    drawAutoMaskDebugOverlay(painter, vL, scale, m_swapSides);
    if (!m_anaglyphMode) drawAutoMaskDebugOverlay(painter, vR, scale, !m_swapSides);
    
    if (m_isSelecting && !m_selectionRect.isNull()) { 
        painter.setPen(QPen(Qt::white, 1, Qt::DashLine)); painter.setBrush(QColor(255, 255, 255, 50)); 
        painter.drawRect(m_selectionRect); 
    }

    if (m_isFreehandDrawing && m_freehandPoints.size() > 1) {
        QPainterPath path;
        QPointF first = imageToWidget(m_freehandPoints.first(), m_freehandViewport, m_freehandScale);
        path.moveTo(first);
        for (int i = 1; i < m_freehandPoints.size(); ++i) {
            path.lineTo(imageToWidget(m_freehandPoints[i], m_freehandViewport, m_freehandScale));
        }
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(Qt::cyan, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);
    }
    if (m_brushMode && m_brushMode != 3 && m_brushMode < 7 && !m_anaglyphMode && !m_pickingColor && m_cursorVisible &&
        (vL.contains(m_brushCursor) || vR.contains(m_brushCursor))) {
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(Qt::NoBrush);
        const qreal radius = m_brushSize * scale / 2.0;
        painter.setPen(QPen(Qt::black, 3));
        painter.drawEllipse(QPointF(m_brushCursor), radius, radius);
        painter.setPen(QPen(m_brushMode==4 ? QColor(Qt::green) : m_brushMode==5 ? QColor(Qt::red) : QColor(Qt::white), 1));
        painter.drawEllipse(QPointF(m_brushCursor), radius, radius);
    }

}

void StereoViewWidget::mousePressEvent(QMouseEvent *event)
{
    if (!m_processor.isValid() || m_refining) return;
    setFocus();
    if (m_spacePan || m_panMode || event->button()==Qt::MiddleButton || (event->button()==Qt::LeftButton && (event->modifiers() & Qt::AltModifier))) {
        m_isPanning=true; m_lastMousePos=event->pos(); setCursor(Qt::ClosedHandCursor); return;
    }
    m_brushCursor = event->pos(); m_cursorVisible = true;
    if (m_pickingColor && event->button() == Qt::LeftButton) {
        QRect l, r; float scale; calculateLayout(l, r, scale);
        if (m_anaglyphMode || (!l.contains(event->pos()) && !r.contains(event->pos()))) return;
        const bool inRight = r.contains(event->pos());
        const bool rightEye = inRight ? !m_swapSides : m_swapSides;
        const QImage image = rightEye ? m_processor.rightImage() : m_processor.leftImage();
        const QPoint ip = widgetToImage(event->pos(), inRight ? r : l, scale).toPoint();
        if (!image.rect().contains(ip)) return;
        const QColor color = image.pixelColor(ip);
        setCutoutColor(color);
        m_pickingColor = false;
        m_colorPickClick = true;
        setCursor(m_brushMode ? Qt::BlankCursor : Qt::ArrowCursor);
        emit backgroundColorPicked(color); update(); return;
    }
    if (m_pickingColor) return;
    if (m_brushMode >= 7 && !m_anaglyphMode) {
        if (event->button() != Qt::LeftButton) return;
        QRect l,r; float scale; calculateLayout(l,r,scale);
        if (!l.contains(event->pos()) && !r.contains(event->pos())) return;
        const bool right = r.contains(event->pos()) ? !m_swapSides : m_swapSides;
        const QRect viewport = right != m_swapSides ? r : l;
        const QPointF ip = widgetToImage(event->pos(), viewport, scale);
        const QImage source = right ? m_processor.rightImage() : m_processor.leftImage();
        if (!source.rect().contains(ip.toPoint())) return;
        bool nearAnchor=false;
        if (right==m_lassoRight) for (const auto &p:m_lassoAnchors)
            if (QLineF(imageToWidget(p,viewport,scale),QPointF(event->pos())).length()<=7) nearAnchor=true;
        if (m_lassoClosed && !nearAnchor) finishLasso();
        if (!m_lassoAnchors.isEmpty() && right!=m_lassoRight) {
            emit smartStatus(tr("Close this outline before moving to the other eye."));return;
        }
        for (int i=0; i<m_lassoAnchors.size(); ++i) {
            if (QLineF(imageToWidget(m_lassoAnchors[i],viewport,scale),QPointF(event->pos())).length() <= 7) {
                if (i==0 && !m_lassoClosed && m_lassoAnchors.size()>=3) {
                    closeLasso(); return;
                }
                m_lassoDrag=i; return;
            }
        }
        if (m_lassoClosed) { emit smartStatus(tr("Drag yellow points to adjust. Apply selection to keep the enclosed area.")); return; }
        m_lassoRight=right;
        m_lassoAnchors.append(ip);
        if (m_lassoAnchors.size()>1) {
            m_lassoMagnetic.append(m_brushMode==8);
            m_lassoSegments.append(QVector<QPointF>{}); rebuildLassoSegment(m_lassoSegments.size()-1);
        }
        emit smartStatus(tr("Click along the outline. Delete removes the latest segment. Drag points to adjust. Enter closes; Apply selection commits."));
        update(); return;
    }
    if (m_brushMode && !m_anaglyphMode && event->button() == Qt::LeftButton) {
        if (m_points.size() < 3) return;
        QRect l,r; float scale; calculateLayout(l,r,scale);
        if (!l.contains(event->pos()) && !r.contains(event->pos())) return;
        m_brushRight = r.contains(event->pos()) ? !m_swapSides : m_swapSides;
        m_smartRight = m_brushRight;
        if (m_brushMode == 3) {
            const QRect viewport = m_brushRight != m_swapSides ? r : l;
            m_regionStart = widgetToImage(event->pos(), viewport, scale);
            m_beforeRegion = m_brushRight ? m_smartRegionRight : m_smartRegionLeft;
            m_selectingRegion = true; update(); return;
        }
        if (m_brushMode >= 4) {
            const QRect region=m_brushRight ? m_smartRegionRight : m_smartRegionLeft;
            if (region.isEmpty()) { emit smartStatus(tr("Draw a Smart box around the leg in this eye first.")); return; }
            m_beforeStroke = m_brushRight ? m_smartHintsRight : m_smartHintsLeft;
        } else m_beforeStroke = revealMask(m_brushRight);
        m_cutout = true; m_brushing = true; m_hasBrushPoint = false;
        brushAt(event->pos()); return;
    }
    m_lastMousePos = event->pos(); m_mouseDragStart = event->pos();
    QRect vL, vR; float scale; calculateLayout(vL, vR, scale);

    if (m_panMode || event->button() == Qt::MiddleButton || (event->button() == Qt::LeftButton && (event->modifiers() & Qt::AltModifier))) {
        m_isPanning = true; setCursor(Qt::SizeAllCursor); return;
    }

    if (m_freehandMode && event->button() == Qt::LeftButton && m_points.isEmpty()) {
        QRect targetV = vL.contains(event->pos()) ? vL : (vR.contains(event->pos()) && !m_anaglyphMode ? vR : QRect());
        if (!targetV.isNull()) {
            m_freehandViewport = targetV;
            m_freehandScale = scale;
            m_freehandPoints.clear();
            m_isFreehandDrawing = true;
            appendFreehandPoint(event->pos());
            return;
        }
    }

    int hitIdx = -1;
    m_hitType = HitNone;
    bool inRight = vR.contains(event->pos()) && !m_anaglyphMode;
    QRect targetV = inRight ? vR : vL;
    
    if (targetV.contains(event->pos())) {
        bool isRightEye = inRight ? !m_swapSides : m_swapSides;
        for (int i=0; i<m_points.count(); ++i) {
            const auto &p = m_points[i];
            QPointF pW = imageToWidget(p.pos, targetV, scale, isRightEye ? p.disparity : 0);
            if (QLineF(event->pos(), pW).length() < 10) { hitIdx = i; m_hitType = HitPoint; break; }
            
            if (p.isCurve) {
                QPointF cp1W = imageToWidget(p.cp1, targetV, scale, isRightEye ? p.cp1Disparity : 0);
                if (QLineF(event->pos(), cp1W).length() < 10) { hitIdx = i; m_hitType = HitCP1; break; }
                QPointF cp2W = imageToWidget(p.cp2, targetV, scale, isRightEye ? p.cp2Disparity : 0);
                if (QLineF(event->pos(), cp2W).length() < 10) { hitIdx = i; m_hitType = HitCP2; break; }
            }
        }
    }

    if (hitIdx != -1) {
        if (m_hitType == HitPoint) {
            if (event->modifiers() & Qt::ControlModifier) {
                if (m_selectedIndices.contains(hitIdx)) m_selectedIndices.removeOne(hitIdx); else m_selectedIndices.append(hitIdx);
            } else if (!m_selectedIndices.contains(hitIdx)) {
                m_selectedIndices.clear(); m_selectedIndices.append(hitIdx);
            }
        } else {
            m_selectedIndices.clear(); m_selectedIndices.append(hitIdx);
        }
        
        m_selectedPointIndex = hitIdx;
        m_preMovePoints.clear(); for (int idx : m_selectedIndices) m_preMovePoints << m_points[idx];
        setCursor(Qt::ClosedHandCursor); 
        float disp = (m_hitType == HitCP1) ? m_points[hitIdx].cp1Disparity : (m_hitType == HitCP2 ? m_points[hitIdx].cp2Disparity : m_points[hitIdx].disparity);
        emit selectionChanged(disp);
        notifySelectionState();
    } else {
        if (event->button() == Qt::LeftButton) {
            if (vL.contains(event->pos()) || vR.contains(event->pos())) {
                m_selectedIndices.clear(); m_selectedPointIndex = -1;
                m_isSelecting = true; m_selectionRect = QRect(event->pos(), QSize(1, 1));
                notifySelectionState();
            } else { m_isPanning = true; setCursor(Qt::SizeAllCursor); }
        } else if (event->button() == Qt::RightButton && targetV.contains(event->pos())) {
            QPointF imgPos = widgetToImage(event->pos(), targetV, scale);
            bool isRightEye = inRight ? !m_swapSides : m_swapSides;
            float d = (m_points.isEmpty() ? 0 : m_points[0].disparity);
            if (isRightEye) imgPos.setX(imgPos.x() + d);
            
            // Check if we clicked on an edge to insert
            int insertIdx = -1;
            float minEdgeDist = 10.0f; // px threshold
            for (int i=0; i<m_points.count(); ++i) {
                int nextIdx = (i + 1) % m_points.count();
                const auto &p1 = m_points[i];
                const auto &p2 = m_points[nextIdx];
                QPointF p1W = imageToWidget(p1.pos, targetV, scale, isRightEye ? p1.disparity : 0);
                QPointF p2W = imageToWidget(p2.pos, targetV, scale, isRightEye ? p2.disparity : 0);
                
                if (p1.isCurve) {
                    // Approximate curve distance by sampling
                    QPointF cp1W = imageToWidget(p1.cp1, targetV, scale, isRightEye ? p1.cp1Disparity : 0);
                    QPointF cp2W = imageToWidget(p1.cp2, targetV, scale, isRightEye ? p1.cp2Disparity : 0);
                    for (float t = 0.1f; t < 1.0f; t += 0.1f) {
                        float it = 1.0f - t;
                        QPointF b = it*it*it*p1W + 3*it*it*t*cp1W + 3*it*t*t*cp2W + t*t*t*p2W;
                        if (QLineF(event->pos(), b).length() < minEdgeDist) { insertIdx = nextIdx; break; }
                    }
                } else {
                    // Linear distance
                    QLineF line(p1W, p2W);
                    QPointF p3 = event->pos();
                    float l2 = line.length() * line.length();
                    if (l2 != 0) {
                        float t = qBound(0.0f, (float)QPointF::dotProduct(p3 - p1W, p2W - p1W) / l2, 1.0f);
                        QPointF projection = p1W + t * (p2W - p1W);
                        if (QLineF(p3, projection).length() < minEdgeDist) insertIdx = nextIdx;
                    }
                }
                if (insertIdx != -1) break;
            }

            addPoint(imgPos, d, insertIdx);
            int newIdx = (insertIdx == -1) ? m_points.count()-1 : insertIdx;
            m_selectedPointIndex = newIdx; m_selectedIndices.clear(); m_selectedIndices << m_selectedPointIndex; m_hitType = HitPoint;
            m_preMovePoints.clear(); m_preMovePoints << m_points[newIdx];
            notifySelectionState();
        }
    }
    update();
}

void StereoViewWidget::mouseMoveEvent(QMouseEvent *event)
{
    m_brushCursor = event->pos(); m_cursorVisible = true;
    if (m_refining) return;
    if (m_isPanning) {
        QRect l,r; float scale; calculateLayout(l,r,scale);
        const QPoint delta=event->pos()-m_lastMousePos;
        m_panOffset+=QPointF(delta.x()/scale,delta.y()/scale);
        m_lastMousePos=event->pos(); update(); return;
    }
    if (m_brushMode >= 7 && !m_pickingColor) {
        setCursor(Qt::CrossCursor);
        if (m_lassoDrag >= 0 && (event->buttons() & Qt::LeftButton)) {
            QRect l,r; float scale; calculateLayout(l,r,scale);
            const QRect viewport=m_lassoRight != m_swapSides ? r : l;
            QPointF p=widgetToImage(event->pos(),viewport,scale);
            const QImage source=m_lassoRight ? m_processor.rightImage() : m_processor.leftImage();
            p.setX(qBound(0.0,p.x(),double(source.width()-1)));
            p.setY(qBound(0.0,p.y(),double(source.height()-1)));
            m_lassoAnchors[m_lassoDrag]=p;
            rebuildLassoSegment(m_lassoDrag-1); rebuildLassoSegment(m_lassoDrag);
            if (m_lassoClosed) rebuildLassoClosing();
        }
        update(); return;
    }
    if (m_selectingRegion) {
        QRect l,r; float scale; calculateLayout(l,r,scale);
        const QRect viewport=m_brushRight != m_swapSides ? r : l;
        const QPointF current=widgetToImage(event->pos(),viewport,scale);
        (m_brushRight ? m_smartRegionRight : m_smartRegionLeft)=QRectF(m_regionStart,current).normalized().toAlignedRect().intersected(m_processor.leftImage().rect());
        update(); return;
    }
    if (m_brushMode && !m_pickingColor) {
        QRect l, r; float scale; calculateLayout(l, r, scale);
        setCursor(!m_anaglyphMode && (l.contains(event->pos()) || r.contains(event->pos())) ? (m_brushMode==3 ? Qt::CrossCursor : Qt::BlankCursor) : Qt::ArrowCursor);
        update();
    }
    if (m_pickingColor) return;
    if (m_brushing) { brushAt(event->pos()); return; }
    if (m_isFreehandDrawing) {
        if (event->buttons() & Qt::LeftButton) {
            appendFreehandPoint(event->pos());
        }
        return;
    }

    if (m_isPanning) {
        QPoint delta = event->pos() - m_lastMousePos;
        float scale; QRect vL, vR; calculateLayout(vL, vR, scale);
        m_panOffset += QPointF(delta.x() / scale, delta.y() / scale);
        m_lastMousePos = event->pos(); update(); return;
    }
    if (m_isSelecting) { m_selectionRect.setBottomRight(event->pos()); update(); return; }
    
    if (m_selectedPointIndex != -1 && (event->buttons() & Qt::LeftButton)) {
        float scale; QRect vL, vR; calculateLayout(vL, vR, scale);
        QPointF totalDelta((event->pos().x() - m_mouseDragStart.x()) / scale, (event->pos().y() - m_mouseDragStart.y()) / scale);

        if (m_rotationMode != AxisNone) {
            float dx = (float)(event->pos().x() - m_lastMousePos.x());
            float dy = (float)(event->pos().y() - m_lastMousePos.y());
            if (m_lockHorizontal) dx = 0; if (m_lockVertical) dy = 0;
            
            float angle = 0;
            if (m_rotationMode == AxisX) angle = -dy; // Vertical drag tilts around X
            else if (m_rotationMode == AxisY) angle = dx; // Horizontal drag tilts around Y
            else if (m_rotationMode == AxisZ) angle = dx + dy; // Combined drag rolls around Z
            
            if (angle != 0) {
                QPointF centerPos(0, 0);
                float centerDisp = 0;
                for (int idx : m_selectedIndices) {
                    centerPos += m_points[idx].pos;
                    centerDisp += m_points[idx].disparity;
                }
                centerPos /= (float)m_selectedIndices.count();
                centerDisp /= (float)m_selectedIndices.count();

                float rad = qDegreesToRadians(angle);
                float cosA = qCos(rad);
                float sinA = qSin(rad);

                auto rotate3D = [&](QPointF &pos, float &disp) {
                    float x = pos.x() - centerPos.x();
                    float y = pos.y() - centerPos.y();
                    float z = disp - centerDisp;

                    if (m_rotationMode == AxisX) {
                        pos.setY(centerPos.y() + y * cosA - z * sinA);
                        disp = centerDisp + y * sinA + z * cosA;
                    } else if (m_rotationMode == AxisY) {
                        pos.setX(centerPos.x() + x * cosA + z * sinA);
                        disp = centerDisp - x * sinA + z * cosA;
                    } else if (m_rotationMode == AxisZ) {
                        pos.setX(centerPos.x() + x * cosA - y * sinA);
                        pos.setY(centerPos.y() + x * sinA + y * cosA);
                    }
                };

                for (int idx : m_selectedIndices) {
                    rotate3D(m_points[idx].pos, m_points[idx].disparity);
                    if (m_points[idx].isCurve) {
                        rotate3D(m_points[idx].cp1, m_points[idx].cp1Disparity);
                        rotate3D(m_points[idx].cp2, m_points[idx].cp2Disparity);
                    }
                }
                updateAnaglyphIfActive();
            }
        } else if (event->modifiers() & Qt::ShiftModifier) {
            float dx = (float)(event->pos().x() - m_lastMousePos.x()) / scale;
            if (m_hitType == HitCP1) m_points[m_selectedPointIndex].cp1Disparity += dx;
            else if (m_hitType == HitCP2) m_points[m_selectedPointIndex].cp2Disparity += dx;
            else {
                for (int idx : m_selectedIndices) m_points[idx].disparity += dx;
            }
            float disp = (m_hitType == HitCP1) ? m_points[m_selectedPointIndex].cp1Disparity : (m_hitType == HitCP2 ? m_points[m_selectedPointIndex].cp2Disparity : m_points[m_selectedPointIndex].disparity);
            emit selectionChanged(disp);
        } else {
            if (m_lockHorizontal) totalDelta.setX(0); if (m_lockVertical) totalDelta.setY(0);
            if (m_hitType == HitCP1) m_points[m_selectedPointIndex].cp1 = m_preMovePoints[0].cp1 + totalDelta;
            else if (m_hitType == HitCP2) m_points[m_selectedPointIndex].cp2 = m_preMovePoints[0].cp2 + totalDelta;
            else {
                if (m_selectedIndices.count() == 1) {
                    bool inRight = !m_anaglyphMode && vR.contains(m_mouseDragStart);
                    float dragDisp = inRight ? (!m_swapSides ? m_preMovePoints[0].disparity : 0) : (m_swapSides ? m_preMovePoints[0].disparity : 0);
                    QPointF visualPos = (m_preMovePoints[0].pos - QPointF(dragDisp, 0)) + totalDelta;
                    float snap = 10.0f / scale;
                    for (int i=0; i<m_points.count(); ++i) {
                        if (i == m_selectedPointIndex) continue;
                        float tx = inRight ? (m_points[i].pos.x() - (!m_swapSides ? m_points[i].disparity : 0)) : (m_points[i].pos.x() - (m_swapSides ? m_points[i].disparity : 0));
                        if (m_snapEnabled) {
                            if (qAbs(visualPos.x() - tx) < snap) visualPos.setX(tx);
                            if (qAbs(visualPos.y() - m_points[i].pos.y()) < snap) visualPos.setY(m_points[i].pos.y());
                        }
                    }
                    m_points[m_selectedPointIndex].pos = visualPos + QPointF(dragDisp, 0);
                } else {
                    for (int i=0; i<m_selectedIndices.count(); ++i) m_points[m_selectedIndices[i]].pos = m_preMovePoints[i].pos + totalDelta;
                }
            }
        }
        m_lastMousePos = event->pos(); update();
    }
}

void StereoViewWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_isPanning) {
        m_isPanning=false;
        setCursor(m_spacePan || m_panMode ? Qt::OpenHandCursor : m_brushMode >= 7 ? Qt::CrossCursor : m_brushMode ? Qt::BlankCursor : Qt::ArrowCursor);
        update(); return;
    }
    if (m_brushMode >= 7 && !m_pickingColor) { m_lassoDrag=-1; return; }
    if (m_selectingRegion && event->button()==Qt::LeftButton) {
        mouseMoveEvent(event); m_selectingRegion=false;
        const QRect region=m_brushRight ? m_smartRegionRight : m_smartRegionLeft;
        const QImage hints=m_brushRight ? m_smartHintsRight : m_smartHintsLeft;
        m_undoStack->push(new SmartHintsCommand(this,m_brushRight,hints,hints,m_beforeRegion,region));
        emit smartStatus(tr("Box selected. Paint Keep inside the leg and Remove on nearby background, then Find edges."));
        update(); return;
    }
    if (m_colorPickClick && event->button() == Qt::LeftButton) {
        m_colorPickClick = false; return;
    }
    if (m_pickingColor) return;
    if (m_brushing && event->button() == Qt::LeftButton) {
        brushAt(event->pos()); m_brushing=false;
        if (m_brushMode>=4) {
            const QImage hints=m_brushRight ? m_smartHintsRight : m_smartHintsLeft;
            const QRect region=m_brushRight ? m_smartRegionRight : m_smartRegionLeft;
            m_undoStack->push(new SmartHintsCommand(this,m_brushRight,m_beforeStroke,hints,region,region));
        } else m_undoStack->push(new RevealStrokeCommand(this, m_brushRight, m_beforeStroke, revealMask(m_brushRight)));
        m_beforeStroke = QImage(); return;
    }
    if (m_isFreehandDrawing) {
        if (event->button() == Qt::LeftButton) {
            appendFreehandPoint(event->pos());
            finishFreehandDrawing();
        }
        return;
    }

    if (m_isPanning) { m_isPanning = false; setCursor(m_panMode ? Qt::OpenHandCursor : Qt::ArrowCursor); return; }
    if (m_isSelecting) {
        float scale; QRect vL, vR; calculateLayout(vL, vR, scale);
        QRect normRect = m_selectionRect.normalized();
        for (int i = 0; i < m_points.count(); ++i) {
            QPointF pL = imageToWidget(m_points[i].pos, vL, scale, m_swapSides ? m_points[i].disparity : 0);
            QPointF pR = imageToWidget(m_points[i].pos, vR, scale, !m_swapSides ? m_points[i].disparity : 0);
            if (normRect.contains(pL.toPoint()) || normRect.contains(pR.toPoint())) {
                if (!m_selectedIndices.contains(i)) m_selectedIndices << i;
            }
        }
        if (!m_selectedIndices.isEmpty()) emit selectionChanged(m_points[m_selectedIndices.last()].disparity);
        m_isSelecting = false; m_selectionRect = QRect();
        notifySelectionState();
    } else if (m_selectedPointIndex != -1) {
        bool moved = false; QVector<MaskPoint> newPs;
        for (int i = 0; i < m_selectedIndices.count(); ++i) {
            newPs << m_points[m_selectedIndices[i]];
            const auto &p = m_points[m_selectedIndices[i]];
            const auto &pre = m_preMovePoints[i];
            if (p.pos != pre.pos || p.disparity != pre.disparity || p.cp1 != pre.cp1 || p.cp1Disparity != pre.cp1Disparity || p.cp2 != pre.cp2 || p.cp2Disparity != pre.cp2Disparity || p.isCurve != pre.isCurve) moved = true;
        }
        if (moved) {
            m_undoStack->push(new BatchMoveCommand(this, m_selectedIndices, m_preMovePoints, newPs));
            if (m_autoSave) saveProject();
        }
    }
    m_selectedPointIndex = -1; m_hitType = HitNone; notifySelectionState(); setCursor(m_panMode ? Qt::OpenHandCursor : Qt::ArrowCursor); update();
}

void StereoViewWidget::wheelEvent(QWheelEvent *event) 
{ 
    QRect vL, vR; float oldScale; calculateLayout(vL, vR, oldScale);
    QPoint pos = event->position().toPoint();
    QRect targetV = vL.contains(pos) ? vL : (vR.contains(pos) ? vR : QRect());
    if (targetV.isNull()) {
        setZoom(m_zoom * (event->angleDelta().y() > 0 ? 1.1f : 0.9f));
        return;
    }

    QPointF imgPos = widgetToImage(pos, targetV, oldScale);
    float factor = (event->angleDelta().y() > 0 ? 1.1f : 0.9f);
    m_zoom = qBound(0.1f, m_zoom * factor, 20.0f);
    
    float newScale; calculateLayout(vL, vR, newScale);
    float iw = m_processor.leftImage().width();
    float ih = m_processor.leftImage().height();
    m_panOffset.setX((pos.x() - targetV.center().x()) / newScale + iw/2.0 - imgPos.x());
    m_panOffset.setY((pos.y() - targetV.center().y()) / newScale + ih/2.0 - imgPos.y());
    update();
}

void StereoViewWidget::setSelectedPointDisparity(int disparity) 
{ 
    if (m_selectedIndices.isEmpty()) return;
    QVector<MaskPoint> oldPs, newPs;
    for (int idx : m_selectedIndices) { oldPs << m_points[idx]; MaskPoint p = m_points[idx]; p.disparity = (float)disparity; newPs << p; }
    m_undoStack->push(new BatchMoveCommand(this, m_selectedIndices, oldPs, newPs));
    if (m_autoSave) saveProject();
}

void StereoViewWidget::transformSelectedPoints(float scaleX, float scaleY, float dx, float dy)
{
    QList<int> indices = m_selectedIndices; 
    if (indices.isEmpty()) for (int i = 0; i < m_points.count(); ++i) indices << i;
    if (indices.isEmpty()) return;

    QPointF center(0, 0); 
    float totalDisp = 0;
    for (int idx : indices) {
        center += m_points[idx].pos;
        totalDisp += m_points[idx].disparity;
    }
    center /= (float)indices.count();
    float avgDisp = totalDisp / (float)indices.count();

    QVector<MaskPoint> oldPs, newPs;
    for (int idx : indices) {
        oldPs << m_points[idx];
        MaskPoint p = m_points[idx];
        
        // Transform main position
        p.pos = center + QPointF((p.pos.x() - center.x()) * scaleX, (p.pos.y() - center.y()) * scaleY) + QPointF(dx, dy);
        
        // Transform handles if segment is a curve
        if (p.isCurve) {
            p.cp1 = center + QPointF((p.cp1.x() - center.x()) * scaleX, (p.cp1.y() - center.y()) * scaleY) + QPointF(dx, dy);
            p.cp2 = center + QPointF((p.cp2.x() - center.x()) * scaleX, (p.cp2.y() - center.y()) * scaleY) + QPointF(dx, dy);
        }
        newPs << p;
    }
    m_undoStack->push(new BatchMoveCommand(this, indices, oldPs, newPs));
    if (m_autoSave) saveProject();
}

void StereoViewWidget::leaveEvent(QEvent *event) {
    m_cursorVisible = false; update(); QWidget::leaveEvent(event);
}
void StereoViewWidget::keyPressEvent(QKeyEvent *event) {
    if (event->key()==Qt::Key_Space && !event->isAutoRepeat()) { m_spacePan=true; setCursor(Qt::OpenHandCursor); event->accept(); return; }
    if (m_brushMode >= 7 && !m_pickingColor) {
        if (event->key()==Qt::Key_Delete || event->key()==Qt::Key_Backspace) { removeLassoSegment(); event->accept(); return; }
        if (event->key()==Qt::Key_Escape) { cancelUnfinishedLasso(); event->accept(); return; }
        if (event->key()==Qt::Key_Return || event->key()==Qt::Key_Enter) { closeLasso(); event->accept(); return; }
    }
    if (m_pickingColor && event->key() == Qt::Key_Escape) {
        m_pickingColor = false;
        setCursor(m_brushMode ? Qt::BlankCursor : Qt::ArrowCursor);
        update(); event->accept(); return;
    }
    QWidget::keyPressEvent(event);
}

void StereoViewWidget::rebuildLassoSegment(int index) {
    if (index < 0 || index >= m_lassoSegments.size()) return;
    const auto a=m_lassoAnchors[index], b=m_lassoAnchors[index+1];
    m_lassoSegments[index] = m_lassoMagnetic[index]
        ? magneticLassoPath(m_lassoRight ? m_processor.rightImage() : m_processor.leftImage(),a,b)
        : QVector<QPointF>{a,b};
}
void StereoViewWidget::rebuildLassoClosing() {
    if (m_lassoAnchors.size()<3) return;
    m_lassoClosing=m_brushMode==8 ? magneticLassoPath(m_lassoRight ? m_processor.rightImage() : m_processor.leftImage(),m_lassoAnchors.last(),m_lassoAnchors.first())
        : QVector<QPointF>{m_lassoAnchors.last(),m_lassoAnchors.first()};
}
void StereoViewWidget::cancelLasso() {
    m_lassoAnchors.clear(); m_lassoSegments.clear(); m_lassoMagnetic.clear(); m_lassoClosing.clear();
    m_lassoClosed=false; m_lassoDrag=-1; update();
}
void StereoViewWidget::removeLassoSegment() {
    if (m_lassoAnchors.isEmpty()) return;
    m_lassoClosed=false; m_lassoClosing.clear(); m_lassoDrag=-1;
    m_lassoAnchors.removeLast();
    if (!m_lassoSegments.isEmpty()) { m_lassoSegments.removeLast(); m_lassoMagnetic.removeLast(); }
    update(); emit smartStatus(tr("Latest segment removed; the rest of the outline remains."));
}
void StereoViewWidget::closeLasso() {
    if (m_lassoAnchors.size()<3) { emit smartStatus(tr("Place at least three points before closing the outline.")); return; }
    m_lassoClosed=true; rebuildLassoClosing(); update(); setFocus();
    emit smartStatus(tr("Outline closed. Drag points to adjust, or click elsewhere to keep it and start another. Apply selection or Escape keeps the closed outline."));
}
void StereoViewWidget::applyLasso(bool erase) {
    if (m_refining || !m_processor.isValid() || m_lassoAnchors.size()<3) return;
    if (!m_lassoClosed) closeLasso();
    QPainterPath path; path.moveTo(m_lassoAnchors.first());
    for (const auto &segment:m_lassoSegments) for (const auto &p:segment) path.lineTo(p);
    for (const auto &p:m_lassoClosing) path.lineTo(p);
    path.closeSubpath();
    const QImage before=revealMask(m_lassoRight);
    QImage after=before;
    if (after.isNull()) { after=QImage((m_lassoRight ? m_processor.rightImage() : m_processor.leftImage()).size(),QImage::Format_ARGB32_Premultiplied); after.fill(Qt::transparent); }
    QPainter p(&after);
    if (erase) p.setCompositionMode(QPainter::CompositionMode_Clear);
    p.setRenderHint(QPainter::Antialiasing); p.fillPath(path,Qt::white); p.end();
    m_cutout=true;
    const QPainterPath beforeOutline=completedLasso(m_lassoRight);
    const QPainterPath afterOutline=erase ? beforeOutline.subtracted(path) : beforeOutline.united(path);
    m_undoStack->push(new LassoSelectionCommand(this,m_lassoRight,before,after,beforeOutline,afterOutline));
    cancelLasso(); setFocus(); emit smartStatus(erase ? tr("Lasso area erased from painted reveal. The frame stays open. Undo restores it.") : tr("Lasso area preserved. Undo restores the previous reveal. Draw the corresponding outline in the other eye."));
}

void StereoViewWidget::keyReleaseEvent(QKeyEvent *event) {
    if (event->key()==Qt::Key_Space && !event->isAutoRepeat()) {
        m_spacePan=false;
        setCursor(m_panMode ? Qt::OpenHandCursor : m_brushMode >= 7 ? Qt::CrossCursor : m_brushMode ? Qt::BlankCursor : Qt::ArrowCursor);
        event->accept(); return;
    }
    QWidget::keyReleaseEvent(event);
}
void StereoViewWidget::clearPaintedReveal() {
    if (m_refining) return;
    cancelLasso();
    m_undoStack->beginMacro(tr("Clear painted reveal"));
    if (!m_revealLeft.isNull() || !m_completedLeft.isEmpty()) m_undoStack->push(new LassoSelectionCommand(this,false,m_revealLeft,QImage(),m_completedLeft,QPainterPath()));
    if (!m_revealRight.isNull() || !m_completedRight.isEmpty()) m_undoStack->push(new LassoSelectionCommand(this,true,m_revealRight,QImage(),m_completedRight,QPainterPath()));
    m_undoStack->endMacro();
    emit smartStatus(tr("Painted reveal cleared in both eyes; your frame stays open. Undo restores the paint."));
}
