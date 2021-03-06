

#ifndef WMLDocument_h
#define WMLDocument_h

#if ENABLE(WML)
#include "Document.h"
#include "WMLErrorHandling.h"
#include "WMLPageState.h"

namespace WebCore {

class WMLCardElement;

class WMLDocument : public Document {
public:
    static PassRefPtr<WMLDocument> create(Frame* frame)
    {
        return new WMLDocument(frame);
    }

    virtual ~WMLDocument();

    virtual bool isWMLDocument() const { return true; }
    virtual void finishedParsing();

    bool initialize(bool aboutToFinishParsing = false);

private:
    WMLDocument(Frame*);
    WMLCardElement* m_activeCard;
};

WMLPageState* wmlPageStateForDocument(Document*);

}

#endif
#endif
