#include "parsex.h"
#include "templates.h"

#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <cassert>
#include <unistd.h>
#include <sstream>

using namespace std;

struct ast_record
{
    using mp_dependencies_t = pair<ast_record*, ast_record*>;

    variant<ast_expr_ptr, string>   ast;        // main memory optimization
    optional<string>                annotation;
    optional<mp_dependencies_t>     modus_ponens_deps;
    bool                            was_used_to_prove = 0;
    size_t                          id = 0;
    size_t                          mp_subtree_size = 1;
    hash_t                          hashcode;
    optional<hash_t>                l_hash;

    ast_record(ast_expr_ptr ptr)
        : ast(move(ptr)),
          hashcode(get<ast_expr_ptr>(ast)->hashcode)
    {}
};

static inline bool is_op(ast_expression* ptr)
{
    return ptr->content.index() == 0;
}

static inline auto& get_op(ast_expression* ptr)
{
    return get<ast_expression::operation>(ptr->content);
}

static inline bool is_op(ast_expression* ptr,
                         operation_type type)
{
    return is_op(ptr)
        && get_op(ptr).op_type == type;
}

static inline ast_expression* subtree(ast_expression* ptr,
                                      size_t ind)
{
    assert(ind >= 0 && ind <= 1);
    return get_op(ptr).argv[ind].get();
}

bool check_if_axiom(ast_record* ast_rec,
                    size_t& line)
{
    ast_expression* const ast = get<ast_expr_ptr>(ast_rec->ast).get();
    static stringstream ss;
    ss.str("");
    ss << *ast;

    // 1. A->(B->A)
    if (is_op(subtree(ast, 1))
        && get_op(subtree(ast, 1)).op_type == operation_type::IMPL
        && subtree(subtree(ast, 1), 1)->hashcode == subtree(ast, 0)->hashcode)
    {
        ast_rec->annotation = "Ax. sch. 1";
        neg_hypotesis(cout, line, ss.str());
        return true;
    }
    // 2. (A->B) -> (A->B->C) -> (A->C)
    if (is_op(subtree(ast, 0), operation_type::IMPL)
        && is_op(subtree(ast, 1), operation_type::IMPL)
        && is_op(subtree(subtree(ast, 1), 0), operation_type::IMPL)
        && is_op(subtree(subtree(ast, 1), 1), operation_type::IMPL)
        && is_op(subtree(subtree(subtree(ast, 1), 0), 1), operation_type::IMPL)
        && subtree(subtree(ast, 0), 0)->hashcode == subtree(subtree(subtree(ast, 1), 0), 0)->hashcode // | matching A
        && subtree(subtree(ast, 0), 0)->hashcode == subtree(subtree(subtree(ast, 1), 1), 0)->hashcode // |
        && subtree(subtree(ast, 0), 1)->hashcode == subtree(subtree(subtree(subtree(ast, 1), 0), 1), 0)->hashcode
        && subtree(subtree(subtree(subtree(ast, 1), 0), 1), 1)->hashcode == subtree(subtree(subtree(ast, 1), 1), 1)->hashcode)
    {
        ast_rec->annotation = "Ax. sch. 2";
        neg_hypotesis(cout, line, ss.str());
        return true;
    }
    // 3. A->B->A&B
    if (is_op(subtree(ast, 1), operation_type::IMPL)
        && is_op(subtree(subtree(ast, 1), 1), operation_type::CONJ)
        && subtree(ast, 0)->hashcode == subtree(subtree(subtree(ast, 1), 1), 0)->hashcode
        && subtree(subtree(ast, 1), 0)->hashcode == subtree(subtree(subtree(ast, 1), 1), 1)->hashcode)
    {
        ast_rec->annotation = "Ax. sch. 3";
        neg_hypotesis(cout, line, ss.str());
        return true;
    }

    // 4. A&B->A
    if (is_op(subtree(ast, 0), operation_type::CONJ)
        && subtree(subtree(ast, 0), 0)->hashcode == subtree(ast, 1)->hashcode)
    {
        ast_rec->annotation = "Ax. sch. 4";
        neg_hypotesis(cout, line, ss.str());
        return true;
    }

    // 5. A&B->B
    if (is_op(subtree(ast, 0), operation_type::CONJ)
        && subtree(subtree(ast, 0), 1)->hashcode == subtree(ast, 1)->hashcode)
    {
        ast_rec->annotation = "Ax. sch. 5";
        neg_hypotesis(cout, line, ss.str());
        return true;
    }

    // 6. A->A|B
    if (is_op(subtree(ast, 1), operation_type::DISJ)
        && subtree(ast, 0)->hashcode == subtree(subtree(ast, 1), 0)->hashcode)
    {
        ast_rec->annotation = "Ax. sch. 6";
        neg_hypotesis(cout, line, ss.str());
        return true;
    }

    // 7. B->A|B
    if (is_op(subtree(ast, 1), operation_type::DISJ)
        && subtree(ast, 0)->hashcode == subtree(subtree(ast, 1), 1)->hashcode)
    {
        ast_rec->annotation = "Ax. sch. 7";
        neg_hypotesis(cout, line, ss.str());
        return true;
    }

    // 8. (A->C) -> (B->C) -> (A|B->C)
    if (is_op(subtree(ast, 0), operation_type::IMPL)
        && is_op(subtree(ast, 1), operation_type::IMPL)
        && is_op(subtree(subtree(ast, 1), 0), operation_type::IMPL)
        && is_op(subtree(subtree(ast, 1), 1), operation_type::IMPL)
        && is_op(subtree(subtree(subtree(ast, 1), 1), 0), operation_type::DISJ)
        && subtree(subtree(ast, 0), 0)->hashcode == subtree(subtree(subtree(subtree(ast, 1), 1), 0), 0)->hashcode // match A
        && subtree(subtree(subtree(ast, 1), 0), 0)->hashcode == subtree(subtree(subtree(subtree(ast, 1), 1), 0), 1)->hashcode //match B
        && subtree(subtree(ast, 0), 1)->hashcode == subtree(subtree(subtree(ast, 1), 0), 1)->hashcode   // C
        && subtree(subtree(ast, 0), 1)->hashcode == subtree(subtree(subtree(ast, 1), 1), 1)->hashcode)  // C
    {
        ast_rec->annotation = "Ax. sch. 8";
        neg_hypotesis(cout, line, ss.str());
        return true;
    }

    // 9. (A->B) -> (A->!B) -> !B
    if (is_op(subtree(ast, 0), operation_type::IMPL)
        && is_op(subtree(ast, 1), operation_type::IMPL)
        && is_op(subtree(subtree(ast, 1), 0), operation_type::IMPL)
        && is_op(subtree(subtree(ast, 1), 1), operation_type::NEG)
        && is_op(subtree(subtree(subtree(ast, 1), 0), 1), operation_type::NEG)
        && subtree(subtree(ast, 0), 0)->hashcode == subtree(subtree(subtree(ast, 1), 0), 0)->hashcode
        && subtree(subtree(ast, 0), 0)->hashcode == subtree(subtree(subtree(ast, 1), 1), 0)->hashcode
        && subtree(subtree(ast, 0), 1)->hashcode == subtree(subtree(subtree(subtree(ast, 1), 0), 1), 0)->hashcode)
    {
        ast_rec->annotation = "Ax. sch. 9";
        neg_hypotesis(cout, line, ss.str());
        return true;
    }

    return false;
}

bool check_if_classc_axiom(ast_record* ast_rec,
                           size_t& line)
{
    ast_expression* const ast = get<ast_expr_ptr>(ast_rec->ast).get();

    if (!is_op(ast)) return false;

    auto& op = get_op(ast);
    if (op.op_type != operation_type::IMPL) return false;

    if (check_if_axiom(ast_rec, line)) return true;

    // 10. !!A->A
    if (is_op(subtree(ast, 0), operation_type::NEG)
        && is_op(subtree(subtree(ast, 0), 0), operation_type::NEG)
        && subtree(subtree(subtree(ast, 0), 0), 0)->hashcode == subtree(ast, 1)->hashcode)
    {
        ast_rec->annotation = "Ax. sch. 10";
        tenth_axiom(cout, line, to_string(*subtree(ast, 1)));
        return true;
    }

    return false;
}

template<typename Container>
bool check_if_hypotesis(ast_record* ast_rec,
                        Container const& mp,
                        size_t& line)
{
    auto f = mp.find(get<ast_expr_ptr>(ast_rec->ast)->hashcode);
    if (f != mp.end())
    {
        ast_rec->annotation = "Hypothesis " + to_string(f->second);

        neg_hypotesis(std::cout, line, to_string(*get<ast_expr_ptr>(ast_rec->ast)));
    }
    return f != mp.end();
}

void mark_dependencies(ast_record* ast_rec)
{
    ast_rec->was_used_to_prove = true;
    if (ast_rec->modus_ponens_deps)
    {
        mark_dependencies(ast_rec->modus_ponens_deps->first);
        mark_dependencies(ast_rec->modus_ponens_deps->second);
    }
}

using all_ast_trees_t = vector<unique_ptr<ast_record>>;
using hash_to_record_t = unordered_map<hash_t, ast_record*, hash_t_hash>;
using ast_set_by_hash_t = unordered_map<hash_t, unordered_map<hash_t, ast_record*, hash_t_hash>, hash_t_hash>;
using id_by_hash_t = unordered_map<hash_t, size_t, hash_t_hash>;

int main()
{
//    sleep(8);
    reader_impl             rdr;
    all_ast_trees_t         all_asts;
    vector<ast_record*>     expressions_order;
    id_by_hash_t            hypotheses;
    vector<ast_expression*> hypotheses_order; // TODO: ast_expr* -> ast_record*
    hash_to_record_t        proven_by_hash;
    ast_set_by_hash_t       proven_impl_by_right_subtree_hash;
    ast_expr_ptr            result;

    assert(!eof(rdr));
    size_t id = 1;
    if (!goes_next(rdr, "|-"))
    {
        while (true)
        {
            auto expr = parse_expr(rdr);
            auto* ptr = expr.get();
            hypotheses_order.push_back(ptr);
            auto ins = hypotheses.insert({ptr->hashcode, id++});
            assert(ins.second);
            all_asts.push_back(make_unique<ast_record>(move(expr)));

            if (*rdr.read_left != ',')
                break;
            else
                ++rdr.read_left;
        }
    }

    assert(goes_next(rdr, "|-"));
    rdr.read_left += 2;
    result = parse_expr(rdr);

    for (size_t i = 0; i < hypotheses_order.size(); ++i)
    {
        if (i != 0)
            cout << ", ";
        cout << *hypotheses_order[i];
    }
    cout << "|-!!";
    cout << *result << endl;

    id = 0;
    size_t line = 0;
    while (!eof(rdr))
    {
        line++;
        auto expr = parse_expr(rdr);
        auto* ptr = expr.get();
        auto ast_record_ptr = make_unique<ast_record>(move(expr));

        ast_record* ast_rec = ast_record_ptr.get();
        all_asts.push_back(move(ast_record_ptr));

        expressions_order.push_back(ast_rec);

        if (!check_if_hypotesis(ast_rec, hypotheses, line)
         && !check_if_classc_axiom(ast_rec, line))
        {
            bool modus_ponens_found = false;
            auto it = proven_impl_by_right_subtree_hash.find(ast_rec->hashcode);
            if (it != proven_impl_by_right_subtree_hash.end())
            {
                for (auto hash_and_parent : it->second)
                {
                    auto it2 = proven_by_hash.find(*hash_and_parent.second->l_hash);
                    if (it2 == proven_by_hash.end()) continue;

                    if (!modus_ponens_found)
                    {
                        ast_rec->modus_ponens_deps = make_pair(hash_and_parent.second, it2->second);
                        ast_rec->mp_subtree_size = hash_and_parent.second->mp_subtree_size
                                                 + it2->second->mp_subtree_size
                                                 + 1;
                        modus_ponens_found = true;
                    } else
                    {
                        if (ast_rec->modus_ponens_deps->first->mp_subtree_size + ast_rec->modus_ponens_deps->second->mp_subtree_size
                            > hash_and_parent.second->mp_subtree_size + it2->second->mp_subtree_size)
                        {
                            ast_rec->modus_ponens_deps = make_pair(hash_and_parent.second, it2->second);
                            ast_rec->mp_subtree_size = hash_and_parent.second->mp_subtree_size
                                                     + it2->second->mp_subtree_size
                                                     + 1;
                        }
                    }
                }
            }

            if (!modus_ponens_found)
            {
                cout << "Proof is incorrect" << endl;
                cerr << *ptr << endl;
                return -1;
            }

            auto pr = *ast_rec->modus_ponens_deps;
            auto A = to_string(*get<ast_expr_ptr>(pr.second->ast));
            auto B = to_string(*get<ast_expression::operation>(get<ast_expr_ptr>(pr.first->ast)->content).argv[1]);

//            std::cerr << "#MP" << A << " and " << B << endl;
            neg_modus_ponens(cout, line, A, B);
        }

        bool inserted = false;
        auto it = proven_by_hash.find(ptr->hashcode);
        if (it == proven_by_hash.end()
         || it->second->mp_subtree_size > ast_rec->mp_subtree_size)
        {
            inserted = true;
            proven_by_hash.insert(it, {ptr->hashcode, ast_rec});
        }

        if (is_op(ptr, operation_type::IMPL))
        {
            inserted = true;
            proven_impl_by_right_subtree_hash[subtree(ptr, 1)->hashcode].insert({ptr->hashcode, ast_rec});
            ast_rec->l_hash = subtree(ptr, 0)->hashcode;
        }

        if (!inserted)
        {
            expressions_order.pop_back();
        }
    }

    return 0;
}
