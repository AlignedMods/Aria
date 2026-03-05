#pragma once

#include "black_lua/internal/allocator.hpp"
#include "black_lua/internal/compiler/core/source_location.hpp"
#include "black_lua/internal/compiler/lexer/tokens.hpp"
#include "black_lua/internal/vm/op_codes.hpp"

namespace BlackLua::Internal {

    struct Stmt;

    struct CompilerError {
        size_t Line = 0; size_t Column = 0;
        size_t StartLine = 0; size_t StartColumn = 0;
        size_t EndLine = 0; size_t EndColumn = 0;
        std::string Error;
    };

    class CompilationContext {
    public:
        inline CompilationContext(const std::string& source)
            : m_Allocator(new Allocator(10 * 1024 * 1024)), m_SourceCode(source) {}

        inline CompilationContext(const CompilationContext& other) = delete; // Disallow copying
        inline CompilationContext(const CompilationContext&& other) = delete; // Disallow moving

        inline ~CompilationContext() { delete m_Allocator; }

        template <typename T>
        inline T* Allocate() {
            return m_Allocator->AllocateNamed<T>();
        }

        template <typename T, typename... Args>
        inline T* Allocate(Args&&... args) {
            return m_Allocator->AllocateNamed<T>(std::forward<Args>(args)...);
        }

        inline void* AllocateSized(size_t size) {
            return m_Allocator->Allocate(size);
        }

        inline std::string& GetSourceCode() { return m_SourceCode; }
        inline const std::string& GetSourceCode() const { return m_SourceCode; }

        inline Tokens& GetTokens() { return m_Tokens; }
        inline const Tokens& GetTokens() const { return m_Tokens; }
        inline void SetTokens(const Tokens& tokens) { m_Tokens = tokens; }

        inline Stmt* GetRootASTNode() { return m_RootASTNode; }
        inline const Stmt* GetRootASTNode() const { return m_RootASTNode; }
        inline void SetRootASTNode(Stmt* node) { m_RootASTNode = node; }

        inline std::vector<OpCode>& GetOpCodes() { return m_OpCodes; }
        inline const std::vector<OpCode>& GetOpCodes() const { return m_OpCodes; }
        inline void SetOpCodes(const std::vector<OpCode>& opcodes) { m_OpCodes = opcodes; }

        inline std::vector<CompilerError>& GetCompilerErrors() { return m_CompilerErrors; }
        inline const std::vector<CompilerError>& GetCompilerErrors() const { return m_CompilerErrors; }

        inline void ReportCompilerError(SourceLocation loc, SourceRange range, const std::string& error) {
            CompilerError e;
            e.Line = loc.Line;
            e.Column = loc.Column;
            e.StartLine = range.Start.Line;
            e.StartColumn = range.Start.Column;
            e.EndLine = range.End.Line;
            e.EndColumn = range.End.Column;
            m_CompilerErrors.push_back(e);
        }
    
        void Compile();

        void Lex();
        void Parse();
        void Analyze();
        void Emit();

    private:
        Allocator* m_Allocator = nullptr;

        // Data for this compilation unit
        std::string m_SourceCode;
        Tokens m_Tokens;
        Stmt* m_RootASTNode;
        std::vector<OpCode> m_OpCodes;

        std::vector<CompilerError> m_CompilerErrors;
    };

} // namespace BlackLua::Internal
