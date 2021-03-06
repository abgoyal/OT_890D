

#ifndef CollectionCache_h
#define CollectionCache_h

#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WebCore {

class AtomicStringImpl;
class Element;

struct CollectionCache {
    CollectionCache();
    CollectionCache(const CollectionCache&);
    CollectionCache& operator=(const CollectionCache& other)
    {
        CollectionCache tmp(other);    
        swap(tmp);
        return *this;
    }
    ~CollectionCache();

    void reset();
    void swap(CollectionCache&);

    typedef HashMap<AtomicStringImpl*, Vector<Element*>*> NodeCacheMap;

    unsigned version;
    Element* current;
    unsigned position;
    unsigned length;
    int elementsArrayPosition;
    NodeCacheMap idCache;
    NodeCacheMap nameCache;
    bool hasLength;
    bool hasNameCache;

private:
    static void copyCacheMap(NodeCacheMap&, const NodeCacheMap&);
};

} // namespace

#endif
