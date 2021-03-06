

#ifndef V8AbstractEventListener_h
#define V8AbstractEventListener_h

#include "EventListener.h"
#include "OwnHandle.h"
#include <v8.h>

namespace WebCore {

    class Event;
    class Frame;

    // There are two kinds of event listeners: HTML or non-HMTL. onload,
    // onfocus, etc (attributes) are always HTML event handler type; Event
    // listeners added by Window.addEventListener or
    // EventTargetNode::addEventListener are non-HTML type.
    //
    // Why does this matter?
    // WebKit does not allow duplicated HTML event handlers of the same type,
    // but ALLOWs duplicated non-HTML event handlers.
    class V8AbstractEventListener : public EventListener {
    public:
        virtual ~V8AbstractEventListener() { }

        // Returns the owner frame of the listener.
        Frame* frame() { return m_frame; }

        virtual void handleEvent(Event*, bool isWindowEvent);
        void invokeEventHandler(v8::Handle<v8::Context>, Event*, v8::Handle<v8::Value> jsEvent, bool isWindowEvent);

        // Returns the listener object, either a function or an object.
        virtual v8::Local<v8::Object> getListenerObject()
        {
            return v8::Local<v8::Object>::New(m_listener);
        }

        // Dispose listener object and clear the handle.
        void disposeListenerObject();

        virtual bool disconnected() const { return !m_frame; }

        virtual bool isObjectListener() const { return false; }

    protected:
        v8::Persistent<v8::Object> m_listener;

        // Indicates if this is an HTML type listener.
        bool m_isAttribute;

    private:
        V8AbstractEventListener(Frame*, bool isInline);

        virtual v8::Local<v8::Value> callListenerFunction(v8::Handle<v8::Value> jsevent, Event*, bool isWindowEvent) = 0;

        // Get the receiver object to use for event listener call.
        v8::Local<v8::Object> getReceiverObject(Event*, bool isWindowEvent);

        // Frame to which the event listener is attached to. An event listener must be destroyed before its owner frame is
        // deleted. See fast/dom/replaceChild.html
        // FIXME: this could hold m_frame live until the event listener is deleted.
        Frame* m_frame;
        OwnHandle<v8::Context> m_context;

        // Position in the HTML source for HTML event listeners.
        int m_lineNumber;
        int m_columnNumber;

        friend class V8EventListener;
        friend class V8ObjectEventListener;
        friend class V8LazyEventListener;
    };

} // namespace WebCore

#endif // V8AbstractEventListener_h
