

#ifndef ScriptObject_h
#define ScriptObject_h

#include "ScriptValue.h"

#include <v8.h>

namespace WebCore {
    class InspectorBackend;
    class ScriptState;

    class ScriptObject : public ScriptValue {
    public:
        ScriptObject(ScriptState*, v8::Handle<v8::Object>);
        ScriptObject() {};
        virtual ~ScriptObject() {}

        v8::Local<v8::Object> v8Object() const;

        bool set(const String& name, const String&);
        bool set(const char* name, const ScriptObject&);
        bool set(const char* name, const String&);
        bool set(const char* name, double);
        bool set(const char* name, long long);
        bool set(const char* name, int);
        bool set(const char* name, bool);

        static ScriptObject createNew(ScriptState*);
    protected:
        ScriptState* m_scriptState;
    };

    class ScriptGlobalObject {
    public:
        static bool set(ScriptState*, const char* name, const ScriptObject&);
        static bool set(ScriptState*, const char* name, InspectorBackend*);
        static bool get(ScriptState*, const char* name, ScriptObject&);
        static bool remove(ScriptState*, const char* name);
    private:
        ScriptGlobalObject() { }
    };

}

#endif // ScriptObject_h
