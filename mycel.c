#define _GNU_SOURCE /* for pipe2() */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/types.h>

#define RED "\033[38;2;230;0;0m"
#define YELLOW "\033[38;2;200;200;0m"
#define GREEN "\033[38;2;0;150;0m"
#define NORMAL "\033[0m"
#define BOLD "\033[1m"

struct Commands
{
        const char *label;
        const char *command;
        char *output;
        size_t length;
        int failed;
};

/* Function used for pretty error handling */
static void error(const char *err_msg)
{
        fprintf(stderr, "%s%sERROR:%s%s\n", BOLD, RED, err_msg, NORMAL);
        exit(EXIT_FAILURE);
}

static struct Commands *createCommand(const char *label, const char *command)
{
        struct Commands *c = calloc(1, sizeof(struct Commands));
        if (c == NULL)
        {
                error("Out of memory.");
        }
        c->label = label;
        c->command = command;
        return c;
}

/* std System() code with output stapled on */
static int sys_cmd(struct Commands *c)
{
        int pipefd[2];
        pid_t pid;
        int status;
        int read_failed = 0;
        size_t capacity = 0;
        if (pipe2(pipefd, O_CLOEXEC) == -1)
        {
                return 1;
        }
        pid = fork();
        if (pid < 0)
        {
                close(pipefd[0]);
                close(pipefd[1]);
                return 1;
        }
        if (pid == 0)
        {
                /* async-signal-safe calls until exec */
                int devnull = open("/dev/null", O_RDONLY | O_CLOEXEC);
                if (devnull == -1 || dup2(devnull, STDIN_FILENO) == -1 || dup2(pipefd[1], STDOUT_FILENO) == -1)
                {
                        _exit(127);
                }
                execl("/bin/sh", "sh", "-c", c->command, (char *)NULL);
                _exit(127);
        }
        close(pipefd[1]);
        /* Read until EOF, otherwise a commands fail to exit pipe */
        for (;;)
        {
                ssize_t n;
                if (c->length == capacity)
                {
                        size_t new_capacity = capacity == 0 ? 4096 : capacity * 2;
                        char *grown = realloc(c->output, new_capacity);
                        if (grown == NULL)
                        {
                                read_failed = 1;
                                break;
                        }
                        c->output = grown;
                        capacity = new_capacity;
                }
                n = read(pipefd[0], c->output + c->length, capacity - c->length);
                if (n > 0)
                {
                        c->length += (size_t)n;
                }
                else if (n == 0)
                {
                        break;
                }
                else if (errno != EINTR)
                {
                        read_failed = 1;
                        break;
                }
        }
        close(pipefd[0]);
        while (waitpid(pid, &status, 0) == -1)
        {
                if (errno != EINTR)
                {
                        return 1;
                }
        }
        return read_failed || !WIFEXITED(status) || WEXITSTATUS(status) != 0;
}

static void *exec(void *cmd)
{
        struct Commands *c = cmd;
        c->failed = sys_cmd(c);
        return NULL;
}

/* print command output with label */
static void print_result(const struct Commands *c, int color)
{
        const char *label_color = c->failed ? RED : GREEN;
        const char *normal = color ? NORMAL : "";
        printf("%s%s:%s\n", color ? label_color : "", c->label, normal);
        if (c->length > 0)
        {
                fwrite(c->output, 1, c->length, stdout);
        }
        printf("\n%s\n", normal);
        fflush(stdout);
}
int main(void)
{
        struct Commands *cmds[] =
        {
                createCommand(
                        /*Label*/"Operating System/Kernel Info",
                        /*Command*/"(uname -a; cat /etc/*-release; cat /proc/version) 2>/dev/null"
                ),
                createCommand(
                        "IDs and Groups",
                        "echo \"YOURID=$(id)\"; for i in $(cut -d':' -f1 /etc/passwd); do id \"$i\"; done 2>/dev/null"
                ),
                createCommand(
                        "Env",
                        "env 2>/dev/null | grep -v 'LS_COLORS' 2>/dev/null"
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
                        "cat /etc/bashrc 2>/dev/null || cat /etc/bash.bashrc 2>/dev/null"
                ),
                createCommand(
                        ".bash_profile Contents",
                        "cat ~/.bash_profile 2>/dev/null"
                ),
                createCommand(
                        "Installed Packages (Dpkg)",
                        "dpkg --get-selections 2>/dev/null"
                ),
                createCommand(
                        "Installed Packages (Rpm)",
                        "rpm -qa 2>/dev/null"
                ),
                createCommand(
                        "Installed Packages (Apt)",
                        "apt list --installed 2>/dev/null"
                ),
                createCommand(
                        "Installed Packages (Yum)",
                        "yum list installed 2>/dev/null"
                ),
                createCommand(
                        "Installed Packages (Pacman)",
                        "pacman -Q 2>/dev/null"
                ),
                /* Append commands below */
        };

        /* Multithreading */
        pthread_t threads[sizeof(cmds)/sizeof(cmds[0])];
        const int color = isatty(STDOUT_FILENO);
        size_t i;
        for (i = 0; i < sizeof(cmds)/sizeof(cmds[0]); i++)
        {
                if (pthread_create(&threads[i], NULL, exec, cmds[i]) != 0)
                {
                        error("Failed to create thread.");
                }
        }
        for (i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++)
        {
                pthread_join(threads[i], NULL);
                print_result(cmds[i], color);
        }
        return 0;
}
