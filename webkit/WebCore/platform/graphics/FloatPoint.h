

#ifndef FloatPoint_h
#define FloatPoint_h

#include "FloatSize.h"
#include "IntPoint.h"
#include <wtf/MathExtras.h>
#include <wtf/Platform.h>

#if PLATFORM(CG)
typedef struct CGPoint CGPoint;
#endif

#if PLATFORM(MAC)
#ifdef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
typedef struct CGPoint NSPoint;
#else
typedef struct _NSPoint NSPoint;
#endif
#endif

#if PLATFORM(QT)
#include "qglobal.h"
QT_BEGIN_NAMESPACE
class QPointF;
QT_END_NAMESPACE
#endif

#if PLATFORM(SKIA)
struct SkPoint;
#endif

namespace WebCore {

class TransformationMatrix;
class IntPoint;

class FloatPoint {
public:
    FloatPoint() : m_x(0), m_y(0) { }
    FloatPoint(float x, float y) : m_x(x), m_y(y) { }
    FloatPoint(const IntPoint&);

    static FloatPoint narrowPrecision(double x, double y);

    float x() const { return m_x; }
    float y() const { return m_y; }

    void setX(float x) { m_x = x; }
    void setY(float y) { m_y = y; }
    void move(float dx, float dy) { m_x += dx; m_y += dy; }

#if PLATFORM(CG)
    FloatPoint(const CGPoint&);
    operator CGPoint() const;
#endif

#if PLATFORM(MAC) && !defined(NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES)
    FloatPoint(const NSPoint&);
    operator NSPoint() const;
#endif

#if PLATFORM(QT)
    FloatPoint(const QPointF&);
    operator QPointF() const;
#endif

#if (PLATFORM(SKIA) || PLATFORM(SGL))
    operator SkPoint() const;
    FloatPoint(const SkPoint&);
#endif

    FloatPoint matrixTransform(const TransformationMatrix&) const;

private:
    float m_x, m_y;
};


inline FloatPoint& operator+=(FloatPoint& a, const FloatSize& b)
{
    a.move(b.width(), b.height());
    return a;
}

inline FloatPoint& operator-=(FloatPoint& a, const FloatSize& b)
{
    a.move(-b.width(), -b.height());
    return a;
}

inline FloatPoint operator+(const FloatPoint& a, const FloatSize& b)
{
    return FloatPoint(a.x() + b.width(), a.y() + b.height());
}

inline FloatSize operator-(const FloatPoint& a, const FloatPoint& b)
{
    return FloatSize(a.x() - b.x(), a.y() - b.y());
}

inline FloatPoint operator-(const FloatPoint& a, const FloatSize& b)
{
    return FloatPoint(a.x() - b.width(), a.y() - b.height());
}

inline bool operator==(const FloatPoint& a, const FloatPoint& b)
{
    return a.x() == b.x() && a.y() == b.y();
}

inline bool operator!=(const FloatPoint& a, const FloatPoint& b)
{
    return a.x() != b.x() || a.y() != b.y();
}

inline IntPoint roundedIntPoint(const FloatPoint& p)
{
    return IntPoint(static_cast<int>(roundf(p.x())), static_cast<int>(roundf(p.y())));
}

}

#endif
