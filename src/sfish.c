#include "sfish.h"
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define SUCCESS 0
#define FAILURE 1
#define MAXARGS 1024
#define STDERR 2
#define STDOUT 1
#define STDIN 0


int prev_return;
job* head;
volatile pid_t fg_pid=0;
int current_jid=0;
int spid=-1;
int command_count=0;
job* fg_job= NULL;



int main(int argc, char** argv) {
    //DO NOT MODIFY THIS. If you do you will get a ZERO.
    //This is disable readline's default signal handlers, since you are going
     //to install your own.
    rl_catch_signals = 0;

    char** new_argv;  // SET UP ARGV
    new_argv=malloc(MAXARGS);
    memset(new_argv,0,MAXARGS);
    //ok

    int new_argc=0;

   Settings* user= malloc(sizeof(Settings));
   Settings* machine= malloc(sizeof(Settings));
   char* fullbuffer= malloc(200);
   char* user_str= malloc(100);
   char* machine_str=malloc(100);

   memset(fullbuffer,0,200);
   memset(user_str,0,100);
   memset(machine_str,0,100);

   init_user_machine(user_str,machine_str); // save user and machine environments
   init_structs(user,machine); // init the structs
   char *cmd;

   signal(SIGTSTP, suspend_handler);
   signal(SIGCHLD, handler);
   signal(SIGINT, int_handler);

        rl_command_func_t store_pid_rl;
        rl_command_func_t get_pid_rl;
        rl_command_func_t sf_info_rl;
        rl_command_func_t print_help_rl;
        rl_bind_keyseq ("\\C-h", print_help_rl);
        rl_bind_keyseq ("\\C-b", store_pid_rl);
        rl_bind_keyseq ("\\C-g", get_pid_rl);
        rl_bind_keyseq("\\C-p",sf_info_rl);



    while((cmd = readline(print_chpmt(user,machine, fullbuffer,user_str,machine_str))) != NULL){
    
        int background=0;
        new_argc= parseargs(new_argv,cmd);
        
        int* redirection_array= malloc(new_argc*sizeof(int));
        memset(redirection_array,-1,new_argc*sizeof(int));
        if(new_argv[0]!=NULL){
        find_redirect_output(new_argv,redirection_array);

         if(strcmp(new_argv[new_argc-1],"&")==0){
            background=1;
            new_argv[new_argc-1]=NULL;
            new_argc--;
        }

        if (strcmp(new_argv[0],"exit")==0){
            exit_handler();
            break;
        }
        else if(strcmp(new_argv[0],"kill")==0){
            command_count++;
            kill_handler(new_argv,new_argc);
        }
        else if(strcmp(new_argv[0],"jobs")==0){
            command_count++;
            handle_output(new_argv,new_argc,redirection_array,print_jobs);
            //print_jobs();
        }
        else if(strcmp(new_argv[0],"disown")==0){
            command_count++;

            if(new_argv[1]==NULL){
                disown_handler(NULL,NULL);
            }
            else{
            if(*(new_argv[1])=='%'){
                char* jid= new_argv[1];
                 disown_handler(NULL,(jid+1));
            }
            else{
                char* pid= new_argv[1];
                disown_handler(pid,NULL);
            }
        }
        }
        else if(strcmp(new_argv[0],"fg")==0){
            command_count++;
            if(*(new_argv[1])=='%'){
                char* jid_char= new_argv[1];
                int jid= atoi(jid_char+1);
                fg(-1,jid);
            }
            else{
                int pid= atoi(new_argv[1]);
                fg(pid,-1);
            }
        }
        else if(strcmp(new_argv[0],"bg")==0){
            command_count++;
            if(*(new_argv[1])=='%'){
                char* jid_char= new_argv[1];
                int jid= atoi(jid_char+1);
                bg(-1,jid);
            }
            else{
                int pid= atoi(new_argv[1]);
                bg(pid,-1);
            }
        }
        else{
        int ret= handle_builtin(new_argv,new_argc,user,machine,redirection_array);
        if(count_pipe(new_argv,new_argc)>0){
            handle_piping(new_argv,cmd,new_argc,background);
        }
        else if(ret==0){
           execute(new_argv,new_argc,redirection_array,background,cmd);
        }
        }
    }
    //     // if(ret==0){
    //     //     execute(new_argv,new_argc,redirection_array);
    //     // }


    //    // printf("%s\n",cmd);

    //     //All your debug print statments should be surrounded by this #ifdef
    //     //block. Use the debug target in the makefile to run with these enabled.
    //     // #ifdef DEBUG
    //     // fprintf(stderr, "Length of command entered: %ld\n", strlen(cmd));
    //     // #endif

    //     //You WILL lose points if your shell prints out garbage values.
        memset(new_argv,0,MAXARGS);
        free(redirection_array);
        free(cmd);
    }

    //Don't forget to free allocated memory, and close file descriptors.
    free(fullbuffer);
    free(user);
    free(machine);
    free(user_str);
    free(machine_str);
    free(new_argv);
    //WE WILL CHECK VALGRIND!

    return EXIT_SUCCESS;
}


int handle_builtin(char** new_argv,int new_argc, Settings* user, Settings* machine, int* redir_array){

    if (strcmp(new_argv[0],"help")==0){
        command_count++;
        handle_output(new_argv,new_argc,redir_array,print_help);
            return 1;
        }

    else if(strcmp(new_argv[0],"cd")==0){
        command_count++;
        handle_cd(new_argv);
        return 1;
    }
    else if(strcmp(new_argv[0],"pwd")==0){
        command_count++;
        handle_output(new_argv,new_argc,redir_array,handle_pwd);
        //handle_pwd();
        return 1;
    }
    else if(strcmp(new_argv[0],"prt")==0){
        command_count++;
        handle_output(new_argv,new_argc,redir_array,handle_prt);
        return 1;
    }
    else if(strcmp(new_argv[0],"chpmt")==0){
        command_count++;
        char* setting_arg=new_argv[1];
        char* toggle_arg= new_argv[2];
        if(setting_arg==NULL || toggle_arg==NULL){
            prev_return=FAILURE;
            return -1; // THERE IS AN ERROR
        }
        else{
        int toggle= *(toggle_arg)- '0';
        handle_chpmt(user,machine,setting_arg,toggle);
        return 1;
        }
    }
    else if(strcmp(new_argv[0],"chclr")==0){
        command_count++;
        if(new_argc==4){
        char* setting_arg=new_argv[1];
        char* color_arg= new_argv[2];
        char* bold_toggle= new_argv[3];
        int bold= *(bold_toggle)- '0';
        int ret= handle_chclr(user,machine,setting_arg,color_arg,bold);

        if(ret<0){
            char* msg= "chclr: Command format SETTINGS[user/machine] COLOR BOLD[0/1]\n";
            write(2,msg,strlen(msg));
            prev_return=FAILURE;
            return -1;
        }
        else{
            return 1;
        }
        }
        else{
            return 0;
        }
    }
    else{
        return 0;
    }

}

void handle_pwd(){
        char buffer[100];
        char* current_dir=getcwd(buffer,100);
        write(STDOUT,current_dir,strlen(current_dir));
        write(STDOUT,"\n",1);
       // fprintf(stderr,"%s\n",current_dir);
        prev_return=SUCCESS;
}

void handle_cd(char** new_argv){
if(new_argv[1]==NULL){
    const char *home = "HOME";
    char *home_dir;
    home_dir = getenv(home);

    change_dir(home_dir);
}
else{
    change_dir(new_argv[1]);
    }
}

void change_dir(char* dir){
    int val= chdir(dir);
    if(val==-1){
        write(STDERR,"sfish: ",7);
        write(STDERR,"cd: ",4);
        write(STDERR,dir,strlen(dir));
        write(STDERR,": No such file or directory \n",30);
        //fprintf(stderr,"cd: %s: No such file or directory \n", dir);
        prev_return=FAILURE;
    }
    else{
        prev_return=SUCCESS;
    }
}

void handle_prt(){
    char* ret= calloc(10,1);
    int_to_string(prev_return,ret);
    write(STDOUT,ret,strlen(ret));
    write(STDOUT,"\n",1);
    free(ret);
    //fprintf(stderr,"%d\n",prev_return);
}

void handle_chpmt(Settings* user, Settings* machine, char* setting, int toggle){
if(strcmp(setting,"user")==0){
    if(toggle==1 || toggle==0){
        user->toggle=toggle;
        prev_return=SUCCESS;
    }
    else{
        prev_return=FAILURE;
        char* msg= "chpmt: Toggle value must be either 0 or 1. Command format SETTINGS[user/machine] TOGGLE[0/1]\n";
        write(STDERR,msg,strlen(msg));
        // fprintf(stderr, "Toggle value must be either 0 or 1. Command format SETTINGS[user/machine] TOGGLE[0/1]\n");
    }
}

else if(strcmp(setting,"machine")==0){
    if(toggle==1 || toggle==0){
        machine->toggle=toggle;
        prev_return=SUCCESS;
    }
    else{
        prev_return=FAILURE;
        char* msg= "chpmt: Toggle value must be either 0 or 1. Command format SETTINGS[user/machine] TOGGLE[0/1]\n";
        write(STDERR,msg,strlen(msg));
        //fprintf(stderr, "Toggle value must be either 0 or 1. Command format SETTINGS[user/machine] TOGGLE[0/1]\n");
    }
}
else{
    prev_return=FAILURE;
    char* msg= "chpmt: Toggle value must be either 0 or 1. Command format SETTINGS[user/machine] TOGGLE[0/1]\n";
     write(STDERR,msg,strlen(msg));
    //fprintf(stderr, "Setting must be either 'user' or 'machine'. Command format SETTINGS[user/machine] TOGGLE[0/1]\n");
}
}

char* print_chpmt(Settings* user, Settings* machine, char* fullbuffer,char* user_str, char* machine_str){

    char* relativepath= malloc(100);
    memset(relativepath,0,100);
    if(user->toggle==1 && machine->toggle==1){
        memset(fullbuffer,0,200);
        strcat(fullbuffer,"sfish-");
        strcat(fullbuffer,user->bold);
        strcat(fullbuffer,user->color);
        strcat(fullbuffer,user_str);
        strcat(fullbuffer,REGULAR);
        strcat(fullbuffer,RESET);
        strcat(fullbuffer,"@");
        strcat(fullbuffer,machine->bold);
        strcat(fullbuffer,machine->color);
        strcat(fullbuffer,machine_str);
        strcat(fullbuffer,REGULAR);
        strcat(fullbuffer,RESET);
        strcat(fullbuffer,":[");
        strcat(fullbuffer,relative_cwd(relativepath));
        strcat(fullbuffer, "]>");
        strcat(fullbuffer,REGULAR);
        strcat(fullbuffer,RESET);
        free(relativepath);
        return fullbuffer;
    }
    else if(user->toggle==1){
       memset(fullbuffer,0,200);
        strcat(fullbuffer,"sfish-");
        strcat(fullbuffer,user->bold);
        strcat(fullbuffer,user->color);
        strcat(fullbuffer,user_str);
        strcat(fullbuffer,REGULAR);
        strcat(fullbuffer,RESET);
         strcat(fullbuffer,":[");
        strcat(fullbuffer,relative_cwd(relativepath));
        strcat(fullbuffer, "]>");
        free(relativepath);
        return fullbuffer;
    }
    else if(machine->toggle==1){
        memset(fullbuffer,0,200);
        strcat(fullbuffer,"sfish-");
        strcat(fullbuffer,machine->bold);
        strcat(fullbuffer,machine->color);
        strcat(fullbuffer,machine_str);
        strcat(fullbuffer,REGULAR);
        strcat(fullbuffer,RESET);
        strcat(fullbuffer,":[");
        strcat(fullbuffer,relative_cwd(relativepath));
        strcat(fullbuffer, "]>");
        free(relativepath);
        return fullbuffer;
    }
    else{
        memset(fullbuffer,0,200);
        strcat(fullbuffer,"sfish");
        strcat(fullbuffer,":[");
        strcat(fullbuffer,relative_cwd(relativepath));
        strcat(fullbuffer, "]>");
        free(relativepath);
        return fullbuffer;
    }
}

void init_user_machine(char* user, char* machine){

    const char *user_env = "USER";
    char* temp = getenv(user_env);
    strcpy(user,temp);
    gethostname(machine,100);
}
int handle_chclr(Settings* user,Settings* machine,char* setting_arg,char* color_arg, int bold){
   char* color_code;
   if(strcmp(color_arg,"red")==0){
    color_code=RED;
   }
   else if(strcmp(color_arg,"blue")==0){
    color_code=BLU;
   }
   else if(strcmp(color_arg,"green")==0){
    color_code=GRN;
   }
   else if(strcmp(color_arg,"yellow")==0){
    color_code=YEL;
   }
   else if(strcmp(color_arg,"cyan")==0){
    color_code=CYN;
   }
   else if(strcmp(color_arg,"magenta")==0){
    color_code=MAG;
   }
   else if(strcmp(color_arg,"black")==0){
    color_code=BLK;
   }
   else if(strcmp(color_arg,"white")==0){
    color_code=WHT;
   }
   else{
    color_code=RESET;
    return -1;
   }

   if(strcmp(setting_arg,"user")==0){
        if(bold==1){
        user->color=color_code;
         user->bold=BOLD;
        }
        else if(bold==0){
            user->color=color_code;
            user->bold=REGULAR;
        }
        else{
            return -1;
        }
        return 0;
        //prev_return=SUCCESS;
   }
   else if(strcmp(setting_arg,"machine")==0){
    
    if(bold==1){ 
        machine->color=color_code;
        machine->bold=BOLD;
    }
    else if(bold==0){
        machine->color=color_code;
        machine->bold=REGULAR;
    }
    else{
        return -1;
    }
        return 0;
        //prev_return=SUCCESS;
   }
   else{
        return -1;
        //prev_return=FAILURE;
   }


}

void init_structs(Settings* user, Settings* machine){
    user->toggle=1; // INIT VALUES OF THE STRUCTS
    machine->toggle=1;
    user->color=RESET;
    machine->color=RESET;
    user->bold=REGULAR;
    machine->bold=REGULAR;
}
char* relative_cwd(char* relativepath){

    char buffer[100];
    memset(buffer,0,100);
    char* current_dir=getcwd(buffer,100);

    const char *home = "HOME";
    char *home_dir;
    home_dir = getenv(home); 
    size_t home_len = strlen(home_dir);

    if(strcmp(current_dir,home_dir)==0 || home_len>(strlen(current_dir))){
        strcpy(relativepath,"~");
    }
    else{
        strcpy(relativepath,(current_dir+home_len));
    }

    return relativepath;
}

// int parseargs(char** new_argv,char* cmd){

// const char delimiters[] = " ";
// char *temp=strdup(cmd);
// int count=0;
// char* temp1;
//     while((temp1= strsep(&temp, delimiters))!=NULL){
//         if(strcmp(temp1,"")!=0){
//         char* arg= malloc(sizeof(temp1));
//         arg=temp1;
//         new_argv[count]=arg;
//         count++;
//         }
//     }
// new_argv[++count]=NULL;
// free(temp);
// return count-1;
// }

void execute(char** new_argv, int new_argc,int* redir_array,int background,char* cmd){ 
    pid_t pid;
    command_count++;

    if ((pid = fork()) == 0) { /* Child runs user job */
        signal(SIGTSTP,SIG_IGN);
        signal(SIGINT,SIG_IGN);
        if(strchr(new_argv[0],'/') && check_file(new_argv[0]) ){ // run the file directly
                handle_redirect_output(new_argv,new_argc,redir_array);
                execvp(new_argv[0],new_argv);
        }
        else{ // look for the executable in the PATH environment
            char *path;
            path = getenv("PATH"); 
            const char delimiters[] = ":";
            char *temp=strdup(path);
            char* exec_file;
    while((exec_file= strsep(&temp, delimiters))!=NULL){

        char* exec_path=malloc(200*sizeof(char));
        memset(exec_path,0,200*sizeof(char));
        strcat(exec_path,exec_file);
        strcat(exec_path,"/");
        strcat(exec_path,new_argv[0]);
        if (check_file(exec_path)){ // check to see if the file exists
                handle_redirect_output(new_argv,new_argc,redir_array);
                execvp(exec_path,new_argv);
                break;
            }
            free(exec_path);
        }

        write(STDERR,new_argv[0],strlen(new_argv[0]));
        write(STDERR,": command not found.\n",21);
        exit(EXIT_FAILURE);
    }
}
    else if(pid>0 && background==0){ // if its in foreground just wait
        fg_pid=pid;
        job* new_job = malloc(sizeof(job));
        new_job->status="Running";
        new_job->gpid=0;
        new_job->command= strdup(cmd);
        new_job->pid=pid;
        fg_job=new_job;
       //add_job(new_job);

        int status;
       pid = waitpid(pid, &status, WUNTRACED | WCONTINUED);

        if(WIFEXITED(status)){
            free(new_job);
            fg_job=NULL;
            prev_return=WEXITSTATUS(status);
             //remove_job(new_job);
        }
       //fg_pid=0;
        }
        else{// job is in the background so don't wait
        job* new_job = malloc(sizeof(job));
        new_job->status="Running";
        new_job->gpid=0;
        new_job->command= strdup(cmd);
        new_job->pid=pid;
        add_job(new_job);

        write(STDOUT,"[",1);
        char* jid= calloc(100,1);
        int_to_string(new_job->jid,jid);

        char* pid = calloc(100,1);
        int_to_string(new_job->pid,pid);

        write(STDOUT,jid,strlen(jid));
        write(STDOUT,"]\t",2);
        write(STDOUT,pid,strlen(pid));
        write(STDOUT,"\n",1);

        free(jid);
        free(pid);
        }
}


int check_file(char *filename){
  struct stat buf;   
  return (stat(filename, &buf) == 0);
} 
void print_help(){

    write(STDOUT,"help\n",5);
    write(STDOUT,"exit\n",5);
    write(STDOUT,"cd [- | directory]\n",19);
    write(STDOUT,"pwd\n",4);
    write(STDOUT,"prt\n",4);
    write(STDOUT,"chpmt SETTING[user/machine] TOGGLE[0/1]\n",40);
    write(STDOUT,"chclr SETTING[user/machine] COLOR BOLD[0/1]\n",44);
}

void handle_output(char** new_argv,int new_argc,int* redir_array,void (*print)()){
    pid_t pid_help; 
    if ((pid_help = fork()) == 0) {
        handle_redirect_output(new_argv,new_argc,redir_array);
        print();
             exit(EXIT_SUCCESS);
    }    
    else {
        int status;
        waitpid(pid_help,&status,0);
        prev_return=SUCCESS;
    }

}
void find_redirect_output(char** new_argv, int* array){

int i=0;
while(new_argv[i]!=NULL){
    if(strcmp(new_argv[i],"<")==0){
        array[i]=STDIN;
    }
    else if(strcmp(new_argv[i],">")==0){
        array[i]=STDOUT;
    }
    else if(strncmp(new_argv[i],"2>",2)==0){
        array[i]=STDERR;
    }
    i++;
}
}

void handle_redirect_output(char** new_argv, int new_argc,int* redir_array){

                for(int i=0; i<new_argc;i++){
                    if(redir_array[i]==STDOUT){
                        char* file_name = new_argv[i+1];
                        int fd;
                       if((fd=open(file_name,O_RDWR| O_CREAT| O_APPEND, S_IRUSR | S_IWUSR| S_IXUSR))!=-1){
                            dup2(fd,STDOUT);
                            close(fd);
                       }
                       new_argv[i]=NULL;
                       new_argv[i+1]=NULL;

                    }
                    else if(redir_array[i]==STDIN){
                        char* file_name = new_argv[i+1];
                        int fd;
                       if((fd=open(file_name,O_RDONLY, S_IRUSR))!=-1){
                            dup2(fd,STDIN);
                            close(fd);
                       }
                        new_argv[i]=NULL;
                       new_argv[i+1]=NULL;
                    }
                    else if(redir_array[i]==STDERR){
                        char* file_name;
                        if(new_argv[i+1]==NULL){
                            file_name= new_argv[i];
                            file_name+=2;
                            fprintf(stdout,"file name: %s\n",file_name);
                        }
                        else{
                            file_name = new_argv[i+1];
                        }
                        int fd;
                       if((fd=open(file_name,O_RDWR| O_CREAT| O_APPEND, S_IRUSR | S_IWUSR| S_IXUSR))!=-1){
                            dup2(fd,STDERR);
                            close(fd);
                       }
                        new_argv[i]=NULL;
                       new_argv[i+1]=NULL;
                    }
                }
}
void handle_piping(char** new_argv, char* cmd, int new_argc,int bg){ // ls | grep '.txt' | wc -l
    pid_t parent_pid;
    command_count++;
    job* new_job= malloc(sizeof(job));
    // sigset_t mask_all, mask_child, prev_one;
    // sigfillset(&mask_all);
    // sigemptyset(&mask_child);
    // sigaddset(&mask_child, SIGCHLD);

if ((parent_pid = fork()) == 0) {
   signal(SIGTSTP,SIG_IGN);
   signal(SIGINT,SIG_IGN);
   setpgid(0, 0);
   int pipe_count= count_pipe(new_argv,new_argc); // count the number of pipes.
   int pipefd[2*pipe_count]; // set up the pipes.
   int arg_count=0;
   int j=0;
   pid_t pid;

   char** temp_argv= init_arg_array(cmd,new_argc);

   for(int i = 0; i < 2*pipe_count; i++){
        pipe(pipefd+(i*2));
   }

    while(temp_argv[arg_count]!=NULL){
        if ((pid = fork()) == 0) {
            if(arg_count!=0){
                dup2(pipefd[j-2], 0);
            }

            if(temp_argv[arg_count+1]!=NULL){ // not the last command
                dup2(pipefd[j + 1], 1);
            }

            for(int i = 0; i < 2*pipe_count; i++){
                    close(pipefd[i]);
            }

            char* command_argv[MAXARGS]={0};
            int command_argc;
            command_argc=parseargs(command_argv,temp_argv[arg_count]);

             if(*(command_argv[command_argc-1])=='&'){
                command_argv[command_argc-1]=NULL;
            }

            int* redirection_array= malloc(command_argc*sizeof(int));
            memset(redirection_array,-1,command_argc*sizeof(int));
            find_redirect_output(command_argv,redirection_array);
            handle_redirect_output(command_argv, command_argc,redirection_array);
            free(redirection_array);

            execvp(command_argv[0], command_argv);

            write(STDERR,command_argv[0],strlen(command_argv[0]));
            write(STDERR,": command not found.\n",21);
            exit(EXIT_FAILURE);

        }
        else if(pid>0){
            j+=2;
            arg_count++;
        }
    }
    for(int i = 0; i < 2 *pipe_count; i++){
                close(pipefd[i]);
            }
            int status;
             // while((pid = waitpid(pid, &status, WUNTRACED | WCONTINUED))>0);
            pid = waitpid(pid, &status, WUNTRACED | WCONTINUED);
            exit(EXIT_SUCCESS);
}
    else if(parent_pid>0 && bg==0){
        setpgid(parent_pid, parent_pid);
        //fg_pid=getpgid(parent_pid);
        new_job->jid=1;
        new_job->gpid=getpgid(parent_pid);
        new_job->status="Running";
        new_job->command=strdup(cmd);
        new_job->pid=parent_pid;
        // add_job(new_job);
        fg_job=new_job;
    
        int status1;
        parent_pid = waitpid(parent_pid, &status1, WUNTRACED | WCONTINUED);
      
        if(WIFEXITED(status1)){
            free(new_job);
            fg_job=NULL;
            prev_return=WEXITSTATUS(status1);
             //remove_job(new_job);
        }
    }
    else if(parent_pid>0 && bg==1){
        setpgid(parent_pid, parent_pid);
        new_job->jid=1;
        new_job->gpid=getpgid(parent_pid);
        new_job->status="Running";
        new_job->command=strdup(cmd);
        new_job->pid=parent_pid;
        add_job(new_job); 

        write(STDOUT,"[",1);
        char* jid= calloc(100,1);
        int_to_string(new_job->jid,jid);

        char* pid = calloc(100,1);
        int_to_string(new_job->pid,pid);

        write(STDOUT,jid,strlen(jid));
        write(STDOUT,"]\t",2);
        write(STDOUT,pid,strlen(pid));
        write(STDOUT,"\n",1);

        free(jid);
        free(pid);
    }
}

int count_pipe(char** new_argv, int new_argc){
    int count=0;
    for(int i=0; i<new_argc; i++){
        if(strcmp(new_argv[i],"|")==0){
            count++;
        }
    }
    return count;
    
}

char** init_arg_array(char* cmd,int new_argc){
   char** arg_array= malloc(new_argc*sizeof(char*));
   memset(arg_array,0,new_argc*sizeof(char*));
   char* argument=calloc(1024,sizeof(char));
   const char delimiters[] = "|";
   int i=0;
    while((argument=strsep(&cmd,delimiters))!=NULL){
        arg_array[i]= argument;
        i++;
    }
    return arg_array;
}

void handler(int sig){
pid_t pid;
int status;

sigset_t mask_all, prev_all;

sigfillset(&mask_all);

while((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED))>0){

sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
if(WIFEXITED(status) || WIFSIGNALED(status)){
    job* current;
    if((current=find_job_by_pid(pid))!=NULL){
        remove_job(current);
    }
    prev_return=SUCCESS;
}
else if(WIFSTOPPED(status)){
    job* current;
    if((current=find_job_by_pid(pid))!=NULL){
        current->status="Stopped";
    }
    prev_return=SUCCESS;
}
else if(WIFCONTINUED(status)){
    job* current;
    if((current=find_job_by_pid(pid))!=NULL){
        current->status="Running";
    }
    prev_return=SUCCESS;
}
sigprocmask(SIG_SETMASK, &prev_all, NULL);
}

}

void add_job(job* temp){
    current_jid++;
    temp->jid=current_jid;
    temp->next=head;
    head= temp;
}

void remove_job(job* temp){

if(temp!=NULL){
    if(temp==head){
    head= temp->next;
    temp->next=NULL;
    }
    else{
        job* temp_prev=head;
        while(temp_prev->next!=temp){
            temp_prev=temp_prev->next;
        }
        temp_prev->next=temp->next;
        temp->next=NULL;
    }
    free(temp->command);
    free(temp);
}
}
void kill_handler(char** new_argv, int new_argc){
    int signal;
    int pid;
    job* current;
    if(new_argc==3){ // this means SIGNAL is given: kill sig jid/pid
        if((signal=atoi(new_argv[1]))==0){
            write(STDERR,"Invalid signal.\n",16);
            prev_return=FAILURE;
            return;
        }
        char* temp= new_argv[2];
        if(*temp=='%'){
        int jid= atoi(temp+1);
        current= find_job_by_jid(jid);
            if(current==NULL){
                write(STDERR,"sfish: ",7);
                write(STDERR,"kill: ",6);
                write(STDERR,temp,strlen(temp));
                write(STDERR,": No such Job ID.\n",18);
                prev_return=FAILURE;
                return;
            }
        }
        else{
        int temp_pid= atoi(new_argv[2]);
            if((current=find_job_by_pid(temp_pid))==NULL){
                write(STDERR,"sfish: ",7);
                write(STDERR,"kill: ",6);
                write(STDERR,temp,strlen(temp));
                write(STDERR,": No such PID.\n",16);
                prev_return=FAILURE;
                return;
            }
        }
    }
    else if(new_argc==2){
        signal=SIGTERM;
        char* temp= new_argv[1];
        if(*temp=='%'){
        int jid= atoi(temp+1);
        current= find_job_by_jid(jid);
            if(current==NULL){
                write(STDERR,"sfish: ",7);
                write(STDERR,"kill: ",6);
                write(STDERR,temp,strlen(temp));
                write(STDERR,": No such Job ID.\n",18);
                prev_return=FAILURE;
                return;
            }

        }
        else{
        int temp_pid= atoi(new_argv[1]);

            if((current=find_job_by_pid(temp_pid))==NULL){
                write(STDERR,"sfish: ",7);
                write(STDERR,"kill: ",6);
                write(STDERR,temp,strlen(temp));
                write(STDERR,": No such PID.\n",16);
                prev_return=FAILURE;
                return;
            }
        }
    }
    if(current->gpid>0){
        pid= current->gpid;
        killpg(pid,signal);
          prev_return=SUCCESS;
    }
    else{
        pid= current->pid;
        kill(pid,signal);
        prev_return=SUCCESS;
    }
}

void print_jobs(){
    job* current= head;
    while((current!=NULL)){
        char* jid= calloc(100,1);
        int_to_string(current->jid,jid);

        char* pid= calloc(100,1);
        int_to_string(current->pid,pid);

        write(STDOUT,"[",1);
        write(STDOUT,jid,strlen(jid));
        write(STDOUT,"]",1);
        write(STDOUT,"\t",1);
        write(STDOUT,current->status,strlen(current->status));
        write(STDOUT,"\t",1);
        write(STDOUT,"\t",1);
        write(STDOUT,pid,strlen(pid));
        write(STDOUT,"\t",1);
        write(STDOUT,current->command,strlen(current->command));
        write(STDOUT,"\n",1);

        free(jid);
        free(pid);
         current= current->next;
    }
     prev_return=SUCCESS;
}

void disown_handler(char* pid,char* jid){
if(pid!=NULL){
int pid_disown= atoi(pid);
job* current;
    if((current=find_job_by_pid(pid_disown))!=NULL){
        remove_job(current);
         prev_return=SUCCESS;
    }
    else{
        write(STDOUT,"sfish: ",7);
        write(STDOUT,"kill: ",6);
        write(STDOUT,pid,strlen(pid));
        write(STDOUT,": No such PID.\n",16);
         prev_return=FAILURE;
    }
}
else if(jid!=NULL){
    int jid_disown= atoi(jid);
    job* current;
    if((current=find_job_by_jid(jid_disown))!=NULL){
        remove_job(current);
         prev_return=SUCCESS;
    }
    else{
        write(STDOUT,"sfish: ",7);
        write(STDOUT,"kill: ",6);
        write(STDOUT,"%%",1);
        write(STDOUT,jid,strlen(jid));
        write(STDOUT,": No such Job ID.\n",18);
         prev_return=FAILURE;
    }
}
else{
    while(head!=NULL){
        remove_job(head);

    }
     prev_return=SUCCESS;
}
}

job* find_job_by_jid(int jid){
    job* current=head;
    while(current!=NULL){
        if(current->jid==jid){
            return current;
        }
        else{
            current=current->next;
        }
    }
    return NULL;
}
job* find_job_by_pid(int pid){
    job* current=head;
    while(current!=NULL){
       if(current->pid==pid){
            return current;
       } 
        else{
            current=current->next;
        }
    }
    return NULL;

}

int find_pid_with_jid(int jid){
    job* temp = find_job_by_jid(jid);

    if(temp==NULL){
        return -1;
    }
    else{
        return temp->pid;
    }
}

void fg(int pid , int jid){
    job* current;
    if(pid!=-1){
        current= find_job_by_pid(pid);
        if(current==NULL){
        write(STDOUT,"sfish: ",7);
        write(STDOUT,"fg: ",6);
        write(STDOUT,"No such PID.\n",13);
        }
    }
    else if(jid!=-1){
        current=find_job_by_jid(jid);
        pid=current->pid;
        if(current==NULL){
        write(STDOUT,"sfish: ",7);
        write(STDOUT,"fg: ",6);
        write(STDOUT,"No such JID.\n",13);
        }
    }
    
    if(current!=NULL){
        fg_job=current;
        if(strcmp(current->status,"Stopped")==0){

            if(current->gpid>0){
              killpg(pid,SIGCONT);
              prev_return=SUCCESS;
            }
            else{
               kill(pid,SIGCONT);
               prev_return=SUCCESS;
            }
        }
        int status;
        waitpid(pid,&status,WUNTRACED);
        if(WIFEXITED(status)){
            prev_return=WEXITSTATUS(status);
        }
    }

}

void bg(int pid, int jid){
job* current;
    if(pid!=-1){
        current= find_job_by_pid(pid);

        if(current==NULL){
        write(STDOUT,"sfish: ",7);
        write(STDOUT,"bg: ",4);
        write(STDOUT,"No such PID.\n",13);
        }
    }
    else if(jid!=-1){
        current=find_job_by_jid(jid);

        if(current==NULL){
        write(STDOUT,"sfish: ",7);
        write(STDOUT,"bg: ",4);
        write(STDOUT,"No such JID.\n",13);
        }
        else{
             pid= current->pid;
        }
    }
    
    if(current!=NULL){

        if(current->gpid>0){
            current->status="Running";
            killpg(pid,SIGCONT);
            prev_return=SUCCESS;
        }
        else{
            current->status="Running";
            kill(pid,SIGCONT);
            prev_return=SUCCESS;
        }
    }
}

// void child_handler(int sig){

// }


void int_handler(int sig){
    job* current;
    if(fg_job!=NULL){
        if(fg_job->gpid>0){
            killpg(fg_job->pid,SIGTERM);
        }
        else{
            kill(fg_job->pid,SIGTERM);
        }
        if((current=find_job_by_pid(fg_job->pid))!=NULL){
            remove_job(current);
        }
        else{
            free(fg_job);
        }
        fg_job=NULL;
    }
}

void suspend_handler(int signum){

    if(fg_job!=NULL){
        if(fg_job->gpid>0){
            if(find_job_by_pid(fg_job->pid)==NULL){
                 add_job(fg_job);
            }
            fg_job->status="Stopped";
            killpg(fg_job->pid,SIGSTOP);
        }
        else{
            if(find_job_by_pid(fg_job->pid)==NULL){
                 add_job(fg_job);
            }
            fg_job->status="Stopped";
            kill(fg_job->pid,SIGSTOP);
        }
        fg_job=NULL;

    }



    //    if(fg_pid>0){
    //     job* current;
    //     if((current=find_job_by_pid(fg_pid))!=NULL){
    //         if(current->gpid>0){
    //              killpg(fg_pid,SIGSTOP);
    //         }
    //         else{
    //             kill(fg_pid,SIGSTOP);
    //         }
    //     remove_job(current);
    //     fg_pid=0;
    //     }
    // }
}

int print_help_rl (int count, int key) {
   print_help();
   rl_on_new_line ();
   return 0;
}
int store_pid_rl (int count, int key){

    if(head==NULL){
        spid=-1;
    }
    else{
    job* current=head;
    while(current->next!=NULL){
        current=current->next;
    }   

    if(current!=NULL){
        spid=current->pid;
    }
    }
    write(STDOUT,"\n",1);
    rl_on_new_line ();
    return 0;
}

int get_pid_rl (int count, int key){
    job* current;
    if(spid!=-1 && (current=find_job_by_pid(spid))!=NULL){

        char* jid= calloc(100,1);
        int_to_string(current->jid,jid);

        char* pid= calloc(100,1);
        int_to_string(current->pid,pid);
        write(STDOUT,"\n",1);
        write(STDOUT,"[",1);
        write(STDOUT,jid,strlen(jid));
        write(STDOUT,"]",1);
        write(STDOUT,"  ",2);
        write(STDOUT,pid,strlen(pid));
        write(STDOUT," stopped by signal 15\n",22);

        kill(spid,SIGTERM);

        free(jid);
        free(pid);

        spid=-1;

    }
    else{
        write(STDOUT,"SPID does not exist and has been set to -1\n",44);
        spid=-1;
    }
    rl_on_new_line ();
    return 0;
}
int sf_info_rl(int count, int key){
    write(STDOUT,"\n----Info----\n",14);
    write(STDOUT,"help\n",5);
    write(STDOUT,"prt\n",4);
    write(STDOUT,"----CTRL----\n",13);
    write(STDOUT,"cd\n",3);
    write(STDOUT,"chclr\n",6);
    write(STDOUT,"chpmt\n",6);
    write(STDOUT,"pwd\n",4);
    write(STDOUT,"exit\n",5);
    write(STDOUT,"----Job Control----\n",20);
    write(STDOUT,"bg\n",3);
    write(STDOUT,"fg\n",3);
    write(STDOUT,"disown\n",7);
    write(STDOUT,"job\n",4);
    write(STDOUT,"---Number of Commands Run----\n",30);

    char* cmds= calloc(100,1); // REMEMBER TO FREE THIS
    int_to_string(command_count,cmds);

    write(STDOUT,cmds,strlen(cmds));
    
    write(STDOUT,"\n---Process Table----\n",22);
    write(STDOUT,"PGID\tPID\tTIME\tCMD\n",18);
    free(cmds);
    job* current= head;

    while(current!=NULL){
        char* pid= calloc(100,1);
        int_to_string(current->pid,pid);

        write(STDOUT,pid,strlen(pid));
        write(STDOUT,"\t",1);
        write(STDOUT,pid,strlen(pid));
        write(STDOUT,"\t",1);
        write(STDOUT,"0:00",4);
        write(STDOUT,"\t",1);
        write(STDOUT,current->command,strlen(current->command));
        write(STDOUT,"\n",1);
        free(pid);
        current=current->next;
    }
    write(STDOUT,"\n",1);
    rl_on_new_line ();
    return 0;
}

void int_to_string(int number, char* buf){ // p = buffer, s= buf
    if(number==0){
        *buf='0';
        buf++;
        *buf='\0';
    }
    else{
    char* buffer= buf;
    unsigned num = number;

    while(num){
        *buffer= ((num % 10)+'0');
        num/=10;
        buffer++;
    }

    while (buf < --buffer)
    {
        char x = *buf;
        *buf++ = *buffer;
        *buffer = x;
    }
}
}

int parseargs(char** new_argv,char* cmd){

char* command = strdup(cmd);
int cmd_size = strlen(command);

char* temp= calloc(100,1);

int count=0;

while(*command!='\0'){

int delim=0;
if((delim=check_symbol(command))==0){
    if(*command==' ' && temp[0]!='\0'){
        char* arg= strdup(temp);
        new_argv[count++]=arg;
        command++;
        memset(temp,0,strlen(temp));
    }
    else if(*command==' ' && temp[0]=='\0'){
        command++;
    }
    else{
        strncat(temp,command,1);
        command++;
    }
}
else{
    if(temp[0]!='\0'){
        char* arg= strdup(temp);
        new_argv[count++]=arg;
        memset(temp,0,strlen(temp));
    }
    while(delim--){
        strncat(temp,command,1);
        command++;
    }
    char* args= strdup(temp);
    new_argv[count++]=args;
    memset(temp,0,strlen(temp));
}
}

if(temp[0]!='\0'){
char* arg= strdup(temp);
new_argv[count++]=arg;
}

command-=(cmd_size);
free(temp);
free(command);

new_argv[++count]=NULL;

return count-1;

}
int check_symbol(char* cmd){
    if(strncmp(cmd,"<",1)==0){
        return 1;
    }
    else if(strncmp(cmd,">",1)==0){
        return 1;
    }
    else if(strncmp(cmd,"|",1)==0){
        return 1;
    }
    else if(strncmp(cmd,"2>",2)==0){
        return 2;
    }
    else{
        return 0;
    }
}

void exit_handler(){

    job* current= head;
    while(current!=NULL){
        if(current->gpid>0){
            killpg(current->pid,SIGHUP);
        }
        else{
            kill(current->pid,SIGHUP);
        }
        remove_job(current);
        current=head;
    }
}