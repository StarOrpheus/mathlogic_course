#ifndef AST_RECORD_H
#define AST_RECORD_H

#include "parsex.h"

#include <cassert>
#include <optional>
#include <unordered_map>

struct ast_record
{
    using mp_dependencies_t = std::pair<ast_record*, ast_record*>;

    std::variant<ast_expr_ptr, std::string>     ast;
    size_t                                      mp_subtree_size = 1;
    hash_t                                      hashcode;
    std::vector<std::string>                    varnames;
    std::optional<mp_dependencies_t>            modus_ponens_deps;
    std::optional<hash_t>                       l_hash;

    ast_record(ast_expr_ptr ptr);
};

using varnames_t = std::vector<std::string>;

struct hypotesis_set
{
    explicit hypotesis_set(varnames_t const&);

    hypotesis_set(hypotesis_set&& other) noexcept;
    hypotesis_set& operator=(hypotesis_set&& rhs) noexcept;

    std::unordered_map<std::string, bool> mp;
    varnames_t const& varnames;
};


static inline bool is_op(ast_expression const* ptr)
{
    return ptr->content.index() == 0;
}


bool check_if_axiom(ast_expression const* ast_rec);

template <
    typename T,
    typename = std::enable_if_t<
        std::is_same_v<std::decay_t<T>, ast_expression>
    >
>
static inline decltype(auto) get_op(T* ptr)
{
    return std::get<0>(ptr->content);
}

template <
    typename T,
    typename = std::enable_if_t<
        std::is_same_v<std::decay_t<T>, ast_expression>
    >
>
static inline bool is_op(T* ptr,
                         operation_type type)
{
    return is_op(ptr)
        && get_op(ptr).op_type == type;
}

template <
    typename T,
    typename = std::enable_if_t<
        std::is_same_v<std::decay_t<T>, ast_expression>
    >
>
static inline decltype(auto) subtree(T* ptr,
                                     size_t ind)
{
    assert(ind >= 0 && ind <= 1);
    return get_op(ptr).argv[ind].get();
}

static inline
ast_expr_ptr negated(ast_expr_ptr ptr)
{
    ast_expression::chld_v tmp_v;
    tmp_v.push_back(std::move(ptr));

    auto op = ast_expression::operation{std::move(tmp_v), operation_type::NEG};
    return std::make_unique<ast_expression>(std::move(op));
}

static inline
ast_expr_ptr take_off_negated(ast_expr_ptr ptr)
{
    return std::move(get_op(ptr.get()).argv[0]);
}

bool is_valid(ast_expression const* ast,
              hypotesis_set const& hyp_set) noexcept;

std::optional<hypotesis_set>
try_find_hypset(ast_expr_ptr& ast_rec,
                std::vector<std::string> const& varnames);

#endif // AST_RECORD_H
