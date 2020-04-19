/* program:MyShell, Editor:Heo Jeon Jin(32164959@dankook.ac.kr), date:19-10-30*/
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <string.h>
#define MAX_SYS 128
bool cmd_tty(int argc, char*argv[]);
bool cmd_quit(int argc, char*argv[]);
bool cmd_cd(int argc, char*argv[]);
bool cmd_help(int argc, char *argv[]);
int chk_cmd(char *line[]);
int tokenize(char *buf, char *delims, char *tokens[], int max);
bool run(char *line);

/* This is internal commands with struct */
struct shell {char *key; char *key_ex; bool (*intr_func)(int argc, char *argv[]);};
struct shell cmd[] = {
	{"help", "Show this help again", cmd_help},
	{"quit", "Quit MyShell", cmd_quit},
	{"cd", "Change directory", cmd_cd},
	{"tty", "Show connected terminal's filename", cmd_tty},
	{">", "REDIRECTION: OUTPUT"},
	{"<", "REDIRECTION: INPUT"},
	{"&", "Run task on background"},
	{"|", "pipe fds execution"}
};

int main(){
	char line[1024], usr[MAX_SYS], cwd[MAX_SYS];
	getlogin_r(usr, MAX_SYS); //get username
	getcwd(cwd, MAX_SYS); //get current directory's path
	while(1){
		printf("%s@%s >> ", usr, cwd);
		fgets(line, sizeof(line) - 1, stdin);
		if(run(line) == false) break;
	}
	return 0;
}

bool cmd_tty(int argc, char*argv[]){
	printf("current tty: %s\n", ttyname(0)); return true;
}
bool cmd_quit(int argc, char*argv[]){return 0;}
bool cmd_cd(int argc, char*argv[]){
	if(argc == 1); //continue with just "cd"
	else if(argc == 2) {if(chdir(argv[1])) printf("type correct directory\n");}
	else printf("USAGE: %s [dir_name]\n", argv[0]);
	return true;
}
bool cmd_help(int argc, char *argv[]){
	int i;
	printf("-------HELLO, It's MyShell by HJJ at DKU--------\n");
	printf("| These shell commands are defined internally. |\n");
	printf("------------------------------------------------\n");
	
	printf("CMD\t FUNCTION\n------------------------------------------------\n");
	for(i=0; i<4; i++) printf("%s\t %s\n", cmd[i].key, cmd[i].key_ex);
	printf("------------------------------------------------\n");
	printf("KEY\t FUNCTION\n------------------------------------------------\n");
	for(i=4; i<8; i++) printf("%s\t %s\n", cmd[i].key, cmd[i].key_ex);
	return true;
}
void cmd_red_out(int argc, char *argv[]){
	int fd;
	if(argc != 4){printf("USAGE: %s input > output\n", argv[0]);}
	if((fd = open(argv[3], O_WRONLY | O_CREAT, 0664)) < 0) {
		printf("Can't open %s file with errno %d\n", argv[3], errno);
		return;
	}
	dup2(fd, STDOUT_FILENO);
	argv[2] = NULL;
	close(fd);
}
void cmd_red_in(int argc, char *argv[]){
	int fd;
    if((fd = open(argv[2], O_RDONLY)) < 0) {
       	printf("Can't open %s file with errno %d\n", argv[2], errno);
       	return;
    }
	dup2(fd, STDIN_FILENO);
	argv[1] = NULL;
    close(fd);
}
void cmd_pipe(int argc, char *argv[]){
	int i, read_size, stat, fd[2];
	pid_t pid;
	char bufc[MAX_SYS], bufp[MAX_SYS];
	if(argc != 4){
		printf("USAGE: %s file_name | file_name\n", argv[0]);
		return;
	}
	if(pipe(fd) < 0){
		printf("pipe error\n"); return;
	}
	if((pid = fork()) == 0){
		write(fd[1], argv[1], MAX_SYS);
		sleep(1);
		read_size = read(fd[0], bufc, MAX_SYS);
		bufc[read_size] = '\0';
		exit(0);
	}
	else {
		read_size = read(fd[0], bufp, MAX_SYS);
		bufp[read_size] = '\0';
		write(fd[1], argv[3], MAX_SYS);
		wait(&stat);
		close(fd[0]); close(fd[1]);
	}
	return;
}
int chk_cmd(char *line[]){
	int i;
	for(i=0; line[i] != NULL; i++){
		if(!strcmp(line[i], "<")) return 1; //check redirection STDIN
		if(!strcmp(line[i], ">")) return 2; //check redirection STDOUT
		if(!strcmp(line[i], "|")) {line[i] = NULL; return 3;} //check pipe
		if(!strcmp(line[i], "&")) {line[i] = NULL; return 4;} //check background processing
	}
	return 0;
}
int tokenize(char *buf, char *delims, char *tokens[], int max){
	int token_count = 0;
	char *token = strtok(buf, delims);
	while(token != NULL && token_count < max){
		tokens[token_count] = token; token_count++;
		token = strtok(NULL, delims);
	}
	tokens[token_count] = NULL; //make NULL to point the token is finished
	return token_count;
}
bool run(char *line){
    char delims[] = " \n";
    char *tokens[MAX_SYS];
    pid_t pid;
    int i, stat, chk;
	int token_count = tokenize(line, delims, tokens, sizeof(tokens) / sizeof(char*));
	if(token_count == 0) return true; //return if empty
	chk = chk_cmd(tokens); //check internal commands
	for(i = 0; i < 4; i++) { //do internal function if first token equals cmd's key
        if(strcmp(cmd[i].key, tokens[0]) == 0)
        	return cmd[i].intr_func(token_count, tokens);
    }
	if((pid=fork()) < 0) {perror("fork error caused\n"); exit(-1);}
	else if(pid == 0) {
		if(chk == 1){ //do redirection STDIN
		cmd_red_in(token_count, tokens);
		execvp(tokens[0], tokens);
		}
		else if(chk == 2){ //do redirection STDOUT
		cmd_red_out(token_count, tokens);
		execvp(tokens[0], tokens);
		}
		else if(chk == 3) cmd_pipe(token_count, tokens); //do pipe
		else{
			execvp(tokens[0], tokens);
			printf("execute failed\n"); //if it failed execution
			exit(-1);
		}
	}
	else {
		if(chk == 4) { //do background processing with WNOHANG flag
			waitpid(pid, &stat, WNOHANG); 
			sleep(1);
		}
		else waitpid(pid, &stat, 0);
	}
	return true;
}
