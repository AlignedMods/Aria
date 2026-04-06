#pragma once

#include "aria/internal/allocator.hpp"
#include "aria/internal/compiler/core/source_location.hpp"
#include "aria/internal/compiler/lexer/tokens.hpp"
#include "aria/internal/vm/op_codes.hpp"
#include "aria/internal/compiler/reflection/compiler_reflection.hpp"

namespace Aria::Internal {

    struct Stmt;
    struct Decl;

    class Lexer;
    class Parser;
    class TypeChecker;
    class SemanticAnalyzer;
    class Emitter;
    class Linker;

    struct CompilerError {
        size_t Line = 0; size_t Column = 0;
        size_t StartLine = 0; size_t StartColumn = 0;

        size_t EndLine = 0; size_t EndColumn = 0;
        std::string Error;
    };

    struct CompilationUnit {
        inline CompilationUnit(const std::string& source)
            : Source(source) {}

        std::string Source;
        std::vector<Token> Tokens;
        Stmt* RootASTNode = nullptr;
        std::vector<OpCode> OpCodes;

        CompilerReflectionData ReflectionData;
        std::vector<CompilerError> Errors;

        std::vector<Decl*> Globals;
        std::vector<Decl*> Funcs;
        std::vector<Decl*> Structs;

        size_t Index = 0;
    };

    class CompilationContext {
    public:
        inline CompilationContext()
            : m_Allocator(new Allocator(10 * 1024 * 1024)) {}

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

        inline std::string& GetSourceCode() { return m_ActiveCompUnit->Source; }
        inline const std::string& GetSourceCode() const {  return m_ActiveCompUnit->Source; }

        inline Tokens& GetTokens() { return m_ActiveCompUnit->Tokens; }
        inline const Tokens& GetTokens() const { return m_ActiveCompUnit->Tokens; }
        inline void SetTokens(const Tokens& tokens) { m_ActiveCompUnit->Tokens = tokens; }

        inline Stmt* GetRootASTNode() { return m_ActiveCompUnit->RootASTNode; }
        inline const Stmt* GetRootASTNode() const { return m_ActiveCompUnit->RootASTNode; }
        inline void SetRootASTNode(Stmt* node) { m_ActiveCompUnit->RootASTNode = node; }

        inline std::vector<OpCode>& GetOpCodes() { return m_ActiveCompUnit->OpCodes; }
        inline const std::vector<OpCode>& GetOpCodes() const { return m_ActiveCompUnit->OpCodes; }
        inline void SetOpCodes(const std::vector<OpCode>& opcodes) { m_ActiveCompUnit->OpCodes = opcodes; }

        inline CompilerReflectionData& GetReflectionData() { return m_ActiveCompUnit->ReflectionData; }
        inline const CompilerReflectionData& GetReflectionData() const { return m_ActiveCompUnit->ReflectionData; }
        inline void SetReflectionData(const CompilerReflectionData& data) { m_ActiveCompUnit->ReflectionData = data; }

        inline std::vector<CompilerError>& GetCompilerErrors() { return m_ActiveCompUnit->Errors; }
        inline const std::vector<CompilerError>& GetCompilerErrors() const { return m_ActiveCompUnit->Errors; }

        inline CompilationUnit* GetCompilationUnit() { return m_ActiveCompUnit; }
        inline const CompilationUnit* GetCompilationUnit() const { return m_ActiveCompUnit; }

        inline void ReportCompilerError(SourceLocation loc, SourceRange range, const std::string& error) {
            CompilerError e;
            e.Line = loc.Line;
            e.Column = loc.Column;
            e.StartLine = range.Start.Line;
            e.StartColumn = range.Start.Column;
            e.EndLine = range.End.Line;
            e.EndColumn = range.End.Column;
            e.Error = error;
            m_ActiveCompUnit->Errors.push_back(e);
        }
    
        void Compile(const std::string& source);

        void Lex();
        void Parse();
        void Analyze();
        void Emit();
        void Link();

    private:
        Allocator* m_Allocator = nullptr;
        std::vector<CompilationUnit> m_CompilationUnits;
        CompilationUnit* m_ActiveCompUnit = nullptr;

        friend class Lexer;
        friend class Parser;
        friend class TypeChecker;
        friend class SemanticAnalyzer;
        friend class Emitter;
        friend class Linker;
    };

} // namespace Aria::Internal
