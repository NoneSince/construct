#ifndef RECONSTRUCT_H_
#define RECONSTRUCT_H_

#include <string>
#include <vector>
#include "construct_types.h"

extern CON_BITWIDTH bitwidth;

std::string comparison_to_string(const CON_COMPARISON& condition);

// The following functions transform the construct specific tokens to nasm ones,
// the parent construct tokens remain, but are removed during linearization

// Converts args to macros and adds tag with same name to child tokens
void apply_whiles(std::vector<con_token*>& tokens);
void apply_ifs(std::vector<con_token*>& tokens);
void apply_functions(std::vector<con_token*>& tokens);
void apply_macros(std::vector<con_token*>& tokens, std::vector<con_macro*>& macros);
void apply_funcalls(std::vector<con_token*>& tokens);
void apply_syscalls(std::vector<con_token*>& tokens);

// During linearization, the construct parent tokens are removed
void linearize_tokens(std::vector<con_token*>& tokens);

std::string tokens_to_nasm(const std::vector<con_token*>& tokens);

#endif // RECONSTRUCT_H_
