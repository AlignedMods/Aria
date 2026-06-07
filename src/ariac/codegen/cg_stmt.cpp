#include "ariac/codegen/codegen.hpp"

namespace ariac {

    void Codegen::gen_block_stmt(Stmt* stmt) {
        BlockStmt& block = stmt->block;

        for (Stmt* stmt : block.stmts) {
            gen_stmt(stmt);
        }
    }

    void Codegen::gen_while_stmt(Stmt* stmt) {
        WhileStmt& wh = stmt->while_;
        
        llvm::BasicBlock* loop_cond = llvm::BasicBlock::Create(*m_active_module_context.context, "loop_cond", m_active_module_context.function);
        llvm::BasicBlock* loop_body = (wh.body->block.stmts.size == 0) ? nullptr : llvm::BasicBlock::Create(*m_active_module_context.context, "loop_body", m_active_module_context.function);
        llvm::BasicBlock* loop_end = llvm::BasicBlock::Create(*m_active_module_context.context, "loop_end", m_active_module_context.function);

        m_active_module_context.builder->CreateBr(loop_cond);

        m_active_module_context.builder->SetInsertPoint(loop_cond);
        llvm::Value* cond = gen_expr(wh.condition);
        m_active_module_context.builder->CreateCondBr(cond, loop_body ? loop_body : loop_cond, loop_end);

        if (loop_body) {
            m_active_module_context.builder->SetInsertPoint(loop_body);
            gen_block_stmt(wh.body);
            m_active_module_context.builder->CreateBr(loop_cond);
        }

        m_active_module_context.builder->SetInsertPoint(loop_end);
    }

    void Codegen::gen_return_stmt(Stmt* stmt) {
        ReturnStmt& ret = stmt->return_;

        if (ret.value) {
            llvm::Value* val = gen_expr(ret.value);

            if (m_ret_type_abi.ret_direct) {  
                m_active_module_context.builder->CreateRet(val);
            } else if (m_ret_type_abi.ret_by_ptr) {
                llvm::Value* ret_ptr = m_active_module_context.function->getArg(0);
                m_active_module_context.builder->CreateStore(val, ret_ptr);
                m_active_module_context.builder->CreateRetVoid();
            } else if (m_ret_type_abi.ret_by_integer) {
                llvm::Type* ty = llvm::Type::getIntNTy(*m_active_module_context.context, static_cast<unsigned>(m_ret_type_abi.int_bits));
                llvm::Value* ret_int = alloca_at_entry(m_active_module_context.function, "ret", ty);
                m_active_module_context.builder->CreateStore(val, ret_int);
                llvm::Value* load = m_active_module_context.builder->CreateLoad(ty, ret_int);
                m_active_module_context.builder->CreateRet(load);
            } else {
                ARIA_UNREACHABLE();
            }
        } else {
            m_active_module_context.builder->CreateRetVoid();
        }
    }

    void Codegen::gen_expr_stmt(Stmt* stmt) {
        Expr* expr = stmt->expr;
        gen_expr(expr);
    }

    void Codegen::gen_decl_stmt(Stmt* stmt)  {
        Decl* decl = stmt->decl;
        gen_decl(decl);
    }

    void Codegen::gen_stmt(Stmt* stmt) {
        if (!stmt->reached) { return; } // Statement is never reached via control flow

        switch (stmt->kind) {
            case StmtKind::Block: return gen_block_stmt(stmt);
            case StmtKind::While: return gen_while_stmt(stmt);
            case StmtKind::Return: return gen_return_stmt(stmt);
            case StmtKind::Expr: return gen_expr_stmt(stmt);
            case StmtKind::Decl: return gen_decl_stmt(stmt);
            default: ARIA_UNREACHABLE();
        }
    }

} // namespace ariac