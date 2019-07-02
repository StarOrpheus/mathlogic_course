#include "parsex.h"

#include "unistd.h"
#include <iostream>
#include <cstring>
#include <cassert>
#include <numeric>
#include <functional>
#include <sstream>

using namespace std;

char const* to_string(token_type token_type)
{
    switch (token_type)
    {
    case token_type::NO_TOKEN:
        return "";
    case token_type::OP_BRACKET:
        return "(";
    case token_type::CL_BRACKET:
        return ")";
    case token_type::NEG:
        return "!";
    case token_type::CONJ:
        return "&";
    case token_type::DISJ:
        return "|";
    case token_type::IMPL:
        return "->";
    default:
        assert(false && "Unexpected token_type");
        return "";
    }
}

string to_string(lex_token const& token)
{
    switch (token.index())
    {
    case 0:
        return to_string(get<token_type>(token));
    case 1:
        return get<varname>(token);
    case 2:
        assert(false && "damn, that's not ok");
        return "null";
    default:
        assert(false && "Unexpected lex_token state!");
        return "";
    }
}

char const* to_string(operation_type const token_type)
{
    switch (token_type)
    {
    case operation_type::NEG:
        return "!";
    case operation_type::CONJ:
        return "&";
    case operation_type::DISJ:
        return "|";
    case operation_type::IMPL:
        return "->";
    default:
        assert(false && "WHAT");
        return "WHAT";
    }
}

thread_local static lex_token current_token = nullptr;

std::ostream& operator<<(std::ostream& o, ast_expression const& expr)
{
    if (expr.content.index() == 1) // var
        return o << get<varname>(expr.content);

    auto& op = get<ast_expression::operation>(expr.content);
    if (op.op_type == operation_type::NEG)
    {
        assert(op.argv.size() == 1);
        o << "!" << *op.argv[0].get();
    } else
    {
        o << '(';
        assert(op.argv.size() > 1);
        o << *op.argv[0];
        for (size_t i = 1; i < op.argv.size(); ++i)
        {
            assert(op.argv[i] != nullptr);
            o << " " << to_string(op.op_type) << " " << *op.argv[i].get();
        }
        o << ')';
    }
    return o;
}

static inline bool is_ws(char c)
{
    return c == ' '
        || c == '\t'
        || c == '\r';
}

bool is_varname_char(char c)
{
    return isupper(c)
        || isdigit(c)
        || c == '\'';
}

varname read_var(std::string_view& view)
{
    assert(isupper(*view.begin()));
    std::string buf;
    auto v = view.substr(0, find_if_not(view.begin(),
                                        view.end(),
                                        is_varname_char)
                            - view.begin());

    buf.reserve(view.size());
    std::copy(v.begin(), v.end(), std::back_inserter(buf));
    view.remove_prefix(buf.size());
    return buf;
}

bool goes_next(string_view& view,
               string const& str)
{
    assert(view.size() >= str.length());
    return equal(str.begin(), str.end(), view.begin());
}

static inline void skip_ws(string_view& view)
{
    view.remove_prefix(find_if_not(view.begin(), view.end(), is_ws) - view.begin());
}

lex_token& read_token(string_view& view)
{
    skip_ws(view);
    if (view.size() == 0)
        return current_token = token_type::NO_TOKEN;

    if (isupper(*view.begin()))
    {
        assert(current_token.index() == 2);
        return current_token = read_var(view);
    }

    switch (*view.begin())
    {
    case '(':
        return current_token = token_type::OP_BRACKET;
    case ')':
        return current_token = token_type::CL_BRACKET;
    case '&':
        return current_token = token_type::CONJ;
    case '|':
        return current_token = token_type::DISJ;
    case '-':
        if (!goes_next(view, "->"))
            return current_token = token_type::NO_TOKEN;
        return current_token = token_type::IMPL;
    case '!':
        return current_token = token_type::NEG;
    case '\n':
        return current_token = token_type::NO_TOKEN;
    default:
        assert(false && "Unexpected character");
        return current_token = token_type::NO_TOKEN;
    }
}

void skip_token(string_view& view)
{
    switch (current_token.index())
    {
    case 0:
    {
        switch (get<token_type>(current_token))
        {
        case token_type::NO_TOKEN:
            return;
        case token_type::OP_BRACKET:
        case token_type::CL_BRACKET:
        case token_type::NEG:
        case token_type::CONJ:
        case token_type::DISJ:
            view.remove_prefix(1);
            break;
        case token_type::IMPL:
            view.remove_prefix(2);
            break;
        default:
            assert(false && "Unexpected token");
            return;
        }
    }
        break;
    case 1:
        view.remove_prefix(get<varname>(current_token).size());
        break;
    case 2:
        assert(false && "Skipping not processed token");
        return;
    default:
        assert(false && "Unexpected type");
        return;
    }

    current_token = nullptr;
}

lex_token& get_current_token(string_view& rdr)
{
    if (current_token.index() != 2)
        return current_token;
    read_token(rdr);
    return current_token;
}

ast_expr_ptr construct_rassoc_ast(operation_type typ, // looks redundant
                                  vector<ast_expr_ptr>& operands)
{
    ast_expr_ptr result;
    assert(operands.size());
    assert(typ != operation_type::NEG);

    if (operands.size() == 1)
    {
        return move(operands[0]);
    }

    result = move(operands.back());
    operands.pop_back();

    return accumulate(operands.rbegin(),
                      operands.rend(),
                      move(result),
                      [typ] (ast_expr_ptr& lhs,
                             ast_expr_ptr& rhs)
    {
        vector<ast_expr_ptr> tmp_operands;
        tmp_operands.reserve(2);
        tmp_operands.push_back(move(rhs));
        tmp_operands.push_back(move(lhs));
        return make_unique<ast_expression>(ast_expression::operation{std::move(tmp_operands), typ});
    });
}

ast_expr_ptr construct_lassoc_ast(operation_type typ, // looks redundant
                                  vector<ast_expr_ptr>& operands)
{
    ast_expr_ptr result;
    assert(operands.size());
    assert(typ != operation_type::NEG);

    if (operands.size() == 1)
    {
        return move(operands[0]);
    }

    return accumulate(operands.begin() + 1,
                      operands.end(),
                      move(operands[0]),
                      [typ] (ast_expr_ptr& lhs,
                             ast_expr_ptr& rhs)
    {
        vector<ast_expr_ptr> tmp_operands;
        tmp_operands.reserve(2);
        tmp_operands.push_back(move(lhs));
        tmp_operands.push_back(move(rhs));
        return make_unique<ast_expression>(ast_expression::operation{std::move(tmp_operands), typ});
    });
}

ast_expr_ptr parse_var(string_view& rdr)
{
    string varname = read_var(rdr);
    assert(!varname.empty() && "Expected varname");

    return make_unique<ast_expression>(std::move(varname));
}

ast_expr_ptr parse_implication(string_view&);

ast_expr_ptr parse_neg(string_view& view)
{
    lex_token& token = get_current_token(view);
    switch (token.index())
    {
    case 0:
    {
        switch (get<token_type>(token))
        {
        case token_type::NEG:
        {
            vector<ast_expr_ptr> operands;
            skip_token(view);
            operands.push_back(parse_neg(view));
            return make_unique<ast_expression>(ast_expression::operation{std::move(operands), operation_type::NEG});
        }
        case token_type::OP_BRACKET:
        {
            skip_token(view);
            auto ret = parse_implication(view);
            assert(get_current_token(view).index() == 0
                   && get<token_type>(get_current_token(view)) == token_type::CL_BRACKET);
            skip_token(view);
            return ret;
        }
        case token_type::CONJ:
        case token_type::DISJ:
        case token_type::IMPL:
        case token_type::CL_BRACKET:
        case token_type::NO_TOKEN:
            assert(false && "Expected expression");
            return nullptr;
        }
    }
    case 1:
    {
        auto ret = make_unique<ast_expression>(move(get<string>(token)));
        skip_token(view);
        return ret;
    }
    case 2:
    default:
        assert(false && "Unexpected index");
        return nullptr;
    }
}

ast_expr_ptr parse_conj(string_view& view)
{
    vector<ast_expr_ptr> operands;
    operands.reserve(2);
    operands.push_back(parse_neg(view));

    while (true)
    {
        lex_token& token = get_current_token(view);
        switch (token.index())
        {
        case 0:
        {
            switch(get<token_type>(token))
            {
            case token_type::CONJ:
            {
                skip_token(view);
                operands.push_back(parse_neg(view));
                continue;
            }
            default:
                goto ret_st;
            }
        }
        case 1:
        case 2:
        default:
            assert(false && "Unexpected token");
            return nullptr;
        }
    }
ret_st:
    return construct_lassoc_ast(operation_type::CONJ, operands);
}

ast_expr_ptr parse_disj(string_view& view)
{
    vector<ast_expr_ptr> operands;
    operands.reserve(2);
    operands.push_back(parse_conj(view));

    while (true)
    {
        lex_token& token = get_current_token(view);
        switch (token.index())
        {
        case 0:
        {
            switch(get<token_type>(token))
            {
            case token_type::DISJ:
            {
                skip_token(view);
                operands.push_back(parse_conj(view));
                continue;
            }
            default:
                goto ret_st;
            }
        }
        case 1:
        case 2:
        default:
            assert(false && "Unexpected token");
            return nullptr;
        }
    }
ret_st:
    return construct_lassoc_ast(operation_type::DISJ, operands);
}

ast_expr_ptr parse_implication(string_view& rdr)
{
    vector<ast_expr_ptr> operands;
    operands.reserve(2);
    operands.push_back(parse_disj(rdr));

    while (true)
    {
        lex_token& token = get_current_token(rdr);
        switch (token.index())
        {
        case 0:
        {
            switch(get<token_type>(token))
            {
            case token_type::IMPL:
            {
                skip_token(rdr);
                operands.push_back(parse_disj(rdr));
                continue;
            }
            default:
                goto ret_st;
            }
        }
        case 1:
        case 2:
        default:
            assert(false && "Unexpected token");
            return nullptr;
        }
    }
ret_st:
    return construct_rassoc_ast(operation_type::IMPL, operands);
}

ast_expr_ptr parse_expr(string_view& rdr)
{
    auto result = parse_implication(rdr);
    current_token = nullptr;
    return result;
}

hash_t operator*(hash_t const& lhs,
                 prime_v const& rhs)
{
    auto result = lhs;
    for (size_t i = 0; i < hash_n; ++i)
        result[i] *= rhs[i];
    return result;
}

hash_t operator+(hash_t const& lhs,
                 hash_t const& rhs)
{
    auto result = lhs;
    for (size_t i = 0; i < hash_n; ++i)
        result[i] += rhs[i];
    return result;
}

static inline hash_t hash_from_size_t(size_t res)
{
    hash_t h;
    for (auto&& hi : h)
        hi = res;
    return h;
}

static inline hash_t hash_op(ast_expression::operation const& op)
{
    static prime_v const P{64603473};
    static thread_local vector<prime_v> p_pows(1, hash_from_size_t(1));

    size_t h0 = hash<char const*>()(to_string(op.op_type)); // too sleepy to write smth effecient..
    hash_t h = hash_from_size_t(h0);
    size_t i = 1;
    for (auto&& child : op.argv)
    {
        while (p_pows.size() <= i)
        {
            p_pows.push_back(p_pows.back());
            for (size_t i = 0; i < hash_n; ++i)
                p_pows.back()[i] *= P[i];
        }

        h = h + child->hashcode * p_pows[i];
        i += child->subtree_sz;
    }

    return h;
}

static inline size_t calc_subtree_sz(ast_expression::operation const& op)
{
    size_t ans = 1;
    for (auto&& child : op.argv)
    {
        assert(child != nullptr);
        ans += child->subtree_sz;
    }
    return ans;
}

ast_expression::ast_expression(ast_expression::operation &&op)
    : content(std::move(op)),
      hashcode(hash_op(get<operation>(content))),
      subtree_sz(calc_subtree_sz(get<operation>(content)))
{}

ast_expression::ast_expression(string varname)
    : content(std::move(varname)),
      hashcode(hash_from_size_t(hash<std::string>()(get<string>(content)))),
      subtree_sz(1u)
{}

std::string to_string(hash_t const& hsh)
{
    stringstream ss;
    ss << hex << hsh[0] << " ";
    for (size_t i = 1; i < hash_n; ++i)
    {
        ss << " " << hex << hsh[i];
    }

    return ss.str();
}

size_t hash_t_hash::operator()(hash_t const& H) const
{
    size_t ans = 0;
    for (auto&& h : H)
    {
        ans *= 31;
        ans += h;
    }

    return ans;
}

string to_string(const ast_expression &expr)
{
    stringstream asts;
    asts << expr;
    return asts.str();
}
