#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <string.h>
#include <sys/stat.h>

#define BOLD "\001\e[1;" //         \001\e[0;34m\002//       
#define REGULAR "\001\e[0;"
#define RED   "31m\002"
#define BLU   "34m\002"
#define GRN   "32m\002"
#define YEL   "33m\002"
#define CYN   "36m\002"
#define MAG   "35m\002"
#define WHT   "37m\002"
#define BLK   "30m\002"
#define RESET "0m\002"


typedef struct{
	int toggle;
	char* bold;
	char* color;
}Settings;

struct process{
int pid;
struct process* next;
};

typedef struct process process;

struct job{
	pid_t pid;
	int jid;
	char* status;
	char* command;
	pid_t gpid;
	struct job* next;
};

typedef struct job job;



int handle_builtin(char** new_argv,int new_argc,Settings* user, Settings* machine,int* redir_array);
void handle_cd(char** new_argv);
void change_dir(char* dir);
void init_structs(Settings* user, Settings* machine);
void handle_pwd();
void handle_prt();
void handle_chpmt(Settings* user, Settings* machine, char* setting, int toggle);
char* print_chpmt(Settings* user, Settings* machine, char* fullbuffer,char* user_str, char* machine_str);
void init_user_machine(char* user, char* machine);
int handle_chclr(Settings* user,Settings* machine,char* setting_arg,char* color_arg,int color);
char* relative_cwd();
int check_file(char *filename);
void execute(char** new_argv, int new_argc,int* redir_array, int background,char* cmd);
void print_help();
void handle_output(char** new_argv,int new_argc,int* redir_array,void (*print)());
void find_redirect_output(char** new_argv, int* array);
void handle_redirect_output(char** new_argv,int new_argc,int* redir_array);
char** init_arg_array(char* cmd,int new_argc);
void handle_piping(char** new_argv, char* cmd,int new_argc,int bg);
int count_pipe(char** new_argv, int new_argc);
void handler(int sig);
void add_job(job* temp);
void remove_job(job* temp);
void kill_handler(char** new_argv, int new_argc);
void print_jobs();
void disown_handler(char* pid,char* jid);
job* find_job_by_jid(int jid);
job* find_job_by_pid(int pid);
int find_pid_with_jid(int jid);
void fg(int pid , int jid);
void bg(int pid, int jid);
void add_process(job* current, process* temp);
void remove_process(job* current, process* temp);
process* find_process_by_pid(job* current, int pid);
void int_handler(int sig);
void suspend_handler(int sig);
int print_help_rl(int count, int key);
int store_pid_rl (int count, int key);
int get_pid_rl (int count, int key);
void int_to_string(int number, char buf[]);
int sf_info_rl(int count, int key);
int print_help_rl(int count, int key);
int check_symbol(char* cmd);
int parseargs(char** new_argv,char* cmd);
void exit_handler();
