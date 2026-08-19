#include "ariac/codegen/codegen.hpp"

namespace ariac {

    void Codegen::gen_compound_stmt(Stmt* stmt) {
        CompoundStmt& block = stmt->compound;

        for (Stmt* stmt : block.stmts) {
            gen_stmt(stmt);
        }

        gen_stmt_chain(block.cleanup);
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
                llvm::BasicBlock* while_body = wh.body->compound.stmts.size == 0 ? nullptr : llvm::BasicBlock::Create(*m_active_module_context.context, "while.body", m_active_module_context.function);
                llvm::BasicBlock* while_end = llvm::BasicBlock::Create(*m_active_module_context.context, "while.end", m_active_module_context.function);

                wh.backend.continue_block = while_body;
                wh.backend.end_block = while_end;

                if (while_body) {
                    m_active_module_context.builder->CreateBr(while_body);
                    m_active_module_context.builder->SetInsertPoint(while_body);
                    gen_compound_stmt(wh.body);

                    if (!m_active_module_context.builder->GetInsertBlock()->getTerminator()) {
                        m_active_module_context.builder->CreateBr(while_body);
                    }
                }

                m_active_module_context.builder->SetInsertPoint(while_end);
                return;
            }

            case LoopKind::Normal: {
                llvm::BasicBlock* while_cond = llvm::BasicBlock::Create(*m_active_module_context.context, "while.cond", m_active_module_context.function);
                llvm::BasicBlock* while_body = wh.body->compound.stmts.size == 0 ? nullptr : llvm::BasicBlock::Create(*m_active_module_context.context, "while.body", m_active_module_context.function);
                llvm::BasicBlock* while_end = llvm::BasicBlock::Create(*m_active_module_context.context, "while.end", m_active_module_context.function);

                wh.backend.continue_block = while_cond;
                wh.backend.end_block = while_end;

                m_active_module_context.builder->CreateBr(while_cond);
                m_active_module_context.builder->SetInsertPoint(while_cond);
                llvm::Value* cond = gen_cond(wh.condition);
                m_active_module_context.builder->CreateCondBr(cond, while_body ? while_body : while_cond, while_end);

                if (while_body) {
                    m_active_module_context.builder->SetInsertPoint(while_body);
                    gen_compound_stmt(wh.body);

                    if (!m_active_module_context.builder->GetInsertBlock()->getTerminator()) {
                        m_active_module_context.builder->CreateBr(while_cond);
                    }
                }
                
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
        gen_compound_stmt(d.body);

        llvm::Value* cond = gen_cond(d.condition);
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
                llvm::BasicBlock* for_body = f.body->compound.stmts.size == 0 ? 
                    llvm::BasicBlock::Create(*m_active_module_context.context, "for.step", m_active_module_context.function) : 
                    llvm::BasicBlock::Create(*m_active_module_context.context, "for.body", m_active_module_context.function);
                llvm::BasicBlock* for_end = llvm::BasicBlock::Create(*m_active_module_context.context, "for.end", m_active_module_context.function);

                f.backend.continue_block = for_body;
                f.backend.end_block = for_end;

                m_active_module_context.builder->CreateBr(for_body);
                m_active_module_context.builder->SetInsertPoint(for_body);
                gen_compound_stmt(f.body);

                if (!m_active_module_context.builder->GetInsertBlock()->getTerminator()) {
                    if (f.step) { gen_expr(f.step); }
                    m_active_module_context.builder->CreateBr(for_body);
                }

                m_active_module_context.builder->SetInsertPoint(for_end);
                return;
            }

            case LoopKind::Normal: {
                llvm::BasicBlock* for_cond = llvm::BasicBlock::Create(*m_active_module_context.context, "for.cond", m_active_module_context.function);
                llvm::BasicBlock* for_body = f.body->compound.stmts.size == 0 ? 
                    llvm::BasicBlock::Create(*m_active_module_context.context, "for.step", m_active_module_context.function) : 
                    llvm::BasicBlock::Create(*m_active_module_context.context, "for.body", m_active_module_context.function);
                llvm::BasicBlock* for_step = f.step ? create_block("for.step") : nullptr;
                llvm::BasicBlock* for_end = llvm::BasicBlock::Create(*m_active_module_context.context, "for.end", m_active_module_context.function);

                f.backend.continue_block = for_step ? for_step : for_cond;
                f.backend.end_block = for_end;

                m_active_module_context.builder->CreateBr(for_cond);

                m_active_module_context.builder->SetInsertPoint(for_cond);
                llvm::Value* cond = gen_cond(f.condition);
                m_active_module_context.builder->CreateCondBr(cond, for_body, for_end);

                if (for_body) {
                    m_active_module_context.builder->SetInsertPoint(for_body);
                    gen_compound_stmt(f.body);

                    if (!m_active_module_context.builder->GetInsertBlock()->getTerminator()) {
                        m_active_module_context.builder->CreateBr(reinterpret_cast<llvm::BasicBlock*>(f.backend.continue_block));
                    }
                }

                if (for_step) {
                    m_active_module_context.builder->SetInsertPoint(for_step);
                    gen_expr(f.step);
                    m_active_module_context.builder->CreateBr(for_cond);
                }

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

        llvm::Value* cond = gen_cond(i.condition);
        if (else_body) {
            m_active_module_context.builder->CreateCondBr(cond, if_body, else_body);
        } else {
            m_active_module_context.builder->CreateCondBr(cond, if_body, if_end);
        }

        m_active_module_context.builder->SetInsertPoint(if_body);
        gen_compound_stmt(i.body);
        if (!m_active_module_context.builder->GetInsertBlock()->getTerminator()) { m_active_module_context.builder->CreateBr(if_end); }

        if (else_body) {
            m_active_module_context.builder->SetInsertPoint(else_body);
            gen_compound_stmt(i.else_body);
            if (!m_active_module_context.builder->GetInsertBlock()->getTerminator()) { m_active_module_context.builder->CreateBr(if_end); }
        }

        m_active_module_context.builder->SetInsertPoint(if_end);
    }

    void Codegen::gen_switch_stmt(Stmt* stmt) {
        SwitchStmt& s = stmt->switch_;

        llvm::BasicBlock* switch_end = llvm::BasicBlock::Create(*m_active_module_context.context, "switch.end", m_active_module_context.function);
        s.backend.end_block = switch_end;
        llvm::Value* val = gen_expr(s.expression);

        // No cases, nothing to do
        if (s.cases.size == 0) {
            return;
        }

        analyze_switch_cases(s);

        if (s.expression->type->is_typeid()) {
            size_t i = 0;
            for (Stmt* case_ : s.cases) {
                CaseStmt& c = case_->case_;

                llvm::BasicBlock* switch_case = reinterpret_cast<llvm::BasicBlock*>(c.backend.entry_block);
                llvm::BasicBlock* switch_body = reinterpret_cast<llvm::BasicBlock*>(c.backend.body_block);

                if (i == 0) { m_active_module_context.builder->CreateBr(switch_case); }
                m_active_module_context.builder->SetInsertPoint(switch_case);

                llvm::Value* cond_lhs = val;
                llvm::Value* cond_rhs = gen_expr(c.condition);
                llvm::Value* cond = m_active_module_context.builder->CreateICmpEQ(cond_lhs, cond_rhs, "eq");

                m_active_module_context.builder->CreateCondBr(cond, switch_body, reinterpret_cast<llvm::BasicBlock*>(c.backend.fail_block));

                m_active_module_context.builder->SetInsertPoint(switch_body);

                gen_compound_stmt(c.body);
                if (!m_active_module_context.builder->GetInsertBlock()->getTerminator()) { m_active_module_context.builder->CreateBr(switch_end); }

                i++;
            }

            m_active_module_context.builder->SetInsertPoint(switch_end);
            return;
        }

        llvm::SwitchInst* si = m_active_module_context.builder->CreateSwitch(val, switch_end, static_cast<unsigned>(s.cases.size));

        for (Stmt* case_ : s.cases) {
            ARIA_ASSERT(case_->kind == StmtKind::Case, "Invalid case stmt");
            CaseStmt& c = case_->case_;

            llvm::BasicBlock* switch_case = reinterpret_cast<llvm::BasicBlock*>(c.backend.entry_block);
            
            m_active_module_context.builder->SetInsertPoint(switch_case);
            gen_compound_stmt(c.body);
            if (!m_active_module_context.builder->GetInsertBlock()->getTerminator()) {
                m_active_module_context.builder->CreateBr(switch_end);
            }

            llvm::ConstantInt* cond = llvm::dyn_cast<llvm::ConstantInt>(gen_expr(c.condition));
            si->addCase(cond, switch_case);
        }

        m_active_module_context.builder->SetInsertPoint(switch_end);
    }

    void Codegen::gen_break_stmt(Stmt* stmt) {
        void* block = nullptr;
        switch (stmt->break_.target->kind) {
            case StmtKind::While: block = stmt->break_.target->while_.backend.end_block; break;
            case StmtKind::DoWhile: block = stmt->break_.target->do_while.backend.end_block; break;
            case StmtKind::For: block = stmt->break_.target->for_.backend.end_block; break;

            default: ARIA_UNREACHABLE("Invalid stmt kind");
        }

        gen_stmt_chain(stmt->break_.cleanup);
        ARIA_ASSERT(block, "Block must be set");
        m_active_module_context.builder->CreateBr(reinterpret_cast<llvm::BasicBlock*>(block));
    }

    void Codegen::gen_continue_stmt(Stmt* stmt) {
        void* block = nullptr;
        switch (stmt->break_.target->kind) {
            case StmtKind::While: block = stmt->break_.target->while_.backend.continue_block; break;
            case StmtKind::DoWhile: block = stmt->break_.target->do_while.backend.continue_block; break;
            case StmtKind::For: block = stmt->break_.target->for_.backend.continue_block; break;

            default: ARIA_UNREACHABLE("Invalid stmt kind");
        }

        gen_stmt_chain(stmt->continue_.cleanup);
        ARIA_ASSERT(block, "Block must be set");
        m_active_module_context.builder->CreateBr(reinterpret_cast<llvm::BasicBlock*>(block));
    }

    void Codegen::gen_nextcase_stmt(Stmt* stmt) {
        ARIA_ASSERT(stmt->nextcase.target->kind == StmtKind::Case, "Invalid nextcase target");
        void* block = stmt->nextcase.target->case_.backend.entry_block;
        ARIA_ASSERT(block, "block must be set");
        gen_stmt_chain(stmt->continue_.cleanup);
        m_active_module_context.builder->CreateBr(reinterpret_cast<llvm::BasicBlock*>(block));
    }

    void Codegen::gen_return_stmt(Stmt* stmt) {
        ReturnStmt& ret = stmt->return_;

        if (ret.value) {
            llvm::Value* val = gen_expr(ret.value);
            gen_stmt_chain(ret.cleanup);

            switch (m_ret_type_abi.kind) {
                case ABIRetKind::Direct: {
                    if (val->getType()->isIntegerTy(1)) {
                        val = m_active_module_context.builder->CreateZExt(val, m_active_module_context.builder->getInt8Ty(), "zext");
                    }

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

        ARIA_ASSERT(m_active_module_context.builder->GetInsertBlock()->getTerminator(), "Should have a terminator");
    }

    void Codegen::gen_defer_stmt(Stmt* stmt) {
        DeferStmt& defer = stmt->defer;
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
            case StmtKind::Nop: return;
            case StmtKind::Compound: return gen_compound_stmt(stmt);
            case StmtKind::While: return gen_while_stmt(stmt);
            case StmtKind::DoWhile: return gen_do_while_stmt(stmt);
            case StmtKind::For: return gen_for_stmt(stmt);
            case StmtKind::If: return gen_if_stmt(stmt);
            case StmtKind::Switch: return gen_switch_stmt(stmt);
            case StmtKind::Break: return gen_break_stmt(stmt);
            case StmtKind::Continue: return gen_continue_stmt(stmt);
            case StmtKind::Nextcase: return gen_nextcase_stmt(stmt);
            case StmtKind::Return: return gen_return_stmt(stmt);
            case StmtKind::Defer: return gen_defer_stmt(stmt);
            case StmtKind::Expr: return gen_expr_stmt(stmt);
            case StmtKind::Decl: return gen_decl_stmt(stmt);

            default: ARIA_UNREACHABLE("Invalid stmt kind");
        }
    }

    void Codegen::gen_stmt_chain(Stmt* stmt) {
        while (stmt) {
            gen_stmt(stmt);
            stmt = stmt->next;
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

    void Codegen::analyze_switch_cases(SwitchStmt& s) {
        for (Stmt* case_ : s.cases) {
            case_->case_.backend.entry_block = create_block("switch.case");

            if (s.expression->type->is_typeid()) {
                case_->case_.backend.body_block = create_block("switch.case.body");
            }
        }

        if (!s.expression->type->is_typeid()) { return; }

        size_t i = 0;
        for (Stmt* case_ : s.cases) {
            CaseStmt& c = case_->case_;
            
            if (i + 1 == s.cases.size) {
                c.backend.fail_block = s.backend.end_block;
            } else {
                c.backend.fail_block = s.cases.items[i + 1]->case_.backend.entry_block;
            }
            i++;
        }
    }

} // namespace ariac