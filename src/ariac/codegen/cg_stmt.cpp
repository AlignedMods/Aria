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

        if (wh.infinite) {
            llvm::BasicBlock* while_body = llvm::BasicBlock::Create(*m_active_module_context.context, "while.body", m_active_module_context.function);
            m_active_module_context.builder->CreateBr(while_body);
            m_active_module_context.builder->SetInsertPoint(while_body);
            m_active_module_context.builder->CreateBr(while_body);
            return;
        }

        LoopKind lk = get_loop_kind_from_cond(wh.condition);
        
        switch (lk) {
            case LoopKind::Never: {
                return;
            }

            case LoopKind::Always: {
                llvm::BasicBlock* while_body = wh.body->block.stmts.size == 0 ? nullptr : llvm::BasicBlock::Create(*m_active_module_context.context, "while.body", m_active_module_context.function);
                llvm::BasicBlock* while_end = llvm::BasicBlock::Create(*m_active_module_context.context, "while.end", m_active_module_context.function);

                push_scope();
                m_scopes.back().loop_start_block = while_body;
                m_scopes.back().loop_end_block = while_end;

                if (while_body) {
                    m_active_module_context.builder->CreateBr(while_body);
                    m_active_module_context.builder->SetInsertPoint(while_body);
                    gen_block_stmt(wh.body);

                    if (wh.body->block.reaches_end) {
                        pop_defers(m_scopes.back());
                        m_active_module_context.builder->CreateBr(while_body);
                    }
                }

                pop_scope();
                m_active_module_context.builder->SetInsertPoint(while_end);
                return;
            }

            case LoopKind::Normal: {
                llvm::BasicBlock* while_cond = llvm::BasicBlock::Create(*m_active_module_context.context, "while.cond", m_active_module_context.function);
                llvm::BasicBlock* while_body = wh.body->block.stmts.size == 0 ? nullptr : llvm::BasicBlock::Create(*m_active_module_context.context, "while.body", m_active_module_context.function);
                llvm::BasicBlock* while_end = llvm::BasicBlock::Create(*m_active_module_context.context, "while.end", m_active_module_context.function);

                push_scope();
                m_scopes.back().loop_start_block = while_cond;
                m_scopes.back().loop_end_block = while_end;

                m_active_module_context.builder->CreateBr(while_cond);
                m_active_module_context.builder->SetInsertPoint(while_cond);
                llvm::Value* cond = gen_expr(wh.condition);
                m_active_module_context.builder->CreateCondBr(cond, while_body ? while_body : while_cond, while_end);

                if (while_body) {
                    m_active_module_context.builder->SetInsertPoint(while_body);
                    gen_block_stmt(wh.body);

                    if (wh.body->block.reaches_end) {
                        pop_defers(m_scopes.back());
                        m_active_module_context.builder->CreateBr(while_cond);
                    }
                }

                pop_scope();
                
                m_active_module_context.builder->SetInsertPoint(while_end);
                return;
            }

            default: ARIA_UNREACHABLE("Invalid loop kind");
        }
    }

    void Codegen::gen_do_while_stmt(Stmt* stmt) {
        DoWhileStmt& d = stmt->do_while;

        llvm::BasicBlock* do_body = llvm::BasicBlock::Create(*m_active_module_context.context, "do.body", m_active_module_context.function);
        llvm::BasicBlock* do_end = llvm::BasicBlock::Create(*m_active_module_context.context, "do.end", m_active_module_context.function);

        m_active_module_context.builder->CreateBr(do_body);

        m_active_module_context.builder->SetInsertPoint(do_body);
        gen_block_stmt(d.body);

        llvm::Value* cond = gen_expr(d.condition);
        m_active_module_context.builder->CreateCondBr(cond, do_body, do_end);

        m_active_module_context.builder->SetInsertPoint(do_end);
    }

    void Codegen::gen_for_stmt(Stmt* stmt) {
        ForStmt& f = stmt->for_;

        if (f.prologue) { gen_decl(f.prologue); }

        if (f.infinite) {
            llvm::BasicBlock* for_body = llvm::BasicBlock::Create(*m_active_module_context.context, "for.body", m_active_module_context.function);

            m_active_module_context.builder->CreateBr(for_body);
            m_active_module_context.builder->SetInsertPoint(for_body);
            if (f.step) { gen_expr(f.step); }
            m_active_module_context.builder->CreateBr(for_body);
            return;
        }

        LoopKind lk = get_loop_kind_from_cond(f.condition);

        switch (lk) {
            case LoopKind::Never: {
                return;
            }

            case LoopKind::Always: {
                llvm::BasicBlock* for_body = f.body->block.stmts.size == 0 ? 
                    llvm::BasicBlock::Create(*m_active_module_context.context, "for.step", m_active_module_context.function) : 
                    llvm::BasicBlock::Create(*m_active_module_context.context, "for.body", m_active_module_context.function);
                llvm::BasicBlock* for_end = llvm::BasicBlock::Create(*m_active_module_context.context, "for.end", m_active_module_context.function);

                push_scope();
                m_scopes.back().loop_start_block = for_body;
                m_scopes.back().loop_end_block = for_end;

                m_active_module_context.builder->CreateBr(for_body);
                m_active_module_context.builder->SetInsertPoint(for_body);
                gen_block_stmt(f.body);

                if (f.body->block.reaches_end) {
                    if (f.step) { gen_expr(f.step); }
                    pop_defers(m_scopes.back());
                    m_active_module_context.builder->CreateBr(for_body);
                }

                pop_scope();
                m_active_module_context.builder->SetInsertPoint(for_end);
                return;
            }

            case LoopKind::Normal: {
                llvm::BasicBlock* for_cond = llvm::BasicBlock::Create(*m_active_module_context.context, "for.cond", m_active_module_context.function);
                llvm::BasicBlock* for_body = f.body->block.stmts.size == 0 ? 
                    llvm::BasicBlock::Create(*m_active_module_context.context, "for.step", m_active_module_context.function) : 
                    llvm::BasicBlock::Create(*m_active_module_context.context, "for.body", m_active_module_context.function);
                llvm::BasicBlock* for_end = llvm::BasicBlock::Create(*m_active_module_context.context, "for.end", m_active_module_context.function);

                push_scope();
                m_scopes.back().loop_start_block = for_body;
                m_scopes.back().loop_end_block = for_end;

                m_active_module_context.builder->CreateBr(for_cond);

                m_active_module_context.builder->SetInsertPoint(for_cond);
                llvm::Value* cond = gen_expr(f.condition);
                m_active_module_context.builder->CreateCondBr(cond, for_body, for_end);

                if (for_body) {
                    m_active_module_context.builder->SetInsertPoint(for_body);
                    gen_block_stmt(f.body);

                    if (f.step) { gen_expr(f.step); }
                    m_active_module_context.builder->CreateBr(for_cond);
                }

                pop_scope();
                m_active_module_context.builder->SetInsertPoint(for_end);
                return;
            }

            default: ARIA_UNREACHABLE("Invalid loop kind");
        }
    }

    void Codegen::gen_if_stmt(Stmt* stmt) {
        IfStmt& i = stmt->if_;

        llvm::BasicBlock* if_body = llvm::BasicBlock::Create(*m_active_module_context.context, "if.body", m_active_module_context.function);
        llvm::BasicBlock* else_body = (i.else_body) ? llvm::BasicBlock::Create(*m_active_module_context.context, "if.else", m_active_module_context.function) : nullptr;
        llvm::BasicBlock* if_end = llvm::BasicBlock::Create(*m_active_module_context.context, "if.end", m_active_module_context.function);

        llvm::Value* cond = gen_expr(i.condition);
        if (else_body) {
            m_active_module_context.builder->CreateCondBr(cond, if_body, else_body);
        } else {
            m_active_module_context.builder->CreateCondBr(cond, if_body, if_end);
        }

        m_active_module_context.builder->SetInsertPoint(if_body);
        gen_block_stmt(i.body);
        if (i.body->block.reaches_end) { m_active_module_context.builder->CreateBr(if_end); }

        if (else_body) {
            m_active_module_context.builder->SetInsertPoint(else_body);
            gen_block_stmt(i.else_body);
            if (i.body->block.reaches_end) { m_active_module_context.builder->CreateBr(if_end); }
        }

        m_active_module_context.builder->SetInsertPoint(if_end);
    }

    void Codegen::gen_break_stmt(Stmt* stmt) {
        for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); it++) {
            if (it->loop_end_block) {
                m_active_module_context.builder->CreateBr(it->loop_end_block);
            }
        }
    }

    void Codegen::gen_continue_stmt(Stmt* stmt) {
        for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); it++) {
            if (it->loop_start_block) {
                m_active_module_context.builder->CreateBr(it->loop_start_block);
            }
        }
    }

    void Codegen::gen_return_stmt(Stmt* stmt) {
        ReturnStmt& ret = stmt->return_;

        for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); it++) {
            pop_defers(*it);
        }

        if (ret.value) {
            llvm::Value* val = gen_expr(ret.value);

            switch (m_ret_type_abi.kind) {
                case ABIRetKind::Direct: {
                    m_active_module_context.builder->CreateRet(val);
                    break;
                }

                case ABIRetKind::Pointer: {
                    llvm::Value* ret_ptr = m_active_module_context.function->getArg(0);
                    m_active_module_context.builder->CreateStore(val, ret_ptr);
                    m_active_module_context.builder->CreateRetVoid();
                    break;
                }

                case ABIRetKind::Integer: {
                    llvm::Type* ty = llvm::Type::getIntNTy(*m_active_module_context.context, static_cast<unsigned>(m_ret_type_abi.int_bits));
                    llvm::Value* ret_int = alloca_at_entry(m_active_module_context.function, "ret", ty);
                    m_active_module_context.builder->CreateStore(val, ret_int);
                    llvm::Value* load = m_active_module_context.builder->CreateLoad(ty, ret_int);
                    m_active_module_context.builder->CreateRet(load);
                    break;
                }

                default: ARIA_UNREACHABLE("Invalid ABIRetTypeInfo");
            }
        } else {
            m_active_module_context.builder->CreateRetVoid();
        }
    }

    void Codegen::gen_defer_stmt(Stmt* stmt) {
        DeferStmt& defer = stmt->defer;
        m_scopes.back().defers.push_back(defer.statement);
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
            case StmtKind::Block: {
                push_scope();
                gen_block_stmt(stmt);
                pop_defers(m_scopes.back());
                pop_scope();
                return;
            }

            case StmtKind::While: return gen_while_stmt(stmt);
            case StmtKind::DoWhile: return gen_do_while_stmt(stmt);
            case StmtKind::For: return gen_for_stmt(stmt);
            case StmtKind::If: return gen_if_stmt(stmt);
            case StmtKind::Break: return gen_break_stmt(stmt);
            case StmtKind::Continue: return gen_continue_stmt(stmt);
            case StmtKind::Return: return gen_return_stmt(stmt);
            case StmtKind::Defer: return gen_defer_stmt(stmt);
            case StmtKind::Expr: return gen_expr_stmt(stmt);
            case StmtKind::Decl: return gen_decl_stmt(stmt);

            default: ARIA_UNREACHABLE("Invalid stmt kind");
        }
    }

    Codegen::LoopKind Codegen::get_loop_kind_from_cond(Expr* cond) {
        if (!cond) { return LoopKind::Always; }

        if (cond->kind == ExprKind::Const) {
            if (cond->const_.boolean) {
                return LoopKind::Always;
            } else {
                return LoopKind::Never;
            }
        }

        return LoopKind::Normal;
    }

} // namespace ariac