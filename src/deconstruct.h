#ifndef DECONSTRUCT_H_
#define DECONSTRUCT_H_

#include <string>
#include <vector>
#include "construct_types.h"

std::vector<con_token*> delinearize_tokens(std::vector<con_token*> tokens);

std::vector<con_token*> parse_construct(const std::string& code);

#endif // DECONSTRUCT_H_
