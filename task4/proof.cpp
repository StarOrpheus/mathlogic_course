#include "proof.h"
#include "templates.h"

#include <iostream>
#include <map>
#include <algorithm>
#include <future>
#include <sstream>

static inline
void deduct(std::vector<std::string>& proof,
            ast_record& alha,
            hypotesis_set const& hyp_set)
{
    using hash_to_record_t = std::unordered_map<hash_t, ast_record*, hash_t_hash>;
    using ast_set_by_hash_t = std::unordered_map<hash_t, std::unordered_map<hash_t, ast_record*, hash_t_hash>, hash_t_hash>;
    using all_ast_trees_t = std::vector<std::unique_ptr<ast_record>>;

    hash_to_record_t                        proven_by_hash;
    ast_set_by_hash_t                       proven_impl_by_right_subtree_hash;
    all_ast_trees_t                         all_asts;
    std::map<hash_t, std::string>           right_tr;

    std::vector<std::string> result;
    result.reserve(proof.size() * 2);

    auto&& alpha = *std::get<0>(alha.ast);
    auto&& alpha_str = to_string(alpha);

    for (auto&& line : proof)
    {
        std::string_view line_view(line);
        all_asts.push_back(std::make_unique<ast_record>(parse_expr(line_view)));
        auto ast_rec = all_asts.back().get();

        if (check_if_axiom(std::get<ast_expr_ptr>(ast_rec->ast).get())
            || hyp_set.mp.count(line) || hyp_set.mp.count(line.substr(1)))
        {
            result.push_back(line);
            result.push_back("(" + line + ") -> (" + to_string(alpha) + " -> (" + line + "))");
            result.push_back(to_string(alpha) + " -> (" + line + ")");
        }
        else if (ast_rec->hashcode == alha.hashcode)
        {
            self_impl(result, alpha);
        }
        else
        {
            bool modus_ponens_found = false;
            auto it = proven_impl_by_right_subtree_hash.find(ast_rec->hashcode);
            if (it != proven_impl_by_right_subtree_hash.end())
            {
                for (auto hash_and_parent : it->second)
                {
                    auto it2 = proven_by_hash.find(*hash_and_parent.second->l_hash);
                    if (it2 == proven_by_hash.end()) continue;

                    ast_rec->modus_ponens_deps = std::make_pair(hash_and_parent.second, it2->second);
                    ast_rec->mp_subtree_size = hash_and_parent.second->mp_subtree_size
                                             + it2->second->mp_subtree_size
                                             + 1;
                    modus_ponens_found = true;
                    break;
                }
            }

            if (is_op(std::get<0>(ast_rec->ast).get(), operation_type::IMPL))
                right_tr[ast_rec->hashcode] = to_string(*subtree(std::get<0>(ast_rec->ast).get(), 1));

            assert(modus_ponens_found && "WAT");
            auto rit = right_tr.find(ast_rec->modus_ponens_deps->first->hashcode);
            assert(rit != right_tr.end());

            deduc_mp(result, alpha_str,
                     std::get<1>(ast_rec->modus_ponens_deps->second->ast),
                     rit->second);
        }

        proven_by_hash[ast_rec->hashcode] = ast_rec;
        if (is_op(std::get<0>(ast_rec->ast).get(), operation_type::IMPL))
        {
            proven_impl_by_right_subtree_hash[subtree(std::get<0>(ast_rec->ast).get(), 1)->hashcode][ast_rec->hashcode] = ast_rec;
            right_tr[ast_rec->hashcode] = to_string(*subtree(std::get<0>(ast_rec->ast).get(), 1));
        }

        {
            std::stringstream kek;
            kek << *std::get<0>(ast_rec->ast);
            ast_rec->ast = kek.str();
        }
    }

    proof = std::move(result);
}

static inline
void bruteforce(std::vector<std::string>& out,
                ast_expression* ast,
                hypotesis_set const& hyp_set)
{
    assert(is_valid(ast, hyp_set));

    hypotesis_set tmp(hyp_set.varnames);
    tmp.mp = hyp_set.mp;

    std::vector<std::string> proof1, proof2;

    auto&& varname = *std::find_if_not(hyp_set.varnames.begin(),
                                      hyp_set.varnames.end(),
                                      [&tmp] (std::string const& var) { return tmp.mp.count(var); });

    assert(!tmp.mp.count(varname));
    tmp.mp[varname] = false;
    echo_proof(proof1, ast, tmp);

    tmp.mp[varname] = true;
    echo_proof(proof2, ast, tmp);

    ast_record ast_rc(std::make_unique<ast_expression>(varname));

    auto it = tmp.mp.find(varname);
    assert(it != tmp.mp.end());
    tmp.mp.erase(it);

    deduct(proof1, ast_rc, tmp);

    ast_rc = negated(std::move(std::get<0>(ast_rc.ast)));
    deduct(proof2, ast_rc, tmp);

    for (auto&& line : proof1)
        out.push_back(std::move(line));
    for (auto&& line : proof2)
        out.push_back(std::move(line));
    ast_rc = take_off_negated(std::move(std::get<0>(ast_rc.ast)));

    excl_third(out, *std::get<0>(ast_rc.ast));

    excl_assumpion(out, *std::get<0>(ast_rc.ast), *ast);
}

void echo_proof(std::vector<std::string>& out,
                ast_expression* ast,
                hypotesis_set const& hyp_set)
{
    if (check_if_axiom(ast))
    {
        out.push_back(to_string(*ast));
        return;
    }

    assert(is_valid(ast, hyp_set));
    using op_t = operation_type;

    switch (ast->content.index())
    {
    case 0:
    {
        if (is_op(ast, op_t::NEG)
            && is_op(subtree(ast, 0), op_t::NEG))
        {
            auto chld = subtree(subtree(ast, 0), 0);

            echo_proof(out, chld, hyp_set);
            add_double_neg(out, *chld);
            return;
        }

        if (is_op(ast, op_t::NEG)
            && subtree(ast, 0)->content.index() == 1)
        {
            auto chld = subtree(ast, 0);
            auto&& varname = std::get<1>(chld->content);

            auto it = hyp_set.mp.find(varname);
            assert(it != hyp_set.mp.end() && "Improvable variable found!");
            assert(it->second && "Negated variable found, but not negated in hyp set");

            out.push_back("!" + varname);
            return;
        }

        if (is_op(ast, op_t::CONJ))
        {
            auto A = subtree(ast, 0);
            auto B = subtree(ast, 1);

            echo_proof(out, A, hyp_set);
            echo_proof(out, B, hyp_set);

            and_ax(out, *A, *B);
            return;
        }

        if (is_op(ast, op_t::DISJ))
        {
            auto A = subtree(ast, 0);
            auto B = subtree(ast, 1);

            if (is_valid(A, hyp_set))
            {
                echo_proof(out, A, hyp_set);
                out.push_back(to_string(*A) + " -> " + to_string(*ast));
                out.push_back(to_string(*ast));
                return;
            }

            if (is_valid(B, hyp_set))
            {
                echo_proof(out, B, hyp_set);
                out.push_back(to_string(*B) + " -> " + to_string(*ast));
                out.push_back(to_string(*ast));
                return;
            }

            bruteforce(out, ast, hyp_set);
            return;
        }

        if (is_op(ast, op_t::IMPL))
        {
            auto B = subtree(ast, 1);

            if (is_valid(B, hyp_set))
            {
                echo_proof(out, B, hyp_set);
                out.push_back(to_string(*B) + "-> " + to_string(*ast));
                out.push_back(to_string(*ast));
                return;
            }

            ast_expression::chld_v children = std::move(get_op(ast).argv);
            for (size_t i = 0; i < 2; ++i)
                children[i] = negated(std::move(children[i]));

            if (is_valid(children[0].get(), hyp_set)
                && is_valid(children[1].get(), hyp_set))
            {
                for (size_t i = 0; i < 2; ++i)
                    echo_proof(out, children[i].get(), hyp_set);

                for (size_t i = 0; i < 2; ++i   )
                    children[i] = take_off_negated(std::move(children[i]));

                implication_from_negs(out, *children[0], *children[1]);

                get_op(ast).argv = move(children);
                return;
            }

            for (size_t i = 0; i < 2; ++i)
                children[i] = take_off_negated(std::move(children[i]));
            get_op(ast).argv = move(children);

            bruteforce(out, ast, hyp_set);
            return;
        }

        if (is_op(ast, op_t::NEG)
            && is_op(subtree(ast, 0), op_t::CONJ))
        {
            ast_expression::chld_v children = std::move(get_op(subtree(ast, 0)).argv);
            for (size_t i = 0; i < 2; ++i)
                children[i] = negated(std::move(children[i]));

            if (is_valid(children[0].get(), hyp_set))
            {
                echo_proof(out, children[0].get(), hyp_set);

                for (size_t i = 0; i < 2; ++i)
                    children[i] = take_off_negated(std::move(children[i]));

                neg_conj_from_A(out, *children[0], *children[1]);
                get_op(subtree(ast, 0)).argv = std::move(children);
                return;
            }
            else if (is_valid(children[1].get(), hyp_set))
            {
                echo_proof(out, children[1].get(), hyp_set);

                for (size_t i = 0; i < 2; ++i)
                    children[i] = take_off_negated(std::move(children[i]));

                neg_conj_from_B(out, *children[0], *children[1]);
                get_op(subtree(ast, 0)).argv = std::move(children);
                return;
            }
            else
            {
                for (size_t i = 0; i < 2; ++i)
                    children[i] = take_off_negated(std::move(children[i]));
                get_op(subtree(ast, 0)).argv = std::move(children);

                bruteforce(out, ast, hyp_set);
            }
            return;
        }

        if (is_op(ast, op_t::NEG)
            && is_op(subtree(ast, 0), op_t::DISJ))
        {
            ast_expression::chld_v children = std::move(get_op(subtree(ast, 0)).argv);
            for (size_t i = 0; i < 2; ++i)
            {
                children[i] = negated(std::move(children[i]));
                assert(is_valid(children[i].get(), hyp_set));
                echo_proof(out, children[i].get(), hyp_set);
                children[i] = take_off_negated(std::move(children[i]));
            }

            neg_disj(out, *children[0], *children[1]);

            get_op(subtree(ast, 0)).argv = std::move(children);
            return;
        }

        if (is_op(ast, op_t::NEG)
            && is_op(subtree(ast, 0), op_t::IMPL))
        {
            ast_expression::chld_v children = std::move(get_op(subtree(ast, 0)).argv);

            children[1] = negated(std::move(children[1]));

            if (is_valid(children[0].get(), hyp_set)
                && is_valid(children[1].get(), hyp_set))
            {
                for (size_t i = 0; i < 2; ++i)
                {
                    assert(is_valid(children[i].get(), hyp_set));
                    echo_proof(out, children[i].get(), hyp_set);
                }

                children[1] = take_off_negated(std::move(children[1]));

                neg_impl(out, *children[0], *children[1]);

                get_op(subtree(ast, 0)).argv = std::move(children);
                return;
            }

            children[1] = take_off_negated(std::move(children[1]));
            get_op(subtree(ast, 0)).argv = std::move(children);

            bruteforce(out, ast, hyp_set);

            return;
        }

        assert(false && "Ah, shit, here we go again...");
        return;
    }
    case 1:
    {
        auto&& varname = std::get<1>(ast->content);

        auto it = hyp_set.mp.find(varname);
        assert(it != hyp_set.mp.end() && "Improvable variable found!");
        assert(!it->second && "Not negated variable found, but negated in hyp set");

        out.push_back(varname);
        return;
    }
    default:
        assert(false && "Unexpected type");
        exit(-1);
    }
}
