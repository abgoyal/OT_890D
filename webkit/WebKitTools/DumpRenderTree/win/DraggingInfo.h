

#ifndef DraggingInfo_h
#define DraggingInfo_h

#include <objidl.h>

class DraggingInfo {
public:
    DraggingInfo(IDataObject* object, IDropSource* source)
        : m_object(object)
        , m_source(source)
    {
        m_object->AddRef();
        m_source->AddRef();
    }

    ~DraggingInfo()
    {
        if (m_object)
            m_object->Release();
        m_object = 0;
        if (m_source)
            m_source->Release();
        m_source = 0;
    }

    IDataObject* dataObject() const { return m_object; }
    IDropSource* dropSource() const { return m_source; }

private:
    IDataObject* m_object;
    IDropSource* m_source;
};

#endif // !defined(DraggingInfo_h)
