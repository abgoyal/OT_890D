

#ifndef QWEBFRAME_H
#define QWEBFRAME_H

#include <QtCore/qobject.h>
#include <QtCore/qurl.h>
#include <QtCore/qvariant.h>
#include <QtGui/qicon.h>
#include <QtScript/qscriptengine.h>
#if QT_VERSION >= 0x040400
#include <QtNetwork/qnetworkaccessmanager.h>
#endif
#include "qwebkitglobal.h"

QT_BEGIN_NAMESPACE
class QRect;
class QPoint;
class QPainter;
class QPixmap;
class QMouseEvent;
class QWheelEvent;
class QNetworkRequest;
class QRegion;
class QPrinter;
QT_END_NAMESPACE

class QWebNetworkRequest;
class QWebFramePrivate;
class QWebPage;
class QWebHitTestResult;
class QWebHistoryItem;
class QWebSecurityOrigin;
class QWebElement;

namespace WebCore {
    class WidgetPrivate;
    class FrameLoaderClientQt;
    class ChromeClientQt;
}
class QWebFrameData;
class QWebHitTestResultPrivate;
class QWebFrame;

class QWEBKIT_EXPORT QWebHitTestResult {
public:
    QWebHitTestResult();
    QWebHitTestResult(const QWebHitTestResult &other);
    QWebHitTestResult &operator=(const QWebHitTestResult &other);
    ~QWebHitTestResult();

    bool isNull() const;

    QPoint pos() const;
    QRect boundingRect() const;
    QWebElement enclosingBlockElement() const;
    QString title() const;

    QString linkText() const;
    QUrl linkUrl() const;
    QUrl linkTitle() const;
    QWebFrame *linkTargetFrame() const;
    QWebElement linkElement() const;

    QString alternateText() const; // for img, area, input and applet

    QUrl imageUrl() const;
    QPixmap pixmap() const;

    bool isContentEditable() const;
    bool isContentSelected() const;

    QWebElement element() const;

    QWebFrame *frame() const;

private:
    QWebHitTestResult(QWebHitTestResultPrivate *priv);
    QWebHitTestResultPrivate *d;

    friend class QWebFrame;
    friend class QWebPagePrivate;
    friend class QWebPage;
};

class QWEBKIT_EXPORT QWebFrame : public QObject {
    Q_OBJECT
    Q_PROPERTY(qreal textSizeMultiplier READ textSizeMultiplier WRITE setTextSizeMultiplier DESIGNABLE false)
    Q_PROPERTY(qreal zoomFactor READ zoomFactor WRITE setZoomFactor)
    Q_PROPERTY(QString title READ title)
    Q_PROPERTY(QUrl url READ url WRITE setUrl)
    Q_PROPERTY(QUrl requestedUrl READ requestedUrl)
    Q_PROPERTY(QUrl baseUrl READ baseUrl)
    Q_PROPERTY(QIcon icon READ icon)
    Q_PROPERTY(QSize contentsSize READ contentsSize)
    Q_PROPERTY(QPoint scrollPosition READ scrollPosition WRITE setScrollPosition)
    Q_PROPERTY(bool clipRenderToViewport READ clipRenderToViewport WRITE setClipRenderToViewport)
    Q_PROPERTY(bool focus READ hasFocus)
private:
    QWebFrame(QWebPage *parent, QWebFrameData *frameData);
    QWebFrame(QWebFrame *parent, QWebFrameData *frameData);
    ~QWebFrame();

public:
    QWebPage *page() const;

    void load(const QUrl &url);
#if QT_VERSION < 0x040400
    void load(const QWebNetworkRequest &request);
#else
    void load(const QNetworkRequest &request,
              QNetworkAccessManager::Operation operation = QNetworkAccessManager::GetOperation,
              const QByteArray &body = QByteArray());
#endif
    void setHtml(const QString &html, const QUrl &baseUrl = QUrl());
    void setContent(const QByteArray &data, const QString &mimeType = QString(), const QUrl &baseUrl = QUrl());

    void addToJavaScriptWindowObject(const QString &name, QObject *object);
    void addToJavaScriptWindowObject(const QString &name, QObject *object, QScriptEngine::ValueOwnership ownership);
    QString toHtml() const;
    QString toPlainText() const;
    QString renderTreeDump() const;

    QString title() const;
    void setUrl(const QUrl &url);
    QUrl url() const;
    QUrl requestedUrl() const;
    QUrl baseUrl() const;
    QIcon icon() const;
    QMultiMap<QString, QString> metaData() const;

    QString frameName() const;

    QWebFrame *parentFrame() const;
    QList<QWebFrame*> childFrames() const;

    Qt::ScrollBarPolicy scrollBarPolicy(Qt::Orientation orientation) const;
    void setScrollBarPolicy(Qt::Orientation orientation, Qt::ScrollBarPolicy policy);

    void setScrollBarValue(Qt::Orientation orientation, int value);
    int scrollBarValue(Qt::Orientation orientation) const;
    int scrollBarMinimum(Qt::Orientation orientation) const;
    int scrollBarMaximum(Qt::Orientation orientation) const;
    QRect scrollBarGeometry(Qt::Orientation orientation) const;

    void scroll(int, int);
    QPoint scrollPosition() const;
    void setScrollPosition(const QPoint &pos);

    void render(QPainter *painter, const QRegion &clip);
    void render(QPainter *painter);
    bool clipRenderToViewport() const;
    void setClipRenderToViewport(bool clipRenderToViewport);

    void setTextSizeMultiplier(qreal factor);
    qreal textSizeMultiplier() const;

    qreal zoomFactor() const;
    void setZoomFactor(qreal factor);

    bool hasFocus() const;
    void setFocus();

    QPoint pos() const;
    QRect geometry() const;
    QSize contentsSize() const;

    QWebElement documentElement() const;
    QList<QWebElement> findAllElements(const QString &selectorQuery) const;
    QWebElement findFirstElement(const QString &selectorQuery) const;

    QWebHitTestResult hitTestContent(const QPoint &pos) const;

    virtual bool event(QEvent *);

    QWebSecurityOrigin securityOrigin() const;

public Q_SLOTS:
    QVariant evaluateJavaScript(const QString& scriptSource);
#ifndef QT_NO_PRINTER
    void print(QPrinter *printer) const;
#endif

Q_SIGNALS:
    void javaScriptWindowObjectCleared();

    void provisionalLoad();
    void titleChanged(const QString &title);
    void urlChanged(const QUrl &url);

    void initialLayoutCompleted();

    void iconChanged();

    void contentsSizeChanged(const QSize &size);

    void loadStarted();
    void loadFinished(bool ok);

private:
    friend class QWebPage;
    friend class QWebPagePrivate;
    friend class QWebFramePrivate;
    friend class WebCore::WidgetPrivate;
    friend class WebCore::FrameLoaderClientQt;
    friend class WebCore::ChromeClientQt;
    QWebFramePrivate *d;
};

#endif
