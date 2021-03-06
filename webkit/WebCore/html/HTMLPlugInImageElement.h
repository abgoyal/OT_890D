

#ifndef HTMLPlugInImageElement_h
#define HTMLPlugInImageElement_h

#include "HTMLPlugInElement.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class HTMLImageLoader;

class HTMLPlugInImageElement : public HTMLPlugInElement {
public:
    HTMLPlugInImageElement(const QualifiedName& tagName, Document*);
    virtual ~HTMLPlugInImageElement();

    bool isImageType();

    const String& serviceType() const { return m_serviceType; }
    const String& url() const { return m_url; }

protected:
    OwnPtr<HTMLImageLoader> m_imageLoader;
    String m_serviceType;
    String m_url;
};

} // namespace WebCore

#endif // HTMLPlugInImageElement_h
