#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include "construct_types.h"
#include "deconstruct.h"
#include "reconstruct.h"
#include "construct_flags.h"

int main(int argc, char** argv) {
  std::string path;
  std::string outpath;
  if (handle_flags(argc, argv, &path, &outpath) != 0) {
    std::cout << "Some flag(s) not set" << std::endl;
    return 0;
  }
  if (path.empty()) {
    std::cout << "No input file specified" << std::endl;
    return 0;
  }

  std::ifstream inpfile(path);
  std::stringstream buffer;
  buffer << inpfile.rdbuf();
  std::vector<con_token*> tokens = parse_construct(buffer.str());

  // Make _start global
  con_token* glob_tok = new con_token(CMD);
  glob_tok->tok_cmd->command = "global _start";
  glob_tok->indentation = 0;
  tokens.insert(tokens.begin(), glob_tok);
  glob_tok = nullptr;

  tokens = delinearize_tokens(tokens);

  // Order dependant: some tokens are replaced with macros, so apply_macro() must be at the end.
  apply_functions(tokens);
  apply_ifs(tokens);
  apply_whiles(tokens);
  apply_funcalls(tokens);
  apply_syscalls(tokens);
  std::vector<con_macro*> empty_macros; // pointer to con_macros in tokens, not a copy
  apply_macros(tokens, empty_macros);
  empty_macros.clear(); // remove the pointers to con_macro, not the con_macro objects themselves

  set_indentation(tokens);
  linearize_tokens(tokens);

  std::ofstream outfile;
  outfile.open(outpath);
  outfile << tokens_to_nasm(tokens);
  outfile.close();

  for (std::vector<con_token*>::reverse_iterator r_it = tokens.rbegin(); r_it != tokens.rend(); ++r_it) {
    delete *r_it;
    *r_it = nullptr;
  }
  tokens.clear();
  return 0;
}
