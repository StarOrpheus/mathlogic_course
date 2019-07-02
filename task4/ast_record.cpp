#include "ast_record.h"

#include <unordered_set>
#include <algorithm>

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

static inline void
get_varnames_impl(ast_expression* ptr,
                  std::unordered_set<std::string>& hs)
{
    std::visit(overloaded
    {
       [&hs] (ast_expression::operation const& op)
          {
               for (auto&& c : op.argv)
                   get_varnames_impl(c.get(), hs);
          },
       [&hs] (std::string const& varname)
          {
               hs.insert(varname);
          },

    }, ptr->content);
}

static inline std::vector<std::string>
get_varnames(ast_expression* ptr)
{
    std::unordered_set<std::string> hs;
    get_varnames_impl(ptr, hs);
    return {hs.begin(), hs.end()};
}

ast_record::ast_record(ast_expr_ptr ptr)
    : ast(move(ptr)),
      hashcode(std::get<0>(ast)->hashcode),
      varnames(get_varnames(std::get<0>(ast).get()))
{
    if (is_op(std::get<0>(ast).get(), operation_type::IMPL))
        l_hash = subtree(std::get<0>(ast).get(), 0)->hashcode;
}

static inline bool apply(ast_expression const* ast,
                         std::vector<std::string> const& varnames,
                         uint16_t mask)
{
    
    return std::visit(overloaded
    {
       [&varnames, mask] (ast_expression::operation const& op)
          {
                switch (op.op_type)
                {
                case operation_type::NEG:
                   return !apply(op.argv[0].get(), varnames, mask);
                case operation_type::CONJ:
                   return apply(op.argv[0].get(), varnames, mask)
                           && apply(op.argv[1].get(), varnames, mask);
                case operation_type::DISJ:
                   return apply(op.argv[0].get(), varnames, mask)
                           || apply(op.argv[1].get(), varnames, mask);
                case operation_type::IMPL:
                   return !apply(op.argv[0].get(), varnames, mask)
                           || apply(op.argv[1].get(), varnames, mask);
                default:
                   assert(false && "Unknown op type");
                   exit(-1);
                }
          },
       [&varnames, mask] (std::string const& varname)
          {
                auto const p = std::find(varnames.begin(),
                                         varnames.end(),
                                         varname)
                       - varnames.begin();
                return static_cast<bool>(mask & (1 << p));
          },
    }, ast->content);
}

static inline std::optional<hypotesis_set>
try_find_hypset_impl(ast_expression const* ast_rec,
                     std::vector<std::string> const& varnames,
                     bool const negated)
{
    hypotesis_set result_st(varnames);
    int bitcount = 228; // properly chosen constant

    auto const n = varnames.size();

    for(uint16_t mask = 0; mask < (1 << n); ++mask)
    {
        int cur_bitcount = __builtin_popcount(mask);

        if (cur_bitcount > bitcount)
            continue;

        hypotesis_set tmp(varnames);

        for (uint16_t i = 0; i < n; ++i)
        {
            if (mask & (1 << i))
                tmp.mp[varnames[i]] = negated;
        }

        if (is_valid(ast_rec, tmp))
        {
            bitcount = cur_bitcount;
            result_st = std::move(tmp);
        }
    }

    if (bitcount == 228)
        return {};
    return {std::move(result_st)};
}

std::optional<hypotesis_set> try_find_hypset(ast_expr_ptr& ast_rec,
                                             std::vector<std::string> const& varnames)
{
    auto res = try_find_hypset_impl(ast_rec.get(), varnames, false);
    if (res)
        return res;

    ast_rec = negated(move(ast_rec));
    res = try_find_hypset_impl(ast_rec.get(), varnames, true);
    return res;
}

bool is_valid(ast_expression const* ast,
              hypotesis_set const& hyp_set) noexcept
{

    size_t n = hyp_set.varnames.size();
    uint16_t mask = 0;
    uint16_t mask2 = 0;

    {
        size_t i = 0;
        for (auto&& hyp : hyp_set.varnames)
        {
            auto it = hyp_set.mp.find(hyp);
            if (it != hyp_set.mp.end())
            {
                mask |= 1 << i;
                mask2 |= (static_cast<uint16_t>(it->second ? 0 : 1) << i);
            }
            ++i;
        }
    }

    uint16_t m = ~mask;
    m <<= 16 - n;
    m >>= 16 - n;

    for (uint16_t s = m; ; s = (s - 1) & m)
    {
        if (!apply(ast, hyp_set.varnames, s | mask2))
            return false;

        if (s == 0) break;
    }

    return true;
}

hypotesis_set::hypotesis_set(varnames_t const& varnames)
    : varnames(varnames)
{}

hypotesis_set::hypotesis_set(hypotesis_set&& other) noexcept
    : mp(move(other.mp)),
      varnames(other.varnames)
{}

hypotesis_set& hypotesis_set::operator=(hypotesis_set&& rhs) noexcept
{
    assert(&rhs.varnames == &varnames);
    mp = std::move(rhs.mp);
    return *this;
}

bool check_if_axiom(ast_expression const* ast)
{
    if (ast == nullptr) return false;
    if (!is_op(ast)) return false;

    auto& op = get_op(ast);
    if (op.op_type != operation_type::IMPL) return false;

    // 1. A->(B->A)
    if (is_op(subtree(ast, 1))
            && get_op(subtree(ast, 1)).op_type == operation_type::IMPL
            && subtree(subtree(ast, 1), 1)->hashcode == subtree(ast, 0)->hashcode)
    {
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
        return true;
    }
    // 3. A->B->A&B
    if (is_op(subtree(ast, 1), operation_type::IMPL)
            && is_op(subtree(subtree(ast, 1), 1), operation_type::CONJ)
            && subtree(ast, 0)->hashcode == subtree(subtree(subtree(ast, 1), 1), 0)->hashcode
            && subtree(subtree(ast, 1), 0)->hashcode == subtree(subtree(subtree(ast, 1), 1), 1)->hashcode)
    {
        return true;
    }

    // 4. A&B->A
    if (is_op(subtree(ast, 0), operation_type::CONJ)
            && subtree(subtree(ast, 0), 0)->hashcode == subtree(ast, 1)->hashcode)
    {
        return true;
    }

    // 5. A&B->B
    if (is_op(subtree(ast, 0), operation_type::CONJ)
            && subtree(subtree(ast, 0), 1)->hashcode == subtree(ast, 1)->hashcode)
    {
        return true;
    }

    // 6. A->A|B
    if (is_op(subtree(ast, 1), operation_type::DISJ)
            && subtree(ast, 0)->hashcode == subtree(subtree(ast, 1), 0)->hashcode)
    {
        return true;
    }

    // 7. B->A|B
    if (is_op(subtree(ast, 1), operation_type::DISJ)
            && subtree(ast, 0)->hashcode == subtree(subtree(ast, 1), 1)->hashcode)
    {
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
        return true;
    }

    // 10. !!A->A
    if (is_op(subtree(ast, 0), operation_type::NEG)
            && is_op(subtree(subtree(ast, 0), 0), operation_type::NEG)
            && subtree(subtree(subtree(ast, 0), 0), 0)->hashcode == subtree(ast, 1)->hashcode)
    {
        return true;
    }

    return false;
}
