#include <string>
#include <vector>
#include <stdexcept>
#include "reconstruct.h"
#include "construct_types.h"

using namespace std;

#define min(a,b) ((a)<=(b) ? (a) : (b))

int if_amnt = 0;
int while_amnt = 0;
CON_BITWIDTH bitwidth = BIT64;

static CON_COMPARISON get_comparison_inverse(const CON_COMPARISON& condition);

static std::string reg_to_str(const uint8_t& call_num, const CON_BITWIDTH& bitwidth);
static uint8_t str_to_reg(const std::string& reg_name);

static size_t find_macro_in_arg(const std::string& arg, const std::string& macro);
static void apply_macro_to_token(con_token* token, const vector<con_macro*>& macros);
static std::vector<con_token*> push_args(const std::vector<std::string>& args, const CON_BITWIDTH& bitwidth);

std::string comparison_to_string(const CON_COMPARISON& condition) {
  switch (condition) {
    case E:
      return "e";
    case NE:
      return "ne";
    case L:
      return "l";
    case G:
      return "g";
    case LE:
      return "le";
    case GE:
      return "ge";
  }
  throw invalid_argument("Invalid comparison value: "+to_string(static_cast<int>(condition)));
}

void apply_whiles(std::vector<con_token*>& tokens) {
  for (vector<con_token*>::iterator it = tokens.begin(); it != tokens.end(); ++it ) {
    apply_whiles((*it)->tokens);
    if ((*it)->tok_type != WHILE) {
      continue;
    }
    con_token* cmp_tok = new con_token(CMD);
    cmp_tok->tok_cmd->command = "cmp";
    cmp_tok->tok_cmd->arg1 = (*it)->tok_while->condition.arg1;
    cmp_tok->tok_cmd->arg2 = (*it)->tok_while->condition.arg2;

    string endtag_name = "endwhile" + to_string(while_amnt);
    string starttag_name = "startwhile" + to_string(while_amnt);
    ++while_amnt;

    con_token* jmp_tok = new con_token(CMD);
    jmp_tok->tok_cmd->command = "j" + comparison_to_string(get_comparison_inverse((*it)->tok_while->condition.op));
    jmp_tok->tok_cmd->arg1 = endtag_name;

    con_token* jmpbck_tok = new con_token(CMD);
    jmpbck_tok->tok_cmd->command = "jmp";
    jmpbck_tok->tok_cmd->arg1 = starttag_name;

    con_token* endwhile_tok = new con_token(TAG);
    endwhile_tok->tok_tag->name = endtag_name;

    con_token* startwhile_tok = new con_token(TAG);
    startwhile_tok->tok_tag->name = starttag_name;

    // starttag, cmp, jmp endtag, ..., jmp starttag, endtag
    (*it)->tokens.insert((*it)->tokens.begin(), jmp_tok);
    (*it)->tokens.insert((*it)->tokens.begin(), cmp_tok);
    (*it)->tokens.insert((*it)->tokens.begin(), startwhile_tok);
    (*it)->tokens.push_back(jmpbck_tok);
    (*it)->tokens.push_back(endwhile_tok);
  }
}
void apply_ifs(std::vector<con_token*>& tokens) {
  for (vector<con_token*>::iterator it = tokens.begin(); it != tokens.end(); ++it ) {
    apply_ifs((*it)->tokens);
    if ((*it)->tok_type != IF) {
      continue;
    }
    con_token* cmp_tok = new con_token(CMD);
    cmp_tok->tok_cmd->command = "cmp";
    cmp_tok->tok_cmd->arg1 = (*it)->tok_if->condition.arg1;
    cmp_tok->tok_cmd->arg2 = (*it)->tok_if->condition.arg2;

    string tagname = "endif" + to_string(if_amnt);
    ++if_amnt;

    con_token* jmp_tok = new con_token(CMD);
    jmp_tok->tok_cmd->command = "j" + comparison_to_string(get_comparison_inverse((*it)->tok_if->condition.op));
    jmp_tok->tok_cmd->arg1 = tagname;

    con_token* endif_tok = new con_token(TAG);
    endif_tok->tok_tag->name = tagname;

    // cmp, jmp tag, ..., tag
    (*it)->tokens.insert((*it)->tokens.begin(), jmp_tok);
    (*it)->tokens.insert((*it)->tokens.begin(), cmp_tok);
    (*it)->tokens.push_back(endif_tok);
  }
}
void apply_functions(std::vector<con_token*>& tokens) {
  for (vector<con_token*>::iterator it = tokens.begin(); it != tokens.end(); ++it ) {
    if ((*it)->tok_type != FUNCTION) {
      continue;
    }
    con_function* crntfunc = (*it)->tok_function;
    if (crntfunc->name == "main") {
      crntfunc->name = "_start";
    }

    con_token* tag_tok = new con_token(TAG);
    tag_tok->tok_tag->name = crntfunc->name;
    for (size_t j = 0; j < crntfunc->arguments.size(); ++j) {
      con_token* arg_tok = new con_token(MACRO);
      arg_tok->tok_macro->value = reg_to_str(j, bitwidth);
      arg_tok->tok_macro->macro = crntfunc->arguments[j];

      (*it)->tokens.insert((*it)->tokens.begin(), arg_tok);
    }
    (*it)->tokens.insert((*it)->tokens.begin(), tag_tok);
    con_token* ret_tok = new con_token(CMD);
    ret_tok->tok_cmd->command = "ret";
    (*it)->tokens.push_back(ret_tok);
  }
}
void apply_macros(std::vector<con_token*>& tokens, std::vector<con_macro*>& knownmacros) {
  for (vector<con_token*>::iterator it = tokens.begin(); it != tokens.end(); ++it ) {
    if ((*it)->tok_type == MACRO) {
      knownmacros.push_back((*it)->tok_macro);
      continue;
    }
    apply_macro_to_token(*it, knownmacros);
    if ((*it)->tok_type == IF || (*it)->tok_type == WHILE || (*it)->tok_type == FUNCTION) {
      apply_macros((*it)->tokens, knownmacros);
    }
  }
}
void apply_funcalls(std::vector<con_token*>& tokens) {
  for (vector<con_token*>::iterator it = tokens.begin(); it != tokens.end(); ++it ) {
    apply_funcalls((*it)->tokens);
    if ((*it)->tok_type != FUNCALL) {
      continue;
    }
    vector<con_token*> arg_tokens = push_args((*it)->tok_funcall->arguments, bitwidth);
    con_token* call_tok = new con_token(CMD);
    call_tok->tok_cmd->command = "call";
    call_tok->tok_cmd->arg1 = (*it)->tok_funcall->funcname;
    arg_tokens.push_back(call_tok);

    it = tokens.insert(it+1, arg_tokens.begin(), arg_tokens.end()) - 1;
  }
}
void apply_syscalls(std::vector<con_token*>& tokens) {
  for (vector<con_token*>::iterator it = tokens.begin(); it != tokens.end(); ++it ) {
    apply_syscalls((*it)->tokens);
    if ((*it)->tok_type != SYSCALL) {
      continue;
    }
    vector<con_token*> arg_tokens = push_args((*it)->tok_syscall->arguments, bitwidth);
    con_token* call_tok1 = new con_token(CMD);
    call_tok1->tok_cmd->command = "mov";
    call_tok1->tok_cmd->arg1 = "rax";
    call_tok1->tok_cmd->arg2 = to_string((*it)->tok_syscall->number);
    arg_tokens.push_back(call_tok1);
    con_token* call_tok2 = new con_token(CMD);
    call_tok2->tok_cmd->command = "syscall";
    arg_tokens.push_back(call_tok2);

    it = tokens.insert(it+1, arg_tokens.begin(), arg_tokens.end()) - 1;
  }
}

void linearize_tokens(std::vector<con_token*>& tokens) {
  vector<con_token*>::iterator it = tokens.begin();
  while (it != tokens.end()) {
    if ((*it)->tok_type != IF && (*it)->tok_type != WHILE && (*it)->tok_type != FUNCTION) {
      ++it;
    } else {
      it = tokens.insert(it+1, (*it)->tokens.begin(), (*it)->tokens.end()) - 1;
      (*it)->tokens.clear(); // moved the pointers to tokens
      delete *it;
      it = tokens.erase(it);
    }
  }
}

std::string tokens_to_nasm(const std::vector<con_token*>& tokens) {
  string output = "";
  for (vector<con_token*>::const_iterator c_it = tokens.cbegin(); c_it != tokens.cend(); ++c_it ) {
    if ((*c_it)->tok_type == WHILE || (*c_it)->tok_type == IF
        || (*c_it)->tok_type == FUNCTION || (*c_it)->tok_type == MACRO
        || (*c_it)->tok_type == FUNCALL || (*c_it)->tok_type == SYSCALL) {
      continue;
    }
    if ((*c_it)->tok_type == SECTION) {
      output += "section " + (*c_it)->tok_section->name;
    } else if ((*c_it)->tok_type == TAG) {
      output += (*c_it)->tok_tag->name + ":";
    } else if ((*c_it)->tok_type == CMD) {
      output += (*c_it)->tok_cmd->command;
      if (!(*c_it)->tok_cmd->arg1.empty()) {
        output += " " + (*c_it)->tok_cmd->arg1;
      }
      if (!(*c_it)->tok_cmd->arg2.empty()) {
        output += ", " + (*c_it)->tok_cmd->arg2;
      }
    }
    output += "\n";
  }
  return output;
}

// ----- ----- ----- ----- ----- ----- helper functions impl ----- ----- ----- ----- -----

CON_COMPARISON get_comparison_inverse(const CON_COMPARISON& condition) {
  switch (condition) {
    case E:
      return NE;
    case NE:
      return E;
    case L:
      return GE;
    case G:
      return LE;
    case LE:
      return G;
    case GE:
      return L;
  }
  throw invalid_argument("Invalid comparison value: "+to_string(static_cast<int>(condition)));
}

std::string reg_to_str(const uint8_t& call_num, const CON_BITWIDTH& bitwidth) {
  switch (bitwidth) {
    case BIT8:
      switch (call_num) {
        case 0:
          return "dil";
        break;
        case 1:
          return "sil";
        break;
        case 2:
          return "dl";
        break;
        case 3:
          return "cl";
        break;
        case 4:
          return "r8b";
        break;
        case 5:
          return "r9b";
        break;
      }
    break;
    case BIT16:
      switch (call_num) {
        case 0:
          return "di";
        break;
        case 1:
          return "si";
        break;
        case 2:
          return "dx";
        break;
        case 3:
          return "cx";
        break;
        case 4:
          return "r8w";
        break;
        case 5:
          return "r9w";
        break;
      }
    break;
    case BIT32:
      switch (call_num) {
        case 0:
          return "edi";
        break;
        case 1:
          return "esi";
        break;
        case 2:
          return "edx";
        break;
        case 3:
          return "ecx";
        break;
        case 4:
          return "r8d";
        break;
        case 5:
          return "r9d";
        break;
      }
    break;
    case BIT64:
      switch (call_num) {
        case 0:
          return "rdi";
        break;
        case 1:
          return "rsi";
        break;
        case 2:
          return "rdx";
        break;
        case 3:
          return "rcx";
        break;
        case 4:
          return "r8";
        break;
        case 5:
          return "r9";
        break;
      }
    break;
  }
  throw invalid_argument("Invalid bitwidth or call_num: bitwidth="+to_string(static_cast<int>(bitwidth))
                         +" call_num="+to_string(static_cast<int>(call_num)));
}
uint8_t str_to_reg(const std::string& reg_name) {
  if (reg_name=="dil" ||reg_name=="di" || reg_name=="edi" || reg_name=="rdi")
    return 0;
  if (reg_name=="sil" ||reg_name=="si" || reg_name=="esi" || reg_name=="rsi")
    return 1;
  if (reg_name=="dl" || reg_name=="dx" || reg_name=="edx" || reg_name=="rdx")
    return 2;
  if (reg_name=="cl" || reg_name=="cx" || reg_name=="ecx" || reg_name=="rcx")
    return 3;
  if (reg_name=="r8b" || reg_name=="r8w" || reg_name=="r8d" || reg_name=="r8")
    return 4;
  if (reg_name=="r9b" || reg_name=="r9w" || reg_name=="r9d" || reg_name=="r9")
    return 5;
  return 6;
}

size_t find_macro_in_arg(const std::string& arg, const std::string& macro) {
  size_t pos = arg.find(macro);
  if ((pos == 0 || (arg[pos-1]!='_' && !isalpha(arg[pos-1])))
      && (pos+macro.size()-1 == arg.size()-1 || (arg[pos+macro.size()]!='_' && !isalpha(arg[pos+macro.size()])))) {
    return pos;
  }
  return string::npos;
}
void apply_macro_to_token(con_token* token, const vector<con_macro*>& macros) {
  if (token->tok_type != WHILE && token->tok_type != IF && token->tok_type != CMD) {
    return;
  }
  // Unoptimal, but more clear imo
  for (vector<con_macro*>::const_iterator c_it = macros.cbegin(); c_it != macros.cend(); ++c_it ) {
    const string& macro = (*c_it)->macro;
    const string& value = (*c_it)->value;
    size_t pos;
    switch (token->tok_type) {
      case WHILE:
        pos = find_macro_in_arg(token->tok_while->condition.arg1, macro);
        while (pos != string::npos) {
          token->tok_while->condition.arg1.replace(pos, macro.size(), value);
          pos = find_macro_in_arg(token->tok_while->condition.arg1, macro);
        }
        pos = find_macro_in_arg(token->tok_while->condition.arg2, macro);
        while (pos != string::npos) {
          token->tok_while->condition.arg2.replace(pos, macro.size(), value);
          pos = find_macro_in_arg(token->tok_while->condition.arg2, macro);
        }
      break;
      case IF:
        pos = find_macro_in_arg(token->tok_if->condition.arg1, macro);
        while (pos != string::npos) {
          token->tok_if->condition.arg1.replace(pos, macro.size(), value);
          pos = find_macro_in_arg(token->tok_if->condition.arg1, macro);
        }
        pos = find_macro_in_arg(token->tok_if->condition.arg2, macro);
        while (pos != string::npos) {
          token->tok_if->condition.arg2.replace(pos, macro.size(), value);
          pos = find_macro_in_arg(token->tok_if->condition.arg2, macro);
        }
      break;
      case CMD:
        pos = find_macro_in_arg(token->tok_cmd->arg1, macro);
        while (pos != string::npos) {
          token->tok_cmd->arg1.replace(pos, macro.size(), value);
          pos = find_macro_in_arg(token->tok_cmd->arg1, macro);
        }
        pos = find_macro_in_arg(token->tok_cmd->arg2, macro);
        while (pos != string::npos) {
          token->tok_cmd->arg2.replace(pos, macro.size(), value);
          pos = find_macro_in_arg(token->tok_cmd->arg2, macro);
        }
      break;
      default:
      break;
    }
  }
}
std::vector<con_token*> push_args(const std::vector<std::string>& args, const CON_BITWIDTH& bitwidth) {
  vector<con_token*> arg_tokens;

  // stack args;
  for (size_t i = 6; i < args.size() ; ++i) {
    size_t i_rev = args.size()+5 - i;
    con_token* arg_tok = new con_token(CMD);
    arg_tok->tok_cmd->command = "push"; // bitwidth
    arg_tok->tok_cmd->arg1 = args[i_rev];
    arg_tokens.push_back(arg_tok);
  }

  // register args;
  size_t reg_args_size = min(args.size(),6);
  uint8_t first_read[7] = {6,6,6,6,6,6,6}; // cell 6 is garbage to hold not special-regs
  for (size_t i = 0; i < reg_args_size; ++i) {
    uint8_t reg_num = str_to_reg(args[i]);
    first_read[reg_num] = min(first_read[reg_num],i);
  }
  // sort regs by first-read
  uint8_t read_order[6] = {6,6,6,6,6,6};
  for (size_t fr = 0; fr < reg_args_size; ++fr) {
    for (size_t reg = 0; reg < reg_args_size; ++reg) {
      if ((fr == first_read[reg]) && (first_read[reg] > reg)) { //next in turn and will be pushed to stack
        read_order[fr]=reg;
      }
    }
  }
  // push reversed to pop order
  for (size_t fr = 0; fr < 6; ++fr) {
    size_t fr_rev = 5 - fr; // reverse the order
    if (read_order[fr_rev] != 6) { // there is a regester first read i arg number fr, and will be deleted before
      con_token* arg_tok = new con_token(CMD);
      arg_tok->tok_cmd->command = "push";
      arg_tok->tok_cmd->arg1 = reg_to_str(read_order[fr_rev], bitwidth);
      arg_tokens.push_back(arg_tok);
    }
  }
  // set each arg and track values places
  uint8_t current_val_place[6] = {0,1,2,3,4,5}; // 6 means stack
  for (size_t reg = 0; reg < reg_args_size; ++reg) {
    if (first_read[reg] > reg) {
      current_val_place[reg] = 6;
    }
  }
  for (size_t i = 0; i < reg_args_size; ++i) {
    con_token* arg_tok = new con_token(CMD);
    uint8_t wanted_reg = str_to_reg(args[i]);
    if (wanted_reg==6) {
      arg_tok->tok_cmd->command = "mov";
      arg_tok->tok_cmd->arg1 = reg_to_str(i, bitwidth);
      arg_tok->tok_cmd->arg2 = args[i];
      // if regi was read before, then current_val_place[i] is a previous register (correct)
      // if regi isn't read yet, then current_val_place[i] is stack (correct)
    } else {
      if (current_val_place[wanted_reg] == 6) {
        arg_tok->tok_cmd->command = "pop";
        arg_tok->tok_cmd->arg1 = reg_to_str(i, bitwidth);
        current_val_place[wanted_reg] = i; // wanted_reg moved from stack to regi
      } else {
        if (i != current_val_place[wanted_reg]) {
          arg_tok->tok_cmd->command = "mov";
          arg_tok->tok_cmd->arg1 = reg_to_str(i, bitwidth);
          arg_tok->tok_cmd->arg2 = reg_to_str(current_val_place[wanted_reg], bitwidth);
          // if regi was read before, then current_val_place[i] is a previous register (correct)
          // if regi isn't read yet, then current_val_place[i] is stack (correct)
          current_val_place[wanted_reg] = min(current_val_place[wanted_reg],i);
        } else {
          arg_tok->tok_cmd->command = "nop";
        }
      }
    }
    if (arg_tok->tok_cmd->command == "nop") {
      delete arg_tok;
      arg_tok = nullptr;
    } else {
      arg_tokens.push_back(arg_tok);
    }
  }
  return arg_tokens;
}
