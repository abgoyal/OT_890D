

#ifndef SVGDocumentExtensions_h
#define SVGDocumentExtensions_h

#if ENABLE(SVG)
#include <memory>

#include <wtf/HashSet.h>
#include <wtf/HashMap.h>

#include "StringHash.h"
#include "StringImpl.h"
#include "SVGAnimatedTemplate.h"

namespace WebCore {

class Document;
class EventListener;
class Node;
class String;
class SVGElementInstance;
class SVGStyledElement;
class SVGSVGElement;

class SVGDocumentExtensions {
public:
    SVGDocumentExtensions(Document*);
    ~SVGDocumentExtensions();
    
    void addTimeContainer(SVGSVGElement*);
    void removeTimeContainer(SVGSVGElement*);
    
    void startAnimations();
    void pauseAnimations();
    void unpauseAnimations();

    void reportWarning(const String&);
    void reportError(const String&);

private:
    Document* m_doc; // weak reference
    HashSet<SVGSVGElement*> m_timeContainers; // For SVG 1.2 support this will need to be made more general.
    HashMap<String, HashSet<SVGStyledElement*>*> m_pendingResources;

    SVGDocumentExtensions(const SVGDocumentExtensions&);
    SVGDocumentExtensions& operator=(const SVGDocumentExtensions&);

    template<typename ValueType>
    HashMap<const SVGElement*, HashMap<StringImpl*, ValueType>*>* baseValueMap() const
    {
        static HashMap<const SVGElement*, HashMap<StringImpl*, ValueType>*>* s_baseValueMap = new HashMap<const SVGElement*, HashMap<StringImpl*, ValueType>*>();
        return s_baseValueMap;
    }

public:
    // This HashMap contains a list of pending resources. Pending resources, are such
    // which are referenced by any object in the SVG document, but do NOT exist yet.
    // For instance, dynamically build gradients / patterns / clippers...
    void addPendingResource(const AtomicString& id, SVGStyledElement*);
    bool isPendingResource(const AtomicString& id) const;
    std::auto_ptr<HashSet<SVGStyledElement*> > removePendingResource(const AtomicString& id);

    // Used by the ANIMATED_PROPERTY_* macros
    template<typename ValueType>
    ValueType baseValue(const SVGElement* element, const AtomicString& propertyName) const
    {
        HashMap<StringImpl*, ValueType>* propertyMap = baseValueMap<ValueType>()->get(element);
        if (propertyMap)
            return propertyMap->get(propertyName.impl());

        return SVGAnimatedTypeValue<ValueType>::null();
    }

    template<typename ValueType>
    void setBaseValue(const SVGElement* element, const AtomicString& propertyName, ValueType newValue)
    {
        HashMap<StringImpl*, ValueType>* propertyMap = baseValueMap<ValueType>()->get(element);
        if (!propertyMap) {
            propertyMap = new HashMap<StringImpl*, ValueType>();
            baseValueMap<ValueType>()->set(element, propertyMap);
        }

        propertyMap->set(propertyName.impl(), newValue);
    }

    template<typename ValueType>
    void removeBaseValue(const SVGElement* element, const AtomicString& propertyName)
    {
        HashMap<StringImpl*, ValueType>* propertyMap = baseValueMap<ValueType>()->get(element);
        if (!propertyMap)
            return;

        propertyMap->remove(propertyName.impl());
    }

    template<typename ValueType>
    bool hasBaseValue(const SVGElement* element, const AtomicString& propertyName) const
    {
        HashMap<StringImpl*, ValueType>* propertyMap = baseValueMap<ValueType>()->get(element);
        if (propertyMap)
            return propertyMap->contains(propertyName.impl());

        return false;
    }
};

}

#endif // ENABLE(SVG)
#endif
