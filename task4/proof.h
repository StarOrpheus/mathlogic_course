#pragma once

#include "ast_record.h"

void echo_proof(std::vector<std::string>& out,
                ast_expression* ast_rec,
                hypotesis_set const& hyp_set);
