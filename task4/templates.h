#ifndef PROOF_PARSER_H
#define PROOF_PARSER_H

#include "ast_record.h"

void add_double_neg(std::vector<std::string>& out,
                    ast_expression const& A);

void excl_third(std::vector<std::string>& out,
                ast_expression const& A);

void self_impl(std::vector<std::string>& out,
               ast_expression const& A);

void and_ax(std::vector<std::string>& out,
            ast_expression const& A,
            ast_expression const& B);

void implication_from_negs(std::vector<std::string>& out,
                           ast_expression const& A,
                           ast_expression const& B);

void neg_conj_from_A(std::vector<std::string>& out,
                     ast_expression const& A,
                     ast_expression const& B);

void neg_conj_from_B(std::vector<std::string>& out,
                     ast_expression const& A,
                     ast_expression const& B);

void neg_disj(std::vector<std::string>& out,
              ast_expression const& A,
              ast_expression const& B);

void neg_impl(std::vector<std::string>& out,
              ast_expression const& A,
              ast_expression const& B);

void excl_assumpion(std::vector<std::string>& out,
                    ast_expression const& A,
                    ast_expression const& B);

void deduc_mp(std::vector<std::string>& out,
              std::string const& A,
              std::string const& B,
              std::string const& C);

#endif
