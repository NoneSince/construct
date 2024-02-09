#include <string>
#include <vector>
#include <stack>
#include <map>
#include <stdexcept>
#include "deconstruct.h"
#include "construct_types.h"

using namespace std;

static int get_line_indentation(const std::string& line);
static CON_TOKENTYPE get_token_type(const std::string& line, const bool& in_data); // Expects formatted line
static CON_COMPARISON str_to_comparison(const std::string& comp);
static CON_BITWIDTH len_to_bitwidth(const std::string& len);

static con_section* parse_section(const std::string& line);
static con_tag* parse_tag(const std::string& line);
static con_while* parse_while(const std::string& line);
static con_if* parse_if(const std::string& line);
static con_function* parse_function(const std::string& line);
static con_cmd* parse_cmd(const std::string& line);
static con_macro* parse_macro(const std::string& line);
static con_funcall* parse_funcall(const std::string& line);
static con_syscall* parse_syscall(const std::string& line);
static con_data* parse_data(const std::string& line);

static con_token* parse_line(const std::string& line, const bool& in_data);

static std::vector<std::string> split(const std::string& input, const std::string& delims);
static std::vector<std::string> split_first(const std::string& input, const std::string& delims);
static std::string join(const std::vector<std::string>& input, const std::string& delim);
static std::string remove_duplicate(const std::string& input, const char& c);
static std::string strip_left(const std::string& input, const std::string& delims);
static std::string strip_right(const std::string& input, const std::string& delims);
static std::string strip(const std::string& input, const std::string& delims);

static uint16_t get_syscall_number(const std::string& syscall_name);

std::vector<con_token*> delinearize_tokens(std::vector<con_token*> tokens) {
  // Serves as parent "section" where all tokens belong to, convenient for algo
  con_token parent_token(SECTION);
  parent_token.indentation = -1;

  stack<con_token*> parent_stack;
  parent_stack.push(&parent_token);

  // When a new while, if or function is encountered it is pushed to the top of the parent_stack
  // All tokens with the indentation of the top of the parent_stack+1
  // are then added to the elem at the top of the stack (ptr so also to elem in vector).
  // If token is while, if or function it is pushed to stack and becomes new parent.
  // if indentation goes up, new token is pushed to stack, when indentation goes down,
  // tops of stack are popped off by how much it decreased.
  for (vector<con_token*>::iterator it = tokens.begin(); it != tokens.end(); ++it) {
    int delta_indentation = (*it)->indentation - parent_stack.top()->indentation;
    if (delta_indentation <= 0) {
      int indentation_diff = -1*delta_indentation + 1;
      for (int i = 0; i < indentation_diff; ++i) {
        parent_stack.pop();
      }
    }
    parent_stack.top()->tokens.push_back(*it);
    if ((*it)->tok_type == WHILE || (*it)->tok_type == IF || (*it)->tok_type == FUNCTION) {
      parent_stack.push(*it);
    }
  }

  vector<con_token*> delinearized_tokens = parent_token.tokens;
  parent_token.tokens.clear(); // moved the pointers to delinearized_tokens
  return delinearized_tokens;
}

std::vector<con_token*> parse_construct(const std::string& code) {
  vector<string> code_split = split(code, "\n");
  vector<con_token*> tokens;
  bool in_data = false;
  for (size_t i = 0; i < code_split.size(); ++i) {
    // Check if it contains any alphabet chars
    if (code_split[i].find_first_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!") == std::string::npos) {
      continue;
    }
    con_token* new_token = nullptr;
    try {
      new_token = parse_line(code_split[i], in_data);
      new_token->indentation = get_line_indentation(code_split[i]);
      assert_throw(tokens.empty() || new_token->indentation - tokens.back()->indentation <= 1,
        invalid_argument("Syntax error: extra indentation: indentation jumped from "+
          to_string(tokens.back()->indentation)+" to "+to_string(new_token->indentation)+"!"));
    }
    catch (const std::exception& e) {
      throw std::runtime_error("Line "+to_string(i)+" ["+code_split[i]+"]: "+e.what());
    }
    if (new_token->tok_type == SECTION
        && (new_token->tok_section->name == ".data" || new_token->tok_section->name == ".bss")) {
      in_data = true;
    } else if (new_token->tok_type == SECTION && new_token->tok_section->name == ".text") {
      in_data = false;
    }
    tokens.push_back(new_token);
  }
  return tokens;
}

// ----- ----- ----- ----- ----- ----- helper functions impl ----- ----- ----- ----- -----

int get_line_indentation(const std::string& line) {
  int indentation = 0;
  for (string::const_iterator c_it = line.begin(); c_it != line.end(); ++c_it) {
    if (*c_it != '\t') break;
    ++indentation;
  }
  return indentation;
}
CON_TOKENTYPE get_token_type(const std::string& line, const bool& in_data) {
  vector<string> line_split = split(line, " "); // line_split is not empty
  if (line_split[0] == "section")
    return SECTION;
  if (line.find(' ') == string::npos && line[line.size()-1] == ':')
    return TAG;
  if (line_split[0] == "while")
    return WHILE;
  if (line_split[0] == "if")
    return IF;
  if (line_split[0] == "function")
    return FUNCTION;
  if (line[0] == '!')
    return MACRO;
  if (line_split[0] == "call" && line.find('(') != string::npos && line.find(')') != string::npos)
    return FUNCALL;
  if (line_split[0] == "syscall" && line.find('(') != string::npos && line.find(')') != string::npos)
    return SYSCALL;
  if (in_data)
    return DATA;
  return CMD;
}
CON_COMPARISON str_to_comparison(const std::string& comp) {
  if (comp == "e")
    return E;
  if (comp == "ne")
    return NE;
  if (comp == "l")
    return L;
  if (comp == "g")
    return G;
  if (comp == "le")
    return LE;
  if (comp == "ge")
    return GE;
  throw invalid_argument("Invalid comparison: "+comp);
}
CON_BITWIDTH len_to_bitwidth(const std::string& len) {
  if (len == "db")
    return BIT8;
  if (len == "dw")
    return BIT16;
  if (len == "dd")
    return BIT32;
  if (len == "dq")
    return BIT64;
  throw invalid_argument("Invalid function argument length: "+len);
}

con_section* parse_section(const std::string& line) { // section name // section . name ??
  con_section* tok_section = new con_section();
  vector<string> line_split = split(line, " ");
  tok_section->name = line_split[1];
  return tok_section;
}
con_tag* parse_tag(const std::string& line) { // name: // name : ??
  con_tag* tok_tag = new con_tag();
  tok_tag->name = line.substr(0, line.size()-1);
  return tok_tag;
}
con_while* parse_while(const std::string& line) { // while val1 comp val2:
  con_while* tok_while = new con_while();
  vector<string> line_split = split(line, " :");
  tok_while->condition.arg1 = line_split[1];
  tok_while->condition.op = str_to_comparison(line_split[2]);
  tok_while->condition.arg2 = line_split[3];
  return tok_while;
}
con_if* parse_if(const std::string& line) { // if val1 comp val2:
  con_if* tok_if = new con_if();
  vector<string> line_split = split(line, " :");
  tok_if->condition.arg1 = line_split[1];
  tok_if->condition.op = str_to_comparison(line_split[2]);
  tok_if->condition.arg2 = line_split[3];
  return tok_if;
}
con_function* parse_function(const std::string& line) { // function func(arg1: len1, arg2: len2, ...):
  con_function* tok_function = new con_function();
  vector<string> line_split = split(line, "()"); // "function func" "arg1: len1, arg2: len2, ..." ":" *with spaces
  assert_throw(line_split.size()==2 || line_split.size()==3, invalid_argument("Invalid syntax"));
  assert_throw(strip(line_split[line_split.size()-1], " ")==":", invalid_argument("Invalid syntax"));

  vector<string> function_name = split(line_split[0], " ");
  assert_throw(function_name.size()==2, invalid_argument("Invalid syntax"));
  assert_throw(function_name[0]=="function", invalid_argument("Invalid syntax"));
  tok_function->name = function_name[1];

  if (line_split.size()==3) {
    vector<string> args_lens = split(line_split[1], ",");
    for (vector<string>::const_iterator c_it = args_lens.cbegin(); c_it != args_lens.cend(); ++c_it) {
      vector<string> arg_len = split(*c_it, ":");
      assert_throw(arg_len.size()==2, invalid_argument("Invalid syntax"));
      arg_len[0] = remove_duplicate(arg_len[0], ' ');
      arg_len[1] = remove_duplicate(arg_len[1], ' ');
      tok_function->arguments.emplace_back(arg_len[0],len_to_bitwidth(arg_len[1]));
    }
  }
  return tok_function;
}
con_cmd* parse_cmd(const std::string& line) { // op // op arg1 // op arg1, arg2
  con_cmd* tok_cmd = new con_cmd();
  bool arg2_exists = false;
  vector<string> line_split = split(line, ",");
  assert_throw(line_split.size() <= 2,
    invalid_argument("Syntax error: extra commas: the line has "+ to_string(line_split.size()-1)+" lines!"));
  assert_throw(line.back() != ',', invalid_argument("Syntax error: second argument does not exist!"));
  if (line_split.size() == 2) {
    arg2_exists = true;
    tok_cmd->arg2 = remove_duplicate(line_split[1], ' ');
  }
  line_split = split_first(line_split[0], " ");
  assert_throw(line_split.size() != 0, invalid_argument("Syntax error: command and first argument do not exist!"));
  tok_cmd->command = line_split[0];
  if (line_split.size() == 2) {
    tok_cmd->arg1 = remove_duplicate(line_split[1], ' ');
  } else {
    assert_throw(!arg2_exists, invalid_argument("Syntax error: first argument does not exist!"));
  }
  return tok_cmd;
}
con_macro* parse_macro(const std::string& line) { // !name reg
  con_macro* tok_macro = new con_macro();
  vector<string> line_split = split(line, " !");
  tok_macro->macro = line_split[0];
  tok_macro->value = line_split[1];
  return tok_macro;
}
con_funcall* parse_funcall(const std::string& line) { // call func(arg1, arg2, ...)
  con_funcall* tok_funcall = new con_funcall();
  vector<string> line_split = split(line, " (),");
  tok_funcall->funcname = line_split[1];
  for (size_t i = 2; i < line_split.size(); ++i) {
    assert_throw(!line_split[i].empty(), invalid_argument("Invalid syntax"));
    tok_funcall->arguments.push_back(line_split[i]);
  }
  return tok_funcall;
}
con_syscall* parse_syscall(const std::string& line) { // syscall sysc(arg1, arg2, ...)
  con_syscall* tok_syscall = new con_syscall();
  vector<string> line_split = split(line, " (),");
  tok_syscall->number = get_syscall_number(line_split[1]);
  for (size_t i = 2; i < line_split.size(); ++i) {
    assert_throw(!line_split[i].empty(), invalid_argument("Invalid syntax"));
    tok_syscall->arguments.push_back(line_split[i]);
  }
  return tok_syscall;
}
con_data* parse_data(const std::string& line) {
  con_data* tok_data = new con_data();
  tok_data->line = line;
  con_token print;
  print.tok_type = DATA;
  print.tok_data = tok_data;
  print.tok_data = nullptr;
  return tok_data;
}

con_token* parse_line(const std::string& line, const bool& in_data) {
  con_token* token = new con_token;
  //remove multiple spaces from line
  string f_line = "";
  bool caught_space = false;
  for (string::const_iterator c_it = line.cbegin(); c_it != line.cend(); ++c_it) {
    bool is_space = (*c_it == ' ');
    if (*c_it == '\t' || (is_space && caught_space)) continue;
    f_line += *c_it;
    caught_space = is_space;
  }
  token->tok_type = get_token_type(f_line, in_data);
  switch (token->tok_type) {
    case SECTION:
      token->tok_section = parse_section(f_line);
      break;
    case TAG:
      token->tok_tag = parse_tag(f_line);
      break;
    case WHILE:
      token->tok_while = parse_while(f_line);
      break;
    case IF:
      token->tok_if = parse_if(f_line);
      break;
    case FUNCTION:
      token->tok_function = parse_function(f_line);
      break;
    case CMD:
      token->tok_cmd = parse_cmd(f_line);
      break;
    case MACRO:
      token->tok_macro = parse_macro(f_line);
      break;
    case FUNCALL:
      token->tok_funcall = parse_funcall(f_line);
      break;
    case SYSCALL:
      token->tok_syscall = parse_syscall(f_line);
      break;
    case DATA:
      token->tok_data = parse_data(f_line);
      break;
  }
  return token;
}

std::vector<std::string> split(const std::string& input, const std::string& delims) {
  vector<string> result;
  string tmp;
  for (string::const_iterator input_it = input.cbegin(); input_it != input.cend(); ++input_it) {
    bool is_delim = false;
    for (string::const_iterator delims_it = delims.cbegin(); delims_it != delims.cend(); ++delims_it) {
      if (*delims_it == *input_it) {
        is_delim = true;
        break;
      }
    }
    if (is_delim) {
      if (!tmp.empty()) {
        result.push_back(tmp);
        tmp.clear();
      }
    } else {
      tmp.push_back(*input_it);
    }
  }
  if (!tmp.empty())
    result.push_back(tmp);
  return result;
}
std::vector<std::string> split_first(const std::string& input, const std::string& delims) {
  vector<string> result;
  string first_word;
  string else_word;
  for (string::const_iterator first_word_it = input.cbegin(); first_word_it != input.cend(); ++first_word_it) {
    bool is_delim = false;
    for (string::const_iterator delims_it = delims.cbegin(); delims_it != delims.cend(); ++delims_it) {
      if (*delims_it == *first_word_it) {
        is_delim = true;
        break;
      }
    }
    if (is_delim) {
      if (!first_word.empty()) {
        result.push_back(first_word);
        for (string::const_iterator else_word_it = first_word_it+1; else_word_it != input.cend(); ++else_word_it) {
          else_word.push_back(*else_word_it);
        }
        if (!else_word.empty())
          result.push_back(else_word);
        return result;
      }
    } else {
      first_word.push_back(*first_word_it);
    }
  }
  if (!first_word.empty())
    result.push_back(first_word);
  return result;
}
std::string join(const std::vector<std::string>& input, const std::string& delim) {
  string result;
  if (!input.empty()) {
    result = *input.cbegin();
    for (vector<string>::const_iterator input_it = input.cbegin()+1; input_it != input.cend(); ++input_it) {
      result += delim + *input_it;
    }
  }
  return result;
}
std::string remove_duplicate(const std::string& input, const char& c) {
  string to_str = "";
  return join(split(input, to_str+c), to_str+c);
}
std::string strip_left(const std::string& input, const std::string& delims) {
  for (string::const_iterator input_it = input.cbegin(); input_it != input.cend(); ++input_it) {
    bool is_delim = false;
    for (string::const_iterator delims_it = delims.cbegin(); delims_it != delims.cend(); ++delims_it) {
      if (*delims_it == *input_it) {
        is_delim = true;
        break;
      }
    }
    if (!is_delim) {
      return std::string(input_it, input.cend());
    }
  }
  return "";
}
std::string strip_right(const std::string& input, const std::string& delims) {
  for (string::const_reverse_iterator input_it = input.crbegin(); input_it != input.crend(); ++input_it) {
    bool is_delim = false;
    for (string::const_iterator delims_it = delims.cbegin(); delims_it != delims.cend(); ++delims_it) {
      if (*delims_it == *input_it) {
        is_delim = true;
        break;
      }
    }
    if (!is_delim) {
      std::string revresed(input_it, input.crend());
      return std::string(revresed.crbegin(), revresed.crend());;
    }
  }
  return "";
}
std::string strip(const std::string& input, const std::string& delims) {
  return strip_right(strip_left(input, delims), delims);
}

uint16_t get_syscall_number(const std::string& syscall_name) {
  static const map<std::string, uint16_t>& name_to_num = {
    {"read"                  , 0  },
    {"write"                 , 1  },
    {"open"                  , 2  },
    {"close"                 , 3  },
    {"stat"                  , 4  },
    {"fstat"                 , 5  },
    {"lstat"                 , 6  },
    {"poll"                  , 7  },
    {"lseek"                 , 8  },
    {"mmap"                  , 9  },
    {"mprotect"              , 10 },
    {"munmap"                , 11 },
    {"brk"                   , 12 },
    {"rt_sigaction"          , 13 },
    {"rt_sigprocmask"        , 14 },
    {"rt_sigreturn"          , 15 },
    {"ioctl"                 , 16 },
    {"pread64"               , 17 },
    {"pwrite64"              , 18 },
    {"readv"                 , 19 },
    {"writev"                , 20 },
    {"access"                , 21 },
    {"pipe"                  , 22 },
    {"select"                , 23 },
    {"sched_yield"           , 24 },
    {"mremap"                , 25 },
    {"msync"                 , 26 },
    {"mincore"               , 27 },
    {"madvise"               , 28 },
    {"shmget"                , 29 },
    {"shmat"                 , 30 },
    {"shmctl"                , 31 },
    {"dup"                   , 32 },
    {"dup2"                  , 33 },
    {"pause"                 , 34 },
    {"nanosleep"             , 35 },
    {"getitimer"             , 36 },
    {"alarm"                 , 37 },
    {"setitimer"             , 38 },
    {"getpid"                , 39 },
    {"sendfile"              , 40 },
    {"socket"                , 41 },
    {"connect"               , 42 },
    {"accept"                , 43 },
    {"sendto"                , 44 },
    {"recvfrom"              , 45 },
    {"sendmsg"               , 46 },
    {"recvmsg"               , 47 },
    {"shutdown"              , 48 },
    {"bind"                  , 49 },
    {"listen"                , 50 },
    {"getsockname"           , 51 },
    {"getpeername"           , 52 },
    {"socketpair"            , 53 },
    {"setsockopt"            , 54 },
    {"getsockopt"            , 55 },
    {"clone"                 , 56 },
    {"fork"                  , 57 },
    {"vfork"                 , 58 },
    {"execve"                , 59 },
    {"exit"                  , 60 },
    {"wait4"                 , 61 },
    {"kill"                  , 62 },
    {"uname"                 , 63 },
    {"semget"                , 64 },
    {"semop"                 , 65 },
    {"semctl"                , 66 },
    {"shmdt"                 , 67 },
    {"msgget"                , 68 },
    {"msgsnd"                , 69 },
    {"msgrcv"                , 70 },
    {"msgctl"                , 71 },
    {"fcntl"                 , 72 },
    {"flock"                 , 73 },
    {"fsync"                 , 74 },
    {"fdatasync"             , 75 },
    {"truncate"              , 76 },
    {"ftruncate"             , 77 },
    {"getdents"              , 78 },
    {"getcwd"                , 79 },
    {"chdir"                 , 80 },
    {"fchdir"                , 81 },
    {"rename"                , 82 },
    {"mkdir"                 , 83 },
    {"rmdir"                 , 84 },
    {"creat"                 , 85 },
    {"link"                  , 86 },
    {"unlink"                , 87 },
    {"symlink"               , 88 },
    {"readlink"              , 89 },
    {"chmod"                 , 90 },
    {"fchmod"                , 91 },
    {"chown"                 , 92 },
    {"fchown"                , 93 },
    {"lchown"                , 94 },
    {"umask"                 , 95 },
    {"gettimeofday"          , 96 },
    {"getrlimit"             , 97 },
    {"getrusage"             , 98 },
    {"sysinfo"               , 99 },
    {"times"                 , 100},
    {"ptrace"                , 101},
    {"getuid"                , 102},
    {"syslog"                , 103},
    {"getgid"                , 104},
    {"setuid"                , 105},
    {"setgid"                , 106},
    {"geteuid"               , 107},
    {"getegid"               , 108},
    {"setpgid"               , 109},
    {"getppid"               , 110},
    {"getpgrp"               , 111},
    {"setsid"                , 112},
    {"setreuid"              , 113},
    {"setregid"              , 114},
    {"getgroups"             , 115},
    {"setgroups"             , 116},
    {"setresuid"             , 117},
    {"getresuid"             , 118},
    {"setresgid"             , 119},
    {"getresgid"             , 120},
    {"getpgid"               , 121},
    {"setfsuid"              , 122},
    {"setfsgid"              , 123},
    {"getsid"                , 124},
    {"capget"                , 125},
    {"capset"                , 126},
    {"rt_sigpending"         , 127},
    {"rt_sigtimedwait"       , 128},
    {"rt_sigqueueinfo"       , 129},
    {"rt_sigsuspend"         , 130},
    {"sigaltstack"           , 131},
    {"utime"                 , 132},
    {"mknod"                 , 133},
    {"uselib"                , 134},
    {"personality"           , 135},
    {"ustat"                 , 136},
    {"statfs"                , 137},
    {"fstatfs"               , 138},
    {"sysfs"                 , 139},
    {"getpriority"           , 140},
    {"setpriority"           , 141},
    {"sched_setparam"        , 142},
    {"sched_getparam"        , 143},
    {"sched_setscheduler"    , 144},
    {"sched_getscheduler"    , 145},
    {"sched_get_priority_max", 146},
    {"sched_get_priority_min", 147},
    {"sched_rr_get_interval" , 148},
    {"mlock"                 , 149},
    {"munlock"               , 150},
    {"mlockall"              , 151},
    {"munlockall"            , 152},
    {"vhangup"               , 153},
    {"modify_ldt"            , 154},
    {"pivot_root"            , 155},
    {"_sysctl"               , 156},
    {"prctl"                 , 157},
    {"arch_prctl"            , 158},
    {"adjtimex"              , 159},
    {"setrlimit"             , 160},
    {"chroot"                , 161},
    {"sync"                  , 162},
    {"acct"                  , 163},
    {"settimeofday"          , 164},
    {"mount"                 , 165},
    {"umount2"               , 166},
    {"swapon"                , 167},
    {"swapoff"               , 168},
    {"reboot"                , 169},
    {"sethostname"           , 170},
    {"setdomainname"         , 171},
    {"iopl"                  , 172},
    {"ioperm"                , 173},
    {"create_module"         , 174},
    {"init_module"           , 175},
    {"delete_module"         , 176},
    {"get_kernel_syms"       , 177},
    {"query_module"          , 178},
    {"quotactl"              , 179},
    {"nfsservctl"            , 180},
    {"getpmsg"               , 181},
    {"putpmsg"               , 182},
    {"afs_syscall"           , 183},
    {"tuxcall"               , 184},
    {"security"              , 185},
    {"gettid"                , 186},
    {"readahead"             , 187},
    {"setxattr"              , 188},
    {"lsetxattr"             , 189},
    {"fsetxattr"             , 190},
    {"getxattr"              , 191},
    {"lgetxattr"             , 192},
    {"fgetxattr"             , 193},
    {"listxattr"             , 194},
    {"llistxattr"            , 195},
    {"flistxattr"            , 196},
    {"removexattr"           , 197},
    {"lremovexattr"          , 198},
    {"fremovexattr"          , 199},
    {"tkill"                 , 200},
    {"time"                  , 201},
    {"futex"                 , 202},
    {"sched_setaffinity"     , 203},
    {"sched_getaffinity"     , 204},
    {"set_thread_area"       , 205},
    {"io_setup"              , 206},
    {"io_destroy"            , 207},
    {"io_getevents"          , 208},
    {"io_submit"             , 209},
    {"io_cancel"             , 210},
    {"get_thread_area"       , 211},
    {"lookup_dcookie"        , 212},
    {"epoll_create"          , 213},
    {"epoll_ctl_old"         , 214},
    {"epoll_wait_old"        , 215},
    {"remap_file_pages"      , 216},
    {"getdents64"            , 217},
    {"set_tid_address"       , 218},
    {"restart_syscall"       , 219},
    {"semtimedop"            , 220},
    {"fadvise64"             , 221},
    {"timer_create"          , 222},
    {"timer_settime"         , 223},
    {"timer_gettime"         , 224},
    {"timer_getoverrun"      , 225},
    {"timer_delete"          , 226},
    {"clock_settime"         , 227},
    {"clock_gettime"         , 228},
    {"clock_getres"          , 229},
    {"clock_nanosleep"       , 230},
    {"exit_group"            , 231},
    {"epoll_wait"            , 232},
    {"epoll_ctl"             , 233},
    {"tgkill"                , 234},
    {"utimes"                , 235},
    {"vserver"               , 236},
    {"mbind"                 , 237},
    {"set_mempolicy"         , 238},
    {"get_mempolicy"         , 239},
    {"mq_open"               , 240},
    {"mq_unlink"             , 241},
    {"mq_timedsend"          , 242},
    {"mq_timedreceive"       , 243},
    {"mq_notify"             , 244},
    {"mq_getsetattr"         , 245},
    {"kexec_load"            , 246},
    {"waitid"                , 247},
    {"add_key"               , 248},
    {"request_key"           , 249},
    {"keyctl"                , 250},
    {"ioprio_set"            , 251},
    {"ioprio_get"            , 252},
    {"inotify_init"          , 253},
    {"inotify_add_watch"     , 254},
    {"inotify_rm_watch"      , 255},
    {"migrate_pages"         , 256},
    {"openat"                , 257},
    {"mkdirat"               , 258},
    {"mknodat"               , 259},
    {"fchownat"              , 260},
    {"futimesat"             , 261},
    {"newfstatat"            , 262},
    {"unlinkat"              , 263},
    {"renameat"              , 264},
    {"linkat"                , 265},
    {"symlinkat"             , 266},
    {"readlinkat"            , 267},
    {"fchmodat"              , 268},
    {"faccessat"             , 269},
    {"pselect6"              , 270},
    {"ppoll"                 , 271},
    {"unshare"               , 272},
    {"set_robust_list"       , 273},
    {"get_robust_list"       , 274},
    {"splice"                , 275},
    {"tee"                   , 276},
    {"sync_file_range"       , 277},
    {"vmsplice"              , 278},
    {"move_pages"            , 279},
    {"utimensat"             , 280},
    {"epoll_pwait"           , 281},
    {"signalfd"              , 282},
    {"timerfd_create"        , 283},
    {"eventfd"               , 284},
    {"fallocate"             , 285},
    {"timerfd_settime"       , 286},
    {"timerfd_gettime"       , 287},
    {"accept4"               , 288},
    {"signalfd4"             , 289},
    {"eventfd2"              , 290},
    {"epoll_create1"         , 291},
    {"dup3"                  , 292},
    {"pipe2"                 , 293},
    {"inotify_init1"         , 294},
    {"preadv"                , 295},
    {"pwritev"               , 296},
    {"rt_tgsigqueueinfo"     , 297},
    {"perf_event_open"       , 298},
    {"recvmmsg"              , 299},
    {"fanotify_init"         , 300},
    {"fanotify_mark"         , 301},
    {"prlimit64"             , 302},
    {"name_to_handle_at"     , 303},
    {"open_by_handle_at"     , 304},
    {"clock_adjtime"         , 305},
    {"syncfs"                , 306},
    {"sendmmsg"              , 307},
    {"setns"                 , 308},
    {"getcpu"                , 309},
    {"process_vm_readv"      , 310},
    {"process_vm_writev"     , 311},
    {"kcmp"                  , 312},
    {"finit_module"          , 313},
    {"sched_setattr"         , 314},
    {"sched_getattr"         , 315},
    {"renameat2"             , 316},
    {"seccomp"               , 317},
    {"getrandom"             , 318},
    {"memfd_create"          , 319},
    {"kexec_file_load"       , 320},
    {"bpf"                   , 321},
    {"execveat"              , 322},
    {"userfaultfd"           , 323},
    {"membarrier"            , 324},
    {"mlock2"                , 325},
    {"copy_file_range"       , 326},
    {"preadv2"               , 327},
    {"pwritev2"              , 328},
    {"pkey_mprotect"         , 329},
    {"pkey_alloc"            , 330},
    {"pkey_free"             , 331},
    {"statx"                 , 332}
  };

  try {
    return name_to_num.at(syscall_name);
  }
  catch(const std::out_of_range& e) {
    throw std::invalid_argument("Unknown syscall name: "+syscall_name);
  }
}
