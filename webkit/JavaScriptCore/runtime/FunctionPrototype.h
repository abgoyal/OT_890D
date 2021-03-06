

#ifndef FunctionPrototype_h
#define FunctionPrototype_h

#include "InternalFunction.h"

namespace JSC {

    class PrototypeFunction;

    class FunctionPrototype : public InternalFunction {
    public:
        FunctionPrototype(ExecState*, PassRefPtr<Structure>);
        void addFunctionProperties(ExecState*, Structure* prototypeFunctionStructure, NativeFunctionWrapper** callFunction, NativeFunctionWrapper** applyFunction);

        static PassRefPtr<Structure> createStructure(JSValue proto)
        {
            return Structure::create(proto, TypeInfo(ObjectType, HasStandardGetOwnPropertySlot));
        }

    private:
        virtual CallType getCallData(CallData&);
    };

} // namespace JSC

#endif // FunctionPrototype_h
