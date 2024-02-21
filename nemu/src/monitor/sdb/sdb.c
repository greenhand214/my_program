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
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"
#include "watchpoint.h"

#include <common.h>

#include <memory/paddr.h> //注意包含头文件

static int is_batch_mode = false;
void test_expr();
void init_regex();
void init_wp_pool();
word_t vaddr_read(vaddr_t addr, int len);
void sdb_watchpoint_display();
void delete_watchpoint(int no);
void create_watchpoint(char* args);
/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets()
//返回一个指向char类型的指针
 {
  static char *line_read = NULL;
//这里的指针先是指向空的，后续会指向用户输入的命令的字符串
  if (line_read) {
    free(line_read);
    line_read = NULL;
  }
//初始化分配的内存，也就是用户输入的字符串那个内存，因为malloc的内存是动态分配的
  line_read = readline("(nemu) ");
//让line_read,指向用户输入的那串字符串
  if (line_read && *line_read) {
//用户输入内容的判断，如果是非空格，或者非EOF，就存入
    add_history(line_read);
  }
/* Place STRING at the end of the history list.
   The associated data field (if any) is set to NULL. 
   extern void add_history PARAMS((const char *));
   如果如果用户是有输入的，那么就将这些输入存到历史中去
*/
  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}


static int cmd_q(char *args) {
  nemu_state.state = NEMU_QUIT;
  return -1;
}

static int cmd_si(char *args) {
  char *arg = strtok(NULL, " ");
  int step = 0;
  if(arg == NULL) {
    cpu_exec(1);
    return 0;
  }
  sscanf(arg, "%d", &step);
  /*
  sscanf(const char *str, const char *format, ...)
  strcpy( dtm, "Saturday March 25 1989" );
  sscanf( dtm, "%s %s %d  %d", weekday, month, &day, &year );
  printf("%s %d, %d = %s\n", month, day, year, weekday );
  */
  if(step < -1){
    printf("Error,it should be bigger than -1");
    return 0;
  };
  cpu_exec(step);
  return 0;
}

static int cmd_info(char *args){
  if(args == NULL)
  printf("Type unknown args\n");
  else if(strcmp(args, "r") == 0)
  isa_reg_display();
  else if (strcmp(args, "w") == 0)
  sdb_watchpoint_display();
  return 0;
}

static int cmd_x(char *args){
  char *N = strtok(NULL, " ");
  char *EXPR = strtok(NULL, " ");
  int len;
  vaddr_t address;
  sscanf(N, "%d", &len);
  sscanf(EXPR, "%x", &address);
  for(int i = 0; i < len; i++){
    
    printf("0x%x:",address);
    printf("%08x\n",vaddr_read(address, 4));
    address = address + 4;
  }
  return 0;
}
/*
word_t vaddr_read(vaddr_t addr, int len) {
  return paddr_read(addr, len);
}
输入的参数为起始地址、扫描长度
*/

static int cmd_p(char *args){
  bool success = true;
  word_t res = expr(args, &success);
  printf("res is %d\n",res);
  printf("success is %d\n", success);
  if(!success){
    printf("invalid expression\n");
  }
  else{
    printf("%d\n",res);
  }
  return 0;
}

static int cmd_t(char *args){
  test_expr();
   return 0;
}
static int cmd_d (char *args){
    if(args == NULL)
	    printf("No args.\n");
    else{
	    delete_watchpoint(atoi(args));
    }
    return 0;
}
static int cmd_w(char* args){
  create_watchpoint(args);
  return 0;
}
static int cmd_help(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },

  /* TODO: Add more commands */
  { "si", "Single-step exection",cmd_si},
  { "info", "Register informaition",cmd_info},
  { "x", "Scan memory",cmd_x},
  { "p", "Calculate the expression",cmd_p},
  { "t", "Test the expression",cmd_t},
  { "d", "delete watchpoint", cmd_d},
  { "w", "create watchpoint", cmd_w},
};
/*
int (*handler)(char *) 是一个函数指针声明，表示 handler 是一个指向接受一个 char* 参数并返回 int 的函数的指针。
handler: 这是指针的名称，用于指向函数。
(*handler): 这表示 handler 是一个指针。(* ...) 是用于指定一个指针的语法。
(char *): 这是函数指针指向的函数的参数列表。在这种情况下，它表示这个函数接受一个 char* 类型的参数。
所以，整个声明 int (*handler)(char *) 表示 handler 是一个指向接受一个 char* 参数的函数，返回一个 int 的函数指针。
在你的代码中，cmd_table 数组中的每个元素都包含一个这样的函数指针，用于处理相应命令的执行。
也就是说可以使用cmd_table[i].xxx(cmd_help,cmd_c,cmd_q)执行相应的函数
*/
/*
注意cmd_c(char *args)函数中调用cpu_exec传入参数为-1，对应无符号数为MAX，所以单步执行只需要改变函数执行的次数即可
加入一个cmd_si(char *args)函数，并在下面对应的cmd_table[]中增加一个"si"选项就好了
*/
#define NR_CMD ARRLEN(cmd_table)
//输入几个命令，也就是数组的长度

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  //过滤空格
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  //检查是否处于批处理模式
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }
//rl_gets()函数获取用户在命令行中输入的文本。这个文本通常包括一个命令和可能的参数。
//用户输入的命令以字符串形式存储在str中，然后使用strtok()函数从字符串中提取第一个标记作为命令（cmd）。

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    //去掉字符串中的空格
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
 //如果args的起始位置超过了输入文本的末尾位置（args >= str_end），
 //则将参数设为NULL。这可能发生在用户只输入了命令而没有参数的情况下。
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
//如果找到匹配的命令，它会调用相应的处理函数，并将参数传递给它。
//如果处理函数返回小于0的值，表示需要退出，此时循环结束，程序退出。
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { 
        return; 
        }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void test_expr() {

  printf("test\n");
  char line[2048];
  char* e;
  FILE *fp = fopen("/home/liaozhanlong/Documents/ysyx-workbench/nemu/tools/gen-expr/input", "r");
  assert(fp);
  word_t correct_res, res;
  bool success = true;
  if (fp == NULL){
    perror("Fail to open the file");
    assert(fp);
  }
  printf("test\n");
  while(fgets(line, sizeof(line),fp)){
    e = strtok(line, " ");
    if (e != NULL){
      correct_res = strtoul(e, NULL, 10);
      e = strtok(NULL, "\n");
      if (e != NULL){
        res = expr(e, &success);
        puts(e);
        printf("expected: %d, got: %d`\n", correct_res, res);
        if (correct_res == res){
          printf("equal\n");
        }else{
          printf("inqual\n");
        }
      }
    }
  }
}
void sdb_watchpoint_display(){
  bool flag = true;
  word_t tmp =  0;
  for(int i = 0 ; i < NR_WP ; i ++){
	  if(wp_pool[i].flag){
      tmp = isa_reg_str2val(wp_pool[i].expr,&flag);
      if(wp_pool[i].new_value != tmp){
        wp_pool[i].old_value = wp_pool[i].new_value;
        wp_pool[i].new_value = tmp;
      }
      printf("Watchpoint.No: %d, expr = \"%s\", old_value = %d, new_value = %d\n", 
      wp_pool[i].NO, wp_pool[i].expr,wp_pool[i].old_value, wp_pool[i].new_value);
      flag = false;
      tmp = 0;
	  }
    }
  if(flag) printf("No watchpoint now.\n");
}
void delete_watchpoint(int no){
    for(int i = 0 ; i < NR_WP ; i ++)
	if(wp_pool[i].NO == no){
	    free_wp(&wp_pool[i]);
	    return ;
	}
}
void create_watchpoint(char* args){
  WP* p =  new_wp();
  strcpy(p -> expr, args);
  bool success = false;
  int tmp = expr(p -> expr,&success);
  if(success) p -> old_value = tmp;
  else {
    printf("Creating watchpoint failed, when running expr\n");
    delete_watchpoint(p -> NO);
  }
  printf("Create watchpoint No.%d success.\n", p -> NO);
}
void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
