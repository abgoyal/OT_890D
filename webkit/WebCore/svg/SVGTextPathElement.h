

#ifndef SVGTextPathElement_h
#define SVGTextPathElement_h

#if ENABLE(SVG)
#include "SVGTextContentElement.h"

#include "SVGURIReference.h"

namespace WebCore {
    enum SVGTextPathMethodType {
        SVG_TEXTPATH_METHODTYPE_UNKNOWN = 0,
        SVG_TEXTPATH_METHODTYPE_ALIGN = 1,
        SVG_TEXTPATH_METHODTYPE_STRETCH = 2
    };

    enum SVGTextPathSpacingType {
        SVG_TEXTPATH_SPACINGTYPE_UNKNOWN = 0,
        SVG_TEXTPATH_SPACINGTYPE_AUTO = 1,
        SVG_TEXTPATH_SPACINGTYPE_EXACT = 2
    };

    class SVGTextPathElement : public SVGTextContentElement,
                               public SVGURIReference {
    public:
        // Forward declare these enums in the w3c naming scheme, for IDL generation
        enum {
            TEXTPATH_METHODTYPE_UNKNOWN = SVG_TEXTPATH_METHODTYPE_UNKNOWN,
            TEXTPATH_METHODTYPE_ALIGN = SVG_TEXTPATH_METHODTYPE_ALIGN,
            TEXTPATH_METHODTYPE_STRETCH = SVG_TEXTPATH_METHODTYPE_STRETCH,
            TEXTPATH_SPACINGTYPE_UNKNOWN = SVG_TEXTPATH_SPACINGTYPE_UNKNOWN,
            TEXTPATH_SPACINGTYPE_AUTO = SVG_TEXTPATH_SPACINGTYPE_AUTO,
            TEXTPATH_SPACINGTYPE_EXACT = SVG_TEXTPATH_SPACINGTYPE_EXACT
        };

        SVGTextPathElement(const QualifiedName&, Document*);
        virtual ~SVGTextPathElement();
 
        virtual void insertedIntoDocument();

        virtual void parseMappedAttribute(MappedAttribute*);
        virtual bool rendererIsNeeded(RenderStyle* style) { return StyledElement::rendererIsNeeded(style); }
        virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);

        bool childShouldCreateRenderer(Node*) const;

    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGTextPathElement, SVGNames::textPathTagString, SVGNames::startOffsetAttrString, SVGLength, StartOffset, startOffset)
        ANIMATED_PROPERTY_DECLARATIONS(SVGTextPathElement, SVGNames::textPathTagString, SVGNames::methodAttrString, int, Method, method)
        ANIMATED_PROPERTY_DECLARATIONS(SVGTextPathElement, SVGNames::textPathTagString, SVGNames::spacingAttrString, int, Spacing, spacing)
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
