/**
 * CS 4348.003 Project 1 - Dallas Shell
 * Author: Eliot Partridge (EMP170002)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <wait.h>
#include <sys/stat.h>

const char ERROR_MESSAGE[30] = "An error has occurred\n";
const char PROMPT[10] = "dash> ";

/**
 * Linked list structure for storing the command search path
 */
struct pathentry {
	char entry[256];
	struct pathentry *next;
};

struct pathentry *path = NULL;

/**
 * Helper function to free the path list when rebuilding
 */
void free_path_list() {
	struct pathentry *tmp;

	while (path != NULL) {
		tmp = path;
		path = path->next;
		tmp->next = NULL;
		free(tmp);
	}
}

// trim functions adapted from https://stackoverflow.com/a/1431206/3397227
char *ltrim(char *str) {
	while (isspace(*str)) str++;
	return str;
}

char *rtrim(char *str) {
	char *back = str + strlen(str);
	while (isspace(*--back));
	*(back+1) = '\0';
	return str;
}

char *trim(char *str) {
	return rtrim(ltrim(str));
}

/**
 * Execute a single shell command
 *
 * @param rawCmd The command string to execute
 * @return 0 on success, -1 on error
 */
int exec_cmd(char rawCmd[256]) {
	char *tokPtr = NULL;
	char *outTokPtr = NULL;

	strtok_r(rawCmd, ">", &outTokPtr);
	char *outputFile = strtok_r(NULL, "", &outTokPtr); // consume rest of string as output file name

	int outFd = 0;
	if (outputFile) {
		outputFile = trim(outputFile);
		outFd = open(outputFile, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	}

	trim(rawCmd);
	char *cmd = strtok_r(rawCmd, " ", &tokPtr);

	if (cmd == NULL) return -1;

	if (strncmp(cmd, "exit", 4) == 0) {
		if (strtok_r(NULL, " ", &tokPtr)) { // arguments passed, error out
			return -1;
		} else exit(0);
	} else if (strncmp(cmd, "cd", 2) == 0) {
		char *cdPath = strtok_r(NULL, "", &tokPtr);
		if (!cdPath) return -1; // no arguments passed
		if (chdir(cdPath) == -1) return -1;
	} else if (strncmp(cmd, "path", 4) == 0) {
		char *pathDir;
		free_path_list();
		path = NULL;
		struct pathentry *tmp = NULL;
		// rebuild path list from arguments
		while ((pathDir = strtok_r(NULL, " ", &tokPtr)) != NULL) {
			if (path == NULL) {
				path = malloc(sizeof(struct pathentry));
				tmp = path;
			} else {
				tmp->next = malloc(sizeof(struct pathentry));
				tmp = tmp->next;
			}

			strncpy(tmp->entry, pathDir, 256);
			tmp->next = NULL;
		}
	} else {
		// execute general command
		char search[512];
		struct pathentry *searchPath = path;
		int found = 0;

		// iterate over path list
		while (searchPath != NULL) {
			snprintf(search, sizeof(search), "%s/%s", searchPath->entry, cmd);
			if (access(search, X_OK) == 0) {
				found = 1;
				break;
			}
			searchPath = searchPath->next;
		}

		if (!found) return -1;

		char **args = malloc(2 * sizeof(char *));
		args[0] = cmd;
		args[1] = NULL;
		size_t argN = 2;
		char *arg;
		while ((arg = strtok_r(NULL, " ", &tokPtr)) != NULL) {
			args = realloc(args, ++argN * sizeof(char *));
			args[argN - 2] = arg;
			args[argN - 1] = NULL;
		}

		pid_t child = fork();
		if (child == -1) {
			// fork failed
			return -1;
		} else if (child == 0) {
			// we are the child, exec now
			if (outputFile) {
				close(STDOUT_FILENO);
				dup(outFd);
			}
			if (execv(search, args) == -1) {
				return -1;
			}
		}

		free(args);
	}

	return 0;
}

int main(int argc, char *argv[]) {
	if (argc > 2) {
		write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
		return EXIT_FAILURE;
	}

	path = malloc(sizeof(struct pathentry));
	strcpy(path->entry, "/bin");
	path->next = NULL;

	int interactive = 1;

	if (argc == 2) {
		int fd = open(argv[1], O_RDONLY);
		if (fd == -1) {
			write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
			return EXIT_FAILURE;
		}
		if (dup2(fd, STDIN_FILENO) == -1) {
			write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
			return EXIT_FAILURE;
		}

		interactive = 0; // batch mode
	}

	char *inLine = malloc(256 * sizeof(char));
	size_t inLen = 256;
	ssize_t nRead = 0;

	if (interactive) write(STDOUT_FILENO, PROMPT, strlen(PROMPT));

	while ((nRead = getline(&inLine, &inLen, stdin)) != -1) {
		char *cmd = strtok(inLine, "&");

		while (cmd) {
			char cmdBuf[256];
			strncpy(cmdBuf, cmd, 256);

			if (exec_cmd(cmdBuf) == -1) {
				write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
			}

			cmd = strtok(NULL, "&");
		}

		while (wait(NULL) > 0);

		if (interactive) write(STDOUT_FILENO, PROMPT, strlen(PROMPT));
	}

	free_path_list(path);
	free(inLine);

	return EXIT_SUCCESS;
}
