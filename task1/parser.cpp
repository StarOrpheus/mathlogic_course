#include <cassert>
#include <functional>
#include <cstring>
#include <unistd.h>
#include <cctype>
#include <variant>
#include <memory>
#include <fstream>
#include <iostream>
#include <sstream>
#include <numeric>

using namespace std;

enum class token_type
{
    NEG,
    CONJ,
    DISJ,
    IMPL,
    OP_BRACKET,
    CL_BRACKET,
    NO_TOKEN
};

enum class operation_type
{
    NEG,
    CONJ,
    DISJ,
    IMPL
};

using varname = string;
using lex_token = std::variant<token_type, varname, nullptr_t>;

struct reader_impl
{
    enum
    {
        buffer_size = 4096u
    };

    char        read_buffer[buffer_size];
    char const* read_left = read_buffer;
    char const* read_right = read_buffer;
    lex_token   cur_token = nullptr;
};

struct ast_expression
{
    struct operation
    {
        vector<unique_ptr<ast_expression>> argv;
        operation_type op_type;
    };

    ast_expression(operation&& op)
        : content(std::move(op))
    {}

    ast_expression(string&& varname)
        : content(std::move(varname))
    {}

    variant<operation, varname> content;
};

using ast_expr_ptr = unique_ptr<ast_expression>;

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

std::ostream& operator<<(std::ostream& o, ast_expression const& expr)
{
    if (expr.content.index() == 1) // var
        return o << get<varname>(expr.content);

    auto& op = get<ast_expression::operation>(expr.content);
    o << '(';
    if (op.op_type == operation_type::NEG)
    {
        assert(op.argv.size() == 1);
        o << "!" << *op.argv[0].get();
    } else
    {
        o << to_string(op.op_type);
        assert(op.argv.size() > 1);
        for (size_t i = 0; i < op.argv.size(); ++i)
        {
            assert(op.argv[i] != nullptr);
            o << "," << *op.argv[i].get();
        }
    }
    o << ')';
    return o;
}

size_t read_size(reader_impl const& rdr)
{
    assert(rdr.read_left <= rdr.read_right);
    return static_cast<size_t>(rdr.read_right - rdr.read_left);
}

void move_data_to_front(reader_impl& rdr)
{
    assert(rdr.read_left <= rdr.read_right);
    if (rdr.read_left == rdr.read_right)
    {
        rdr.read_left = rdr.read_right = rdr.read_buffer;
        return;
    }

    size_t saved_sz = read_size(rdr);
    memmove(rdr.read_buffer, rdr.read_left, read_size(rdr));
    rdr.read_right -= (rdr.read_left - rdr.read_buffer);
    rdr.read_left = rdr.read_buffer;
    assert(rdr.read_left <= rdr.read_right);
    assert(read_size(rdr) == saved_sz);
}

size_t read_more(reader_impl& rdr)
{
    assert(rdr.read_left <= rdr.read_right);
    move_data_to_front(rdr);
    ssize_t cnt = read(fileno(stdin),
                       const_cast<char*>(rdr.read_right),
                       reader_impl::buffer_size - read_size(rdr));

    if (cnt == -1)
    {
        printf("Read failed! Error: %s\n", strerror(errno));
        exit(0);
    }

    rdr.read_right += cnt;
    assert(rdr.read_left <= rdr.read_right);
    return cnt;
}

void skip_ws(reader_impl& rdr)
{
    for (; rdr.read_left < rdr.read_right
         && isspace(*rdr.read_left); ++rdr.read_left)
    {}
}

bool is_varname_char(char c)
{
    return isupper(c)
        || isdigit(c)
        || c == '\'';
}

varname read_var(reader_impl& rdr)
{
    assert(isupper(*rdr.read_left));
    string buf; // Praise for small obj optimization..
    do
    {
        char const* start = rdr.read_left;
        for (; rdr.read_left < rdr.read_right
               && is_varname_char(*rdr.read_left);
             ++rdr.read_left)
        {}

        if (is_varname_char(*start))
            buf.append(start, rdr.read_left);

        if (read_size(rdr) == 0)
        {
            if (!read_more(rdr))
                return buf;
        } else
        {
            return buf;
        }
    } while (true);
}

void expect_next(reader_impl& rdr,
                 string const& str)
{
    assert(read_size(rdr) >= str.length());

    if (!equal(str.begin(), str.end(), rdr.read_left))
    {
        cerr << "Unexpected token!\n";
        cerr << "Expected: " << str << "\n";
        cerr << "Found: " << string_view(rdr.read_left, str.size()) << endl;
        assert(false);
    }
}

lex_token& read_token(reader_impl& rdr)
{
    skip_ws(rdr);
    while (read_size(rdr) == 0)
    {
        if (!read_more(rdr))
            return rdr.cur_token = token_type::NO_TOKEN;
        skip_ws(rdr);
    }

    if (isupper(*rdr.read_left))
    {
        assert(rdr.cur_token.index() == 2);
        return rdr.cur_token = read_var(rdr);
    }

    switch (*rdr.read_left)
    {
    case '(':
        return rdr.cur_token = token_type::OP_BRACKET;
    case ')':
        return rdr.cur_token = token_type::CL_BRACKET;
    case '&':
        return rdr.cur_token = token_type::CONJ;
    case '|':
        return rdr.cur_token = token_type::DISJ;
    case '-':
        if (read_size(rdr) < 2)
        {
            while (read_more(rdr)
                && read_size(rdr) < 2) {}
            if (read_size(rdr) < 2)
            {
                assert(false && "Unexpected end of file!");
                return rdr.cur_token = token_type::NO_TOKEN;
            }
        }

        expect_next(rdr, "->");
        return rdr.cur_token = token_type::IMPL;
    case '!':
        return rdr.cur_token = token_type::NEG;
    case '\n':
        ++rdr.read_left;
        return rdr.cur_token = token_type::NO_TOKEN;
    default:
        assert(false && "Unexpected character");
        return rdr.cur_token = token_type::NO_TOKEN;
    }
}

void skip_token(reader_impl& rdr)
{
    switch (rdr.cur_token.index())
    {
    case 0:
    {
        switch (get<token_type>(rdr.cur_token))
        {
        case token_type::NO_TOKEN:
            assert(rdr.read_left == rdr.read_right);
            return;
        case token_type::OP_BRACKET:
        case token_type::CL_BRACKET:
        case token_type::NEG:
        case token_type::CONJ:
        case token_type::DISJ:
            rdr.read_left += 1; // token length
            break;
        case token_type::IMPL:
            rdr.read_left += 2;
            break;
        default:
            assert(false && "Unexpected token");
            return;
        }
    }
        break;
    case 1:
        assert(get<varname>(rdr.cur_token).size() == 0);
        rdr.read_left += get<varname>(rdr.cur_token).size();
        break;
    case 2:
        assert(false && "Skipping not processed token");
        return;
    default:
        assert(false && "Unexpected type");
        return;
    }

    assert(rdr.read_left <= rdr.read_right);
    rdr.cur_token = nullptr;
}

lex_token& current_token(reader_impl& rdr)
{
    if (rdr.cur_token.index() != 2)
        return rdr.cur_token;
    read_token(rdr);
    return rdr.cur_token;
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
        tmp_operands.emplace_back(move(rhs));
        tmp_operands.emplace_back(move(lhs));
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
        tmp_operands.emplace_back(move(lhs));
        tmp_operands.emplace_back(move(rhs));
        return make_unique<ast_expression>(ast_expression::operation{std::move(tmp_operands), typ});
    });
}

ast_expr_ptr parse_expr(reader_impl& rdr);

ast_expr_ptr parse_var(reader_impl& rdr)
{
    string varname = read_var(rdr);
    assert(!varname.empty() && "Expected varname");

    return make_unique<ast_expression>(std::move(varname));
}

ast_expr_ptr parse_neg(reader_impl& rdr)
{
    lex_token& token = current_token(rdr);
    switch (token.index())
    {
    case 0:
    {
        switch (get<token_type>(token))
        {
        case token_type::NEG:
        {
            vector<ast_expr_ptr> operands;
            skip_token(rdr);
            operands.emplace_back(parse_neg(rdr));
            return make_unique<ast_expression>(ast_expression::operation{std::move(operands), operation_type::NEG});
        }
        case token_type::OP_BRACKET:
        {
            skip_token(rdr);
            auto ret = parse_expr(rdr);
            assert(current_token(rdr).index() == 0
                   && get<token_type>(current_token(rdr)) == token_type::CL_BRACKET);
            skip_token(rdr);
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
        skip_token(rdr);
        return ret;
    }
    case 2:
    default:
        assert(false && "Unexpected index");
        return nullptr;
    }
}

ast_expr_ptr parse_conj(reader_impl& rdr)
{
    vector<ast_expr_ptr> operands;
    operands.emplace_back(parse_neg(rdr));

    while (true)
    {
        lex_token& token = current_token(rdr);
        switch (token.index())
        {
        case 0:
        {
            switch(get<token_type>(token))
            {
            case token_type::CONJ:
            {
                skip_token(rdr);
                operands.emplace_back(parse_neg(rdr));
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

ast_expr_ptr parse_disj(reader_impl& rdr)
{
    vector<ast_expr_ptr> operands;
    operands.emplace_back(parse_conj(rdr));

    while (true)
    {
        lex_token& token = current_token(rdr);
        switch (token.index())
        {
        case 0:
        {
            switch(get<token_type>(token))
            {
            case token_type::DISJ:
            {
                skip_token(rdr);
                operands.emplace_back(parse_conj(rdr));
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

ast_expr_ptr parse_implication(reader_impl& rdr)
{
    vector<ast_expr_ptr> operands;
    operands.emplace_back(parse_disj(rdr));

    while (true)
    {
        lex_token& token = current_token(rdr);
        switch (token.index())
        {
        case 0:
        {
            switch(get<token_type>(token))
            {
            case token_type::IMPL:
            {
                skip_token(rdr);
                operands.emplace_back(parse_disj(rdr));
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

ast_expr_ptr parse_expr(reader_impl& rdr)
{
    auto result = parse_implication(rdr);

    return result;
}

void reset(reader_impl& rdr)
{
    if (*rdr.read_left == '\n')
        ++rdr.read_left;
    rdr.cur_token = nullptr;
}

int main(int argc, char const** argv)
{
//    sleep(10);
    reader_impl rdr;
    auto ast = parse_expr(rdr);
    cout << *ast << endl;
//    cout.flush();
    return 0;
}
