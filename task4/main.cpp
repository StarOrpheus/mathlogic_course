#include "parsex.h"
#include "templates.h"
#include "ast_record.h"
#include "proof.h"

#include <iostream>
#include <unordered_map>
#include <algorithm>
#include <unordered_set>
#include <cassert>
#include <unistd.h>
#include <sstream>

using namespace std;

using all_ast_trees_t = vector<unique_ptr<ast_record>>;

int main()
{
    ios_base::sync_with_stdio(false);

//    sleep(8);
    ast_expr_ptr            result;

    std::string line;
    std::getline(std::cin, line);

    std::string_view view = line;
    result = parse_expr(view);

    auto ast_rec = ast_record{move(result)};

    auto hyp_set_opt = try_find_hypset(std::get<0>(ast_rec.ast), ast_rec.varnames);

    if (hyp_set_opt)
    {
        auto& hyp_set = *hyp_set_opt;

        {
            std::vector<std::string> vec;
            for (auto&& varname : hyp_set.mp)
                vec.emplace_back((varname.second ? "!" : "") + varname.first);
           if (vec.size())
                std::cout << vec[0];
            for (size_t i = 1; i < vec.size(); ++i)
                std::cout << ", " << vec[i];
            std::cout << " |- ";
            std::cout << *std::get<0>(ast_rec.ast) << std::endl;
            vec.clear();
            auto kek = std::get<0>(ast_rec.ast).get();
            echo_proof(vec, std::get<0>(ast_rec.ast).get(), hyp_set);
            for (auto&& pr : vec)
                std::cout << pr << "\n";
            std::cout.flush();
        }
    }
    else
    {
        std::cout << ":(" << std::endl;
    }

    return 0;
}
