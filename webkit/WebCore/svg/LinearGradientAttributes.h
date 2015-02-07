

#ifndef LinearGradientAttributes_h
#define LinearGradientAttributes_h

#include "GradientAttributes.h"

#if ENABLE(SVG)

namespace WebCore {
    struct LinearGradientAttributes : GradientAttributes {
        LinearGradientAttributes()
            : m_x1(0.0)
            , m_y1(0.0)
            , m_x2(1.0)
            , m_y2(0.0)
            , m_x1Set(false)
            , m_y1Set(false)
            , m_x2Set(false)
            , m_y2Set(false)
        {
        }

        double x1() const { return m_x1; }
        double y1() const { return m_y1; }
        double x2() const { return m_x2; }
        double y2() const { return m_y2; }

        void setX1(double value) { m_x1 = value; m_x1Set = true; }
        void setY1(double value) { m_y1 = value; m_y1Set = true; }
        void setX2(double value) { m_x2 = value; m_x2Set = true; }
        void setY2(double value) { m_y2 = value; m_y2Set = true; }

        bool hasX1() const { return m_x1Set; }
        bool hasY1() const { return m_y1Set; }
        bool hasX2() const { return m_x2Set; }
        bool hasY2() const { return m_y2Set; }

    private:
        // Properties
        double m_x1;
        double m_y1;
        double m_x2;
        double m_y2;

        // Property states
        bool m_x1Set : 1;
        bool m_y1Set : 1;
        bool m_x2Set : 1;
        bool m_y2Set : 1;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif

// vim:ts=4:noet