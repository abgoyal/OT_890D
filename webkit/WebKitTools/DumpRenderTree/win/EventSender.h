

#ifndef EventSender_h
#define EventSender_h

class DraggingInfo;

typedef const struct OpaqueJSContext* JSContextRef;
typedef struct OpaqueJSValue* JSObjectRef;

JSObjectRef makeEventSender(JSContextRef context);
void replaySavedEvents();

extern DraggingInfo* draggingInfo;

#endif
