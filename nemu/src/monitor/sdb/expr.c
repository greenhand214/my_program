/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>
#include <memory/paddr.h>
/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
  TK_NOTYPE = 256,
 
  TK_POS, TK_NEG, TK_DEREF,
  TK_EQ, TK_NEQ, TK_GT, TK_LT, TK_GE, TK_LE,
  TK_AND, TK_OR,

  TK_NUM, // 10 & 16 
  TK_REG,
  //TK_VAR,
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", TK_NOTYPE},    // spaces
  {"\\+", '+'},         // plus
  {"-", '-'},           // sub
  {"\\*", '*'},         // multiply
  {"/", '/'},           // div
  {"==", TK_EQ},        // equal
  {"\\s*\\(", '('},      // bra with optional whitespace
  {"\\s*\\)", ')'},      // ket with optional whitespace
  {"<", TK_LT},
  {">", TK_GT},
  {"<=", TK_LE},
  {">=",TK_GE},
  {"!=",TK_NEQ},
  {"&&", TK_AND},
  {"\\|\\|" ,TK_OR},


  {"(0x)?[0-9]+", TK_NUM}, // TODO: non-capture notation (?:pattern) makes compilation failed
  //{"\\$(\\$0|ra|[sgt]p|t[0-6]|a[0-7]|s([0-9]|1[0-1]))", TK_REG},//寄存器
  //{"w+", TK_REG},
  {"\\$\\w+", TK_REG},
 //{"[A-Za-z_]\\w*", TK_VAR},
};

//lp
#define NR_REGEX ARRLEN(rules)

#define OFTYPES(type, types) oftypes(type, types, ARRLEN(types))

static int bound_types[] = {')',TK_NUM,TK_REG}; // boundary for binary operator
static int nop_types[] = {'(',')',TK_NUM,TK_REG}; // not operator type
static int op1_types[] = {TK_NEG, TK_POS, TK_DEREF}; // unary operator type

static bool oftypes(int type, int types[], int size) {
  for (int i = 0; i < size; i++) {
    if (type == types[i]) return true;
  }
  return false;
}

static regex_t re[NR_REGEX] = {};
//static使变量只能被初始化一次
/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */

static word_t eval(int p, int q, bool *yes);
word_t expr(char *e, bool *success);
static int find_major(int p, int q);
bool check_parentheses(int p, int q);
static word_t eval_operand(int i, bool *yes);
static word_t calc1(int op, word_t val, bool *yes);
static word_t calc2(word_t val1, int op, word_t val2, bool *yes);
static word_t vaddr_read(vaddr_t addr, int len);

void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }else {
      printf("regex compilation succeeded: %s\n", rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[256];
} Token;
//类型加存储的token（字符串5a89b9）

static Token tokens[64] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  init_regex();

  nr_token = 0;

  printf("pmatch.rm_so = %d, pmatch.rm_eo = %d\n", pmatch.rm_so, pmatch.rm_eo);


  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s", i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */
        if (rules[i].token_type == TK_NOTYPE) break;
        if (nr_token < sizeof(tokens) / sizeof(tokens[0])) 
        tokens[nr_token].type = rules[i].token_type;

        switch (rules[i].token_type) {
          case TK_NUM:
          case TK_REG:
            // todo: handle overflow (token exceeding size of 32B)
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            tokens[nr_token].str[substr_len] = '\0';
            break;
            //记得加上终止符
          case '*':
          case '+':
          case '-':
          if (nr_token==0 || !OFTYPES(tokens[nr_token-1].type, bound_types))
          {
            switch (rules[i].token_type)
            {
              case '-': tokens[nr_token].type = TK_NEG; break;
              case '+': tokens[nr_token].type = TK_POS; break;
              case '*': tokens[nr_token].type = TK_DEREF; break;
            }
          }
        break;

        }
        //token[i].type 保存了所有输入的字符
        nr_token++;

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  /* TODO: Insert codes to evaluate the expression. */
  return eval(0, nr_token-1, success);
  //-1是因为最后一个字符是终止符
  return 0;
}

static word_t eval(int p, int q, bool *yes) {
  *yes = true;
  if (p > q) {
    *yes = false;
    return 0;
  } else if (p == q) {
    return eval_operand(p, yes);
  } else if (check_parentheses(p, q)) {
    return eval(p+1, q-1, yes);
  } else {    
    int major = find_major(p, q);
    if (major < 0) {
      *yes = false;
      return 0;
    }

    bool yes1, yes2;
    word_t val1 = eval(p, major-1, &yes1);
    word_t val2 = eval(major+1, q, &yes2);

    if (!yes2) {
      *yes = false;
      return 0;
    }
    if (yes1) {
      word_t ret = calc2(val1, tokens[major].type, val2, yes);
      printf("ret = %d\n",ret);
      return ret;
    } else {
      word_t ret =  calc1(tokens[major].type, val2, yes);
      return ret;
    }
  }
}

/*
uint32_t a = (1-2)/2; // a=0 有符号类型
uint32_t a = (uint32_t)(1-2)/2; // a=2147483647 无符号类型
将word_t（无符号）类型先转为sword_t（有符号）类型
*/
bool check_parentheses(int p, int q) {
  if (tokens[p].type=='(' && tokens[q].type==')') {
    int par = 0;
    for (int i = p; i <= q; i++) {
      if (tokens[i].type=='(') par++;
      else if (tokens[i].type==')') par--;

      if (par == 0) return i==q; // the leftest parenthese is matched
    }
  }
  return false;
}
//如果最外层括号匹配的话，则去除最外层的括号

static int find_major(int p, int q) {
  int ret = -1, par = 0, op_pre = 0;
  for (int i = p; i <= q; i++) {
    if (tokens[i].type == '(') {
      par++;
    } else if (tokens[i].type == ')') {
      if (par == 0) {
        return -1;
      }
      //找不到主运算符，非法表达式， 2 3
      par--;
    } else if (OFTYPES(tokens[i].type, nop_types)) {
      continue;
    } else if (par > 0) {
      continue;
    } else {
      int tmp_pre = 0;
      switch (tokens[i].type) {
      case TK_OR: tmp_pre++;
      case TK_AND: tmp_pre++;
      case TK_EQ: case TK_NEQ: tmp_pre++;
      case TK_LT: case TK_GT: case TK_GE: case TK_LE: tmp_pre++;
      case '+': case '-': tmp_pre++;
      case '*': case '/': tmp_pre++;
      case TK_NEG: case TK_DEREF: case TK_POS: tmp_pre++; break;
      default: return -1;
      }
      if (tmp_pre > op_pre || (tmp_pre == op_pre && !OFTYPES(tokens[i].type, op1_types))) {
        op_pre = tmp_pre;
        ret = i;
      }
    }
  }
  if (par != 0) return -1;
  return ret;
}
static word_t eval_operand(int i, bool *yes) {
  //char str[256];
  vaddr_t val;
  switch (tokens[i].type) {
  case TK_NUM:
    if (strncmp("0x", tokens[i].str, 2) == 0) {
      //sprintf(str, "%s",tokens[i].str);
      //val = strtoul(tokens[i].str, NULL, 16);
      sscanf(tokens[i].str, "%x", &val);
      //val = (word_t)tokens[i].str;
      return val;
      }
    else return strtol(tokens[i].str, NULL, 10);
  case TK_REG:
    return isa_reg_str2val(tokens[i].str, yes);
  default:
    *yes = false;
    return 0;
  }
}
// unary operator
static word_t calc1(int op, word_t val, bool *yes) {
  switch (op)
  {
  case TK_NEG: return -val;
  case TK_POS: return val;
  case TK_DEREF: return vaddr_read(val, 8);
  default: *yes = false;
  }
  return 0;
}

// binary operator
static word_t calc2(word_t val1, int op, word_t val2, bool *yes) {
  switch(op) {
  case '+': return val1 + val2;
  case '-': return val1 - val2;
  case '*': return val1 * val2;
  case '/': if (val2 == 0) {
    *yes = false;
    return 0;
  } 
  return (sword_t)val1 / (sword_t)val2; // e.g. -1/2, may not pass the expr test
  case TK_AND: return val1 && val2;
  case TK_OR: return val1 || val2;
  case TK_EQ: return val1 == val2;
  case TK_NEQ: return val1 != val2;
  case TK_GT: return val1 > val2;
  case TK_LT: return val1 < val2;
  case TK_GE: return val1 >= val2;
  case TK_LE: return val1 <= val2;
  default: *yes = false; return 0;
  }
}

static word_t vaddr_read(vaddr_t addr, int len) {
  return paddr_read(addr, len);
}
//

