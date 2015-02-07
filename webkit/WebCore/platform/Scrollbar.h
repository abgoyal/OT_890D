

#ifndef Scrollbar_h
#define Scrollbar_h

#include "ScrollTypes.h"
#include "Timer.h"
#include "Widget.h"
#include <wtf/MathExtras.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

class GraphicsContext;
class IntRect;
class ScrollbarClient;
class ScrollbarTheme;
class PlatformMouseEvent;

// These match the numbers we use over in WebKit (WebFrameView.m).
const int cScrollbarPixelsPerLineStep = 40;
const int cAmountToKeepWhenPaging = 40;

class Scrollbar : public Widget {
protected:
    Scrollbar(ScrollbarClient*, ScrollbarOrientation, ScrollbarControlSize, ScrollbarTheme* = 0);

public:
    virtual ~Scrollbar();

    // Must be implemented by platforms that can't simply use the Scrollbar base class.  Right now the only platform that is not using the base class is GTK.
    static PassRefPtr<Scrollbar> createNativeScrollbar(ScrollbarClient* client, ScrollbarOrientation orientation, ScrollbarControlSize size);
    
    void setClient(ScrollbarClient* client) { m_client = client; }
    ScrollbarClient* client() const { return m_client; }

    virtual bool isCustomScrollbar() const { return false; }
    ScrollbarOrientation orientation() const { return m_orientation; }
    
    int value() const { return lroundf(m_currentPos); }
    float currentPos() const { return m_currentPos; }
    int pressedPos() const { return m_pressedPos; }
    int visibleSize() const { return m_visibleSize; }
    int totalSize() const { return m_totalSize; }
    int maximum() const { return m_totalSize - m_visibleSize; }        
    ScrollbarControlSize controlSize() const { return m_controlSize; }

    int lineStep() const { return m_lineStep; }
    int pageStep() const { return m_pageStep; }
    float pixelStep() const { return m_pixelStep; }
    
    ScrollbarPart pressedPart() const { return m_pressedPart; }
    ScrollbarPart hoveredPart() const { return m_hoveredPart; }
    virtual void setHoveredPart(ScrollbarPart);
    virtual void setPressedPart(ScrollbarPart);

    void setSteps(int lineStep, int pageStep, int pixelsPerStep = 1);
    bool setValue(int);
    void setProportion(int visibleSize, int totalSize);
    void setPressedPos(int p) { m_pressedPos = p; }

    bool scroll(ScrollDirection, ScrollGranularity, float multiplier = 1.0f);
    
    virtual void paint(GraphicsContext*, const IntRect& damageRect);

    bool enabled() const { return m_enabled; }
    virtual void setEnabled(bool e);

    bool isWindowActive() const;

    // These methods are used for platform scrollbars to give :hover feedback.  They will not get called
    // when the mouse went down in a scrollbar, since it is assumed the scrollbar will start
    // grabbing all events in that case anyway.
    bool mouseMoved(const PlatformMouseEvent&);
    bool mouseExited();
    
    // Used by some platform scrollbars to know when they've been released from capture.
    bool mouseUp();

    bool mouseDown(const PlatformMouseEvent&);

#if PLATFORM(QT)
    // For platforms that wish to handle context menu events.
    // FIXME: This is misplaced.  Normal hit testing should be used to populate a correct
    // context menu.  There's no reason why the scrollbar should have to do it.
    bool contextMenu(const PlatformMouseEvent& event);
#endif

    ScrollbarTheme* theme() const { return m_theme; }

    virtual void setParent(ScrollView*);
    virtual void setFrameRect(const IntRect&);

    virtual void invalidateRect(const IntRect&);
    
    bool suppressInvalidation() const { return m_suppressInvalidation; }
    void setSuppressInvalidation(bool s) { m_suppressInvalidation = s; }

    virtual void styleChanged() { }

    virtual IntRect convertToContainingView(const IntRect&) const;
    virtual IntRect convertFromContainingView(const IntRect&) const;
    
    virtual IntPoint convertToContainingView(const IntPoint&) const;
    virtual IntPoint convertFromContainingView(const IntPoint&) const;

private:
    virtual bool isScrollbar() const { return true; }

protected:
    virtual void updateThumbPosition();
    virtual void updateThumbProportion();

    void autoscrollTimerFired(Timer<Scrollbar>*);
    void startTimerIfNeeded(double delay);
    void stopTimerIfNeeded();
    void autoscrollPressedPart(double delay);
    ScrollDirection pressedPartScrollDirection();
    ScrollGranularity pressedPartScrollGranularity();
    
    void moveThumb(int pos);
    bool setCurrentPos(float pos);

    ScrollbarClient* m_client;
    ScrollbarOrientation m_orientation;
    ScrollbarControlSize m_controlSize;
    ScrollbarTheme* m_theme;
    
    int m_visibleSize;
    int m_totalSize;
    float m_currentPos;
    float m_dragOrigin;
    int m_lineStep;
    int m_pageStep;
    float m_pixelStep;

    ScrollbarPart m_hoveredPart;
    ScrollbarPart m_pressedPart;
    int m_pressedPos;
    
    bool m_enabled;

    Timer<Scrollbar> m_scrollTimer;
    bool m_overlapsResizer;
    
    bool m_suppressInvalidation;
};

}

#endif