

#ifndef CommonIdentifiers_h
#define CommonIdentifiers_h

#include "Identifier.h"
#include <wtf/Noncopyable.h>

// MarkedArgumentBuffer of property names, passed to a macro so we can do set them up various
// ways without repeating the list.
#define JSC_COMMON_IDENTIFIERS_EACH_PROPERTY_NAME(macro) \
    macro(__defineGetter__) \
    macro(__defineSetter__) \
    macro(__lookupGetter__) \
    macro(__lookupSetter__) \
    macro(apply) \
    macro(arguments) \
    macro(call) \
    macro(callee) \
    macro(caller) \
    macro(compile) \
    macro(constructor) \
    macro(eval) \
    macro(exec) \
    macro(fromCharCode) \
    macro(global) \
    macro(getPrototypeOf) \
    macro(hasOwnProperty) \
    macro(ignoreCase) \
    macro(index) \
    macro(input) \
    macro(isPrototypeOf) \
    macro(length) \
    macro(message) \
    macro(multiline) \
    macro(name) \
    macro(now) \
    macro(parse) \
    macro(propertyIsEnumerable) \
    macro(prototype) \
    macro(source) \
    macro(test) \
    macro(toExponential) \
    macro(toFixed) \
    macro(toISOString) \
    macro(toJSON) \
    macro(toLocaleString) \
    macro(toPrecision) \
    macro(toString) \
    macro(UTC) \
    macro(valueOf) \
    macro(displayName)

namespace JSC {

    class CommonIdentifiers : public Noncopyable {
    private:
        CommonIdentifiers(JSGlobalData*);
        friend class JSGlobalData;

    public:
        const Identifier nullIdentifier;
        const Identifier emptyIdentifier;
        const Identifier underscoreProto;
        const Identifier thisIdentifier;

#define JSC_IDENTIFIER_DECLARE_PROPERTY_NAME_GLOBAL(name) const Identifier name;
        JSC_COMMON_IDENTIFIERS_EACH_PROPERTY_NAME(JSC_IDENTIFIER_DECLARE_PROPERTY_NAME_GLOBAL)
#undef JSC_IDENTIFIER_DECLARE_PROPERTY_NAME_GLOBAL
    };

} // namespace JSC

#endif // CommonIdentifiers_h
