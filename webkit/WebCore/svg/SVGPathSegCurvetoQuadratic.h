

#ifndef SVGPathSegCurvetoQuadratic_h
#define SVGPathSegCurvetoQuadratic_h

#if ENABLE(SVG)

#include "SVGPathSeg.h"

namespace WebCore {

    class SVGPathSegCurvetoQuadratic : public SVGPathSeg { 
    public:
        SVGPathSegCurvetoQuadratic(float x, float y, float x1, float y1)
        : SVGPathSeg(), m_x(x), m_y(y), m_x1(x1), m_y1(y1) {}

        virtual String toString() const { return pathSegTypeAsLetter() + String::format(" %.6lg %.6lg %.6lg %.6lg", m_x1, m_y1, m_x, m_y); }

        void setX(float x) { m_x = x; }
        float x() const { return m_x; }

        void setY(float y) { m_y = y; }
        float y() const { return m_y; }

        void setX1(float x1) { m_x1 = x1; }
        float x1() const { return m_x1; }

        void setY1(float y1) { m_y1 = y1; }
        float y1() const { return m_y1; }

    private:
        float m_x;
        float m_y;
        float m_x1;
        float m_y1;
    };

    class SVGPathSegCurvetoQuadraticAbs : public SVGPathSegCurvetoQuadratic {
    public:
        static PassRefPtr<SVGPathSegCurvetoQuadraticAbs> create(float x, float y, float x1, float y1) { return adoptRef(new SVGPathSegCurvetoQuadraticAbs(x, y, x1, y1)); }

        virtual unsigned short pathSegType() const { return PATHSEG_CURVETO_QUADRATIC_ABS; }
        virtual String pathSegTypeAsLetter() const { return "Q"; }

    private:
        SVGPathSegCurvetoQuadraticAbs(float x, float y, float x1, float y1);
    };

    class SVGPathSegCurvetoQuadraticRel : public SVGPathSegCurvetoQuadratic {
    public:
        static PassRefPtr<SVGPathSegCurvetoQuadraticRel> create(float x, float y, float x1, float y1) { return adoptRef(new SVGPathSegCurvetoQuadraticRel(x, y, x1, y1)); }

        virtual unsigned short pathSegType() const { return PATHSEG_CURVETO_QUADRATIC_REL; }
        virtual String pathSegTypeAsLetter() const { return "q"; }

    private:
        SVGPathSegCurvetoQuadraticRel(float x, float y, float x1, float y1);
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif

// vim:ts=4:noet
