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

    void Codegen::gen_do_while_stmt(Stmt* stmt) {
        DoWhileStmt& d = stmt->do_while;

        llvm::BasicBlock* do_body = llvm::BasicBlock::Create(*m_active_module_context.context, "do_body", m_active_module_context.function);
        llvm::BasicBlock* do_end = llvm::BasicBlock::Create(*m_active_module_context.context, "do_end", m_active_module_context.function);

        m_active_module_context.builder->CreateBr(do_body);

        m_active_module_context.builder->SetInsertPoint(do_body);
        gen_block_stmt(d.body);

        llvm::Value* cond = gen_expr(d.condition);
        m_active_module_context.builder->CreateCondBr(cond, do_body, do_end);

        m_active_module_context.builder->SetInsertPoint(do_end);
    }

    void Codegen::gen_for_stmt(Stmt* stmt) {
        ForStmt& f = stmt->for_;

        llvm::BasicBlock* for_cond = llvm::BasicBlock::Create(*m_active_module_context.context, "for_cond", m_active_module_context.function);
        llvm::BasicBlock* for_body = (f.body->block.stmts.size == 0) ? nullptr : llvm::BasicBlock::Create(*m_active_module_context.context, "for_body", m_active_module_context.function);
        llvm::BasicBlock* for_step = (f.step) ? llvm::BasicBlock::Create(*m_active_module_context.context, "for_step", m_active_module_context.function) : nullptr;
        llvm::BasicBlock* for_end = llvm::BasicBlock::Create(*m_active_module_context.context, "for_end", m_active_module_context.function);

        if (f.prologue) { gen_decl(f.prologue); }

        m_active_module_context.builder->CreateBr(for_cond);

        m_active_module_context.builder->SetInsertPoint(for_cond);
        if (f.condition) {
            llvm::Value* cond = gen_expr(f.condition);
            m_active_module_context.builder->CreateCondBr(cond, for_body ? for_body : for_cond, for_end);
        }

        if (for_body) {
            m_active_module_context.builder->SetInsertPoint(for_body);
            gen_block_stmt(f.body);

            if (for_step) { m_active_module_context.builder->CreateBr(for_step); }
            else { m_active_module_context.builder->CreateBr(for_cond); }
        }

        if (for_step) {
            m_active_module_context.builder->SetInsertPoint(for_step);
            gen_expr(f.step);
            m_active_module_context.builder->CreateBr(for_cond);
        }

        m_active_module_context.builder->SetInsertPoint(for_end);
    }

    void Codegen::gen_if_stmt(Stmt* stmt) {
        IfStmt& i = stmt->if_;

        llvm::BasicBlock* if_body = llvm::BasicBlock::Create(*m_active_module_context.context, "if_body", m_active_module_context.function);
        llvm::BasicBlock* else_body = (i.else_body) ? llvm::BasicBlock::Create(*m_active_module_context.context, "if_else", m_active_module_context.function) : nullptr;
        llvm::BasicBlock* if_end = llvm::BasicBlock::Create(*m_active_module_context.context, "if_end", m_active_module_context.function);

        llvm::Value* cond = gen_expr(i.condition);
        if (else_body) {
            m_active_module_context.builder->CreateCondBr(cond, if_body, else_body);
        } else {
            m_active_module_context.builder->CreateCondBr(cond, if_body, if_end);
        }

        m_active_module_context.builder->SetInsertPoint(if_body);
        gen_block_stmt(i.body);
        m_active_module_context.builder->CreateBr(if_end);

        if (else_body) {
            m_active_module_context.builder->SetInsertPoint(else_body);
            gen_block_stmt(i.else_body);
            m_active_module_context.builder->CreateBr(if_end);
        }

        m_active_module_context.builder->SetInsertPoint(if_end);
    }

    void Codegen::gen_return_stmt(Stmt* stmt) {
        ReturnStmt& ret = stmt->return_;

        // Handle defers
        for (auto it = m_defers.rbegin(); it != m_defers.rend(); it++) {
            gen_stmt(*it);
        }

        m_defers.clear();

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

    void Codegen::gen_defer_stmt(Stmt* stmt) {
        DeferStmt& defer = stmt->defer;
        m_defers.push_back(defer.statement);
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
            case StmtKind::DoWhile: return gen_do_while_stmt(stmt);
            case StmtKind::For: return gen_for_stmt(stmt);
            case StmtKind::If: return gen_if_stmt(stmt);
            case StmtKind::Return: return gen_return_stmt(stmt);
            case StmtKind::Defer: return gen_defer_stmt(stmt);
            case StmtKind::Expr: return gen_expr_stmt(stmt);
            case StmtKind::Decl: return gen_decl_stmt(stmt);

            default: ARIA_UNREACHABLE();
        }
    }

} // namespace ariac