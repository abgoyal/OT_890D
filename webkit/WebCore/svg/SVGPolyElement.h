

#ifndef SVGPolyElement_h
#define SVGPolyElement_h

#if ENABLE(SVG)
#include "SVGAnimatedPoints.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGLangSpace.h"
#include "SVGStyledTransformableElement.h"
#include "SVGTests.h"

namespace WebCore {

    class SVGPolyElement :  public SVGStyledTransformableElement,
                            public SVGTests,
                            public SVGLangSpace,
                            public SVGExternalResourcesRequired,
                            public SVGAnimatedPoints {
    public:
        SVGPolyElement(const QualifiedName&, Document*);
        virtual ~SVGPolyElement();
        
        virtual bool isValid() const { return SVGTests::isValid(); }

        virtual SVGPointList* points() const;
        virtual SVGPointList* animatedPoints() const;

        virtual void parseMappedAttribute(MappedAttribute*); 
        virtual void svgAttributeChanged(const QualifiedName&);

        virtual bool rendererIsNeeded(RenderStyle* style) { return StyledElement::rendererIsNeeded(style); }
        virtual bool supportsMarkers() const { return true; }

        virtual void updateAnimatedSVGAttribute(const String&) const;

    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        mutable RefPtr<SVGPointList> m_points;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
