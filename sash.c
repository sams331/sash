#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>

#define MAX_LINE 80

// Fungsi pembantu untuk menyusun prompt berdasarkan variabel PREFIX
void build_prompt(char *dest, size_t dest_size, const char *format, const char *user, const char *host, const char *dir) {
    size_t i = 0, j = 0;
    while (format[i] != '\0' && j < dest_size - 1) {
        if (format[i] == '\\' && format[i + 1] != '\0') {
            // Deteksi simbol format kustom
            if (format[i + 1] == 'u') {
                j += snprintf(dest + j, dest_size - j, "%s", user);
                i += 2;
            } else if (format[i + 1] == 'h') {
                j += snprintf(dest + j, dest_size - j, "%s", host);
                i += 2;
            } else if (format[i + 1] == 'w') {
                j += snprintf(dest + j, dest_size - j, "%s", dir);
                i += 2;
            } else {
                // Jika bukan simbol format, cetak karakter backslash asli
                dest[j++] = format[i++];
            }
        } else {
            dest[j++] = format[i++];
        }
    }
    dest[j] = '\0';
}

void handle_sigint(int sig) {
	printf("\n");
	rl_on_new_line();
	rl_replace_line("", 0);
	rl_redisplay();
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
    	if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
		printf("sash 0.2 Dynamic Prompt Edition\n");
		return 0;
	} else {
		printf("sash: invalid option '%s'\n", argv[1]);
		return 1;
	}
    }

    char *input;
    char *args[MAX_LINE / 2 + 1];
    int should_run = 1;

    char hostname[HOST_NAME_MAX];
    char cwd[PATH_MAX];
    char display_cwd[PATH_MAX];
    char prompt[PATH_MAX + HOST_NAME_MAX + 100];
    char *username;

    char *home_dir = getenv("HOME");

    signal(SIGINT, handle_sigint);
    setenv("SHELL", "sash", 1);
    setenv("PREFIX", "\\u|\\h: \\w > ", 1); 

    printf("Welcome to sash 0.2 Dynamic Prompt Edition.\n\n");

    while (should_run) {
        username = getlogin();
        if (username == NULL) username = "user";

        gethostname(hostname, sizeof(hostname));
        getcwd(cwd, sizeof(cwd));

        if (home_dir != NULL && strstr(cwd, home_dir) == cwd) {
            snprintf(display_cwd, sizeof(display_cwd), "~%s", cwd + strlen(home_dir));
        } else {
            strncpy(display_cwd, cwd, sizeof(display_cwd));
        }

        char *current_prefix = getenv("PREFIX");
        if (current_prefix == NULL) current_prefix = "sash $ ";

        build_prompt(prompt, sizeof(prompt), current_prefix, username, hostname, display_cwd);

        input = readline(prompt);
        if (input == NULL) break;
        if (strlen(input) == 0) { free(input); continue; }

        add_history(input);

        if (strncmp(input, "PREFIX=", 7) == 0) {
            char *val = strchr(input, '=') + 1;
            if (val[0] == '"' || val[0] == '\'') {
                val++; 
                val[strlen(val) - 1] = '\0';
            }
            setenv("PREFIX", val, 1);
            free(input);
            continue;
        }

        int i = 0;
        args[i] = strtok(input, " ");
        while (args[i] != NULL) {
            if (args[i][0] == '$') {
                char *env_val = getenv(args[i] + 1);
                args[i] = (env_val != NULL) ? env_val : "";
            }
            i++;
            args[i] = strtok(NULL, " ");
        }

        if (strcmp(args[0], "exit") == 0) {
            should_run = 0; free(input); continue;
        }

        if (strcmp(args[0], "cd") == 0) {
            if (args[1] == NULL) {
                if (chdir(home_dir) != 0) perror("sash: cd failed");
            } else if (args[1][0] == '~') {
                char path_expanded[PATH_MAX];
                snprintf(path_expanded, sizeof(path_expanded), "%s%s", home_dir, args[1] + 1);
                if (chdir(path_expanded) != 0) perror("sash: cd failed");
            } else {
                if (chdir(args[1]) != 0) perror("sash: cd failed");
            }
            free(input); continue; 
        }

        if (strcmp(args[0], "sash-info") == 0) {
            printf("sash 0.2 (Dynamic Prompt Edition)\n");
	    printf("sams331's fun testing and learning project\n");
	    printf("Compiled in gcc, edited in vim\n");
            free(input); continue; 
        }

	if (strcmp(args[0], "help") == 0) {
		printf("\nsash 0.2 | Help\n");
		printf("Built-in commands:\n");
		printf("cd - Change Directory\nUsage: cd <directory name>\n=============================\n");
		printf("exit - Exits sash\n==============================\n");
		printf("help - Shows this help menu\n==============================\n");
		printf("prefix-help - Shows you help on editing the prefix\n==============================\n");
		printf("sash-info - Shows the info about sash\n==============================\n");
		printf("For other commands' info, you can do 'info <command>' or 'man <command>'.\n\n");
		free(input); continue;
	}

	if (strcmp(args[0], "prefix-help") == 0) {
		printf("\nPrefix\n");
		printf("What you see before a command is called prefix in sash. It's called PS1 in other shells but this is a simple shell so I decided to just call it prefix.\n");
		printf("Customizing it is possible. You just have to edit the \"PREFIX\" variable.\n\n");
		printf("There are some formatting:\n");
		printf("- \\u is for username.\n");
		printf("- \\h is for hostname.\n");
		printf("- \\w is for current directory.\n");
		printf("Example usage: \" \\u@\\h: \\w $ \" returns \" username@hostname: ~ $  \"\n");
		printf("If you want to reset your prefix to the default, exit and reopen sash.\n\n");
		free(input); continue;
	}

        pid_t pid = fork();
        if (pid < 0) {
            perror("sash: Fork failed"); free(input); return 1;
        } else if (pid == 0) {
            if (execvp(args[0], args) < 0) {
                printf("sash: command not found: %s\n", args[0]); exit(1);
            }
        } else {
            wait(NULL);
        }

        free(input);
    }

    printf("exit\n");
    return 0;
}

