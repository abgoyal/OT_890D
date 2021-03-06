

#ifndef SVGPointLightSource_h
#define SVGPointLightSource_h

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "FloatPoint3D.h"
#include "SVGLightSource.h"

namespace WebCore {

    class PointLightSource : public LightSource {
    public:
        PointLightSource(const FloatPoint3D& position)
            : LightSource(LS_POINT)
            , m_position(position)
        { }

        const FloatPoint3D& position() const { return m_position; }

        virtual TextStream& externalRepresentation(TextStream&) const;

    private:
        FloatPoint3D m_position;
    };

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(FILTERS)

#endif // SVGPointLightSource_h
