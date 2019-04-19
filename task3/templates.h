#ifndef PROOF_PARSER_H
#define PROOF_PARSER_H

#include <iostream>

/**
 * @brief Let A=!!A, B=!!(A->C), so it writes to ostream os proof of !!A, !!(A->C) |- !!C
 * @param os Output sink
 * @param line_num Start line numbering in proof with given line_num and increment it.
 */
void neg_modus_ponens(std::ostream& os,
                      size_t& line_num,
                      std::string const& A,
                      std::string const& B);

/**
 * @brief Writes to ostream os proof of !!(!!Y->Y).
 * @param os Output sink.
 * @param line_num Start line numbering in proof with given line_num and increment it.
 */
void tenth_axiom(std::ostream& os,
                     size_t& line_num,
                     std::string const& A);

/**
 * @brief Writes to ostream os proof of !!A, assuming A is hypotesis.
 * @param os Output sink.
 * @param line_num Start line numbering in proof with given line_num and increment it.
 */
void neg_hypotesis(std::ostream& os,
                   size_t& line_num,
                   std::string const& A);



#endif
