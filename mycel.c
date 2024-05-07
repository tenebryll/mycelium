#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/types.h>

#define RED "\033[38;2;230;0;0m"
#define YELLOW "\033[38;2;200;200;0m"
#define GREEN "\033[38;2;0;150;0m"
#define NORMAL "\033[0m"
#define BOLD "\033[1m"
/*
TODO
- Allocate memory dynamically for commands' output, and add coordination between threads to keep their outputs separate when printing.
-Print command with header
-Options such as steath, no color, no multithreading, etc
*/
struct Commands {
	char *header;
	char *command;
};
/* Used to create commands */
struct Commands *createCommand(char *header, char *command) {
	struct Commands *c = malloc(sizeof(struct Commands));
	c->header = strdup(header);
	c->command = strdup(command);
	return c;
}
void error(const char *err_msg) {
	fprintf(stderr, "\033[1m%sERROR:%s\033[0m\n", RED, err_msg);
	exit(EXIT_FAILURE);
}
/*Executes system commands like System() but returns output */
int sys_cmd(const char *command, char *output) {
	int pipefd[2];
	pid_t pid;
	int status;
	if (pipe(pipefd) == -1) {
		error("Error creating pipe.");
	}
	pid = fork();
	if (pid < 0) {
		error("Fork failed.");
	}
	if (pid == 0) {
		close(pipefd[0]);
		if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
			error("Failed to duplicate file descriptor.");
		}
		close(pipefd[1]);
		if (execl("/bin/sh", "sh", "-c", command, NULL) == -1) {
			error("Error executing command.");
		}
	} 
	else {
		close(pipefd[1]);
		waitpid(pid, &status, 0);
		if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
			ssize_t n = read(pipefd[0], output, 4096);
			if (n > 0) {
				output[n] = '\0';
			}
			close(pipefd[0]);
			return 0;
		} 
		else {
			close(pipefd[0]);
			return 1;
		}
	}
	return -1;
}
/* Execute command with header */
void *exec(void *cmd) {
	char output[4096] = {0};
	if (sys_cmd(((struct Commands*)cmd)->command, output) == 0) {
		printf("%s%s:\033[0m\n", GREEN, ((struct Commands*)cmd)->header);
		printf("%s\n%s\n", output, NORMAL);
	} 
	else {
		printf("%s%s:\033[0m\n", RED, ((struct Commands*)cmd)->header);
		printf("%s\n%s\n", output, NORMAL);
	}
	return 0;
}
int main(void){
	struct Commands *cmds[] ={
		createCommand(
			"Operating System/Kernel Info",/*Header*/
			"uname -a ||: && (cat /etc/*-release ||: && (cat /proc/version; sleep 1 ||:)) 2>/dev/null"/*Command*/
		),
		createCommand(
			"IDs and Groups",
			"echo ID=$(id) && for i in $(cut -d':' -f1 /etc/passwd);do id $i;done; 2>/dev/null sleep 1  2>/dev/null"
		),
		createCommand(
			"Env",
			"env 2>/dev/null | grep -v 'LS_COLORS' 2>/dev/null"
		),
		createCommand(
			"Current User",
			"echo $(whoami) 2>/dev/null; sleep 1"
		),
		createCommand(
			"shells",
			"cat /etc/shells 2>/dev/null"
		),
		createCommand(
			"passwd Contents",
			"cat /etc/passwd 2>/dev/null"
		),
		createCommand(
			"master.passwd contents",
			"cat /etc/master.passwd 2>/dev/null"
		),
		createCommand(
			"group Contents",
			"cat /etc/group 2>/dev/null"
		),
		createCommand(
			"shadow Contents",
			"cat /etc/shadow 2>/dev/null"
		),
		createCommand(
			"gshadow Contents",
			"cat /etc/gshadow 2>/dev/null"
		),
		createCommand(
			"sudoers Contents",
			"cat /etc/sudoers 2>/dev/null"
		),
		createCommand(
			"profile Contents",
			"cat /etc/profile 2>/dev/null"
		),
		createCommand(
			"bashrc Contents",
			"cat /etc/bashrc 2>/dev/null"
		),
		createCommand(
			".bash_profile Contents",
			"cat /etc/bashrc 2>/dev/null"
		),
		createCommand(
			"bashrc Contents",
			"cat /etc/bashrc 2>/dev/null"
		),
		createCommand(
			".bashrc Contents",
			"cat ~/.bashrc 2>/dev/null"
		),
		createCommand(
			".bash_logout Contents",
			"cat ~/.bash_logout 2>/dev/null"
		),
		createCommand(
			"bin and sbin Contents",
			"ls -alh /usr/bin/ 2>/dev/null && ls -alh /sbin/ 2>/dev/null"
		),
		createCommand(
			"Installed Packages",
			"dpkg --get-selections||:&&(rpm -qa||:&&(pacman -Q||:&&(ls -alh /var/cache/yum/||:&&(ls -alh /var/cache/apt/archivesO||:)))) 2>/dev/null"
		),
	
	};
	/* Multithreading */
	pthread_t threads[sizeof(cmds)/sizeof(cmds[0])];
	size_t i;
	for (i = 0; i < sizeof(cmds)/sizeof(cmds[0]); i++) {
		char **thread = malloc(2 * sizeof(struct Commands*));
		thread[0] = cmds[i]->header;
		thread[1] = cmds[i]->command;
		pthread_create(&threads[i], NULL, exec, thread);
	}
	for (i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
		pthread_join(threads[i], NULL);
	}
	return 0;
}
