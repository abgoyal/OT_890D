

#ifndef Parser_h
#define Parser_h

#include "Debugger.h"
#include "Nodes.h"
#include "SourceProvider.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>

namespace JSC {

    class FunctionBodyNode;
    class ProgramNode;
    class UString;

    template <typename T> struct ParserArenaData : ParserArenaDeletable { T data; };

    class Parser : public Noncopyable {
    public:
        template <class ParsedNode> PassRefPtr<ParsedNode> parse(ExecState*, Debugger*, const SourceCode&, int* errLine = 0, UString* errMsg = 0);
        template <class ParsedNode> PassRefPtr<ParsedNode> reparse(JSGlobalData*, ParsedNode*);
        void reparseInPlace(JSGlobalData*, FunctionBodyNode*);

        void didFinishParsing(SourceElements*, ParserArenaData<DeclarationStacks::VarStack>*, 
                              ParserArenaData<DeclarationStacks::FunctionStack>*, CodeFeatures features, int lastLine, int numConstants);

        ParserArena& arena() { return m_arena; }

    private:
        void parse(JSGlobalData*, int* errLine, UString* errMsg);

        ParserArena m_arena;
        const SourceCode* m_source;
        SourceElements* m_sourceElements;
        ParserArenaData<DeclarationStacks::VarStack>* m_varDeclarations;
        ParserArenaData<DeclarationStacks::FunctionStack>* m_funcDeclarations;
        CodeFeatures m_features;
        int m_lastLine;
        int m_numConstants;
    };

    template <class ParsedNode> PassRefPtr<ParsedNode> Parser::parse(ExecState* exec, Debugger* debugger, const SourceCode& source, int* errLine, UString* errMsg)
    {
        m_source = &source;
        parse(&exec->globalData(), errLine, errMsg);
        RefPtr<ParsedNode> result;
        if (m_sourceElements) {
            result = ParsedNode::create(&exec->globalData(),
                                         m_sourceElements,
                                         m_varDeclarations ? &m_varDeclarations->data : 0, 
                                         m_funcDeclarations ? &m_funcDeclarations->data : 0,
                                         *m_source,
                                         m_features,
                                         m_numConstants);
            result->setLoc(m_source->firstLine(), m_lastLine);
        }

        m_arena.reset();

        m_source = 0;
        m_varDeclarations = 0;
        m_funcDeclarations = 0;

        if (debugger)
            debugger->sourceParsed(exec, source, *errLine, *errMsg);
        return result.release();
    }

    template <class ParsedNode> PassRefPtr<ParsedNode> Parser::reparse(JSGlobalData* globalData, ParsedNode* oldParsedNode)
    {
        m_source = &oldParsedNode->source();
        parse(globalData, 0, 0);
        RefPtr<ParsedNode> result;
        if (m_sourceElements) {
            result = ParsedNode::create(globalData,
                                        m_sourceElements,
                                        m_varDeclarations ? &m_varDeclarations->data : 0, 
                                        m_funcDeclarations ? &m_funcDeclarations->data : 0,
                                        *m_source,
                                        oldParsedNode->features(),
                                        m_numConstants);
            result->setLoc(m_source->firstLine(), m_lastLine);
        }

        m_arena.reset();

        m_source = 0;
        m_varDeclarations = 0;
        m_funcDeclarations = 0;

        return result.release();
    }

} // namespace JSC

#endif // Parser_h
