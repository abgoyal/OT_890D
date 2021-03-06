

#ifndef InspectorClientAndroid_h
#define InspectorClientAndroid_h

#include "InspectorClient.h"

namespace android {

class InspectorClientAndroid : public InspectorClient {
public:
    virtual ~InspectorClientAndroid() {  }

    virtual void inspectorDestroyed() { delete this; }

    virtual Page* createPage() { return NULL; }

    virtual String localizedStringsURL() { return String(); }

    virtual void showWindow() {}
    virtual void closeWindow() {}

    virtual void attachWindow() {}
    virtual void detachWindow() {}

    virtual void setAttachedWindowHeight(unsigned height) {}

    virtual void highlight(Node*) {}
    virtual void hideHighlight() {}

    virtual void inspectedURLChanged(const String& newURL) {}
    
    // new as of 38068
    virtual void populateSetting(const String&, InspectorController::Setting&) {}
 	virtual void storeSetting(const String&, const InspectorController::Setting&) {}
 	virtual void removeSetting(const String&) {}
    virtual String hiddenPanels() { return String(); }
    virtual void inspectorWindowObjectCleared() {}
};

}

#endif
