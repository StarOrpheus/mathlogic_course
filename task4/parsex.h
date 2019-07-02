#ifndef PARSEX_H
#define PARSEX_H

#include <vector>
#include <memory>
#include <variant>
#include <array>

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

using varname = std::string;
using lex_token = std::variant<token_type, varname, std::nullptr_t>;

enum
{
    hash_n = 1
};


using prime_v = std::array<size_t, hash_n>;
using hash_t = std::array<size_t, hash_n>;

struct hash_t_hash
{
    size_t operator()(hash_t const& H) const;
};

struct ast_expression
{
    using chld_v = std::vector<std::unique_ptr<ast_expression>>;

    struct operation
    {
        chld_v argv;
        operation_type op_type;
    };

    ast_expression(operation&& op);
    ast_expression(std::string varname);

    std::variant<operation, varname> content;
    hash_t hashcode;
    size_t const subtree_sz;
};

using ast_expr_ptr = std::unique_ptr<ast_expression>;

// helpers
ast_expr_ptr    parse_expr(std::string_view& view);

// output helpers
char const*     to_string(token_type token_type);
char const*     to_string(operation_type const token_type);
std::string     to_string(lex_token const& token_type);
std::string     to_string(hash_t const& hsh);
std::string     to_string(ast_expression const& expr);
std::ostream&   operator<<(std::ostream& o, ast_expression const& expr);

#endif // PARSEX_H
