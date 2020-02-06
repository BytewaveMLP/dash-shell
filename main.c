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

#define PATH_ENTRY_SIZE 4096

/**
 * Linked list structure for storing the command search path
 */
struct pathentry {
	char entry[PATH_ENTRY_SIZE];
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

/**
 * Trim whitespace from the left of the string
 *
 * @param str The string to trim
 * @return A pointer inside the original string with whitespace removed
 */
char *ltrim(char *str) {
	while (isspace(*str)) str++;
	return str;
}

/**
 * Trim whitespace from the right of the string
 *
 * @param str The string to trim
 * @return A pointer to the original string
 */
char *rtrim(char *str) {
	char *back = str + strlen(str);
	while (isspace(*--back));
	*(back+1) = '\0';
	return str;
}

/**
 * Trim whitespace from both ends of the string
 *
 * @param str The string to trim
 * @return A pointer inside the original string with whitespace removed
 */
char *trim(char *str) {
	return rtrim(ltrim(str));
}

/**
 * Execute a single shell command
 *
 * @param rawCmd The command string to execute
 * @return 0 on success, -1 on error
 */
int exec_cmd(char *rawCmd) {
	char *tokPtr = NULL;
	char *outTokPtr = NULL;

	strtok_r(rawCmd, ">", &outTokPtr); // discard text before the >
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
		if (strtok_r(NULL, " ", &tokPtr)) return -1; // arguments passed, error out
		else exit(0);
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

			strncpy(tmp->entry, pathDir, PATH_ENTRY_SIZE);
			tmp->next = NULL;
		}
	} else {
		// execute general command
		char search[PATH_ENTRY_SIZE * 2 + 2];
		search[0] = '\0';
		struct pathentry *searchPath = path;
		int found = 0;

		// iterate over path list
		while (searchPath != NULL) {
			strncat(search, searchPath->entry, PATH_ENTRY_SIZE);
			strncat(search, "/", 1);
			strncat(search, cmd, PATH_ENTRY_SIZE);

			if (access(search, X_OK) == 0) {
				found = 1;
				break;
			}
			searchPath = searchPath->next;
		}

		if (!found) return -1;

		char **args = malloc(2 * sizeof(char *));
		args[0] = cmd; // make programs using argv[0] happy
		args[1] = NULL; // execv needs null-terminated argv list
		size_t argN = 2;
		char *arg;
		while ((arg = strtok_r(NULL, " ", &tokPtr)) != NULL) {
			args = realloc(args, ++argN * sizeof(char *));
			args[argN - 2] = arg;
			args[argN - 1] = NULL;
		}

		pid_t child = fork();
		if (child == -1) return -1; // fork failed
		else if (child == 0) {
			// we are the child, exec now
			if (outputFile) {
				if (dup2(outFd, STDOUT_FILENO) == -1) { // replace stdout
					// failed to replace stdout
					return -1;
				}
			}
			if (execv(search, args) == -1) return -1;
		}

		free(args);
	}

	return 0;
}

int main(int argc, char *argv[]) {
	if (argc > 2) { // too many arguments
		write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
		return EXIT_FAILURE;
	}

	path = malloc(sizeof(struct pathentry));
	strcpy(path->entry, "/bin");
	path->next = NULL;

	int interactive = 1;

	if (argc == 2) {
		int fd;
		if ((fd = open(argv[1], O_RDONLY)) == -1) { // open batch file
			// failed to open batch file
			write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
			return EXIT_FAILURE;
		}
		if (dup2(fd, STDIN_FILENO) == -1) { // replace stdin
			// failed to replace stdin
			write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
			return EXIT_FAILURE;
		}

		interactive = 0; // set batch mode
	}

	char *inLine = NULL;
	size_t inLen = 0;

	if (interactive) write(STDOUT_FILENO, PROMPT, strlen(PROMPT));

	while (getline(&inLine, &inLen, stdin) != -1) {
		char *cmd = strtok(inLine, "&");

		while (cmd) {
			char cmdBuf[inLen];
			strncpy(cmdBuf, cmd, inLen);

			if (exec_cmd(cmdBuf) == -1) write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));

			cmd = strtok(NULL, "&");
		}

		while (wait(NULL) > 0); // wait for children to exit

		if (interactive) write(STDOUT_FILENO, PROMPT, strlen(PROMPT));
	}

	free_path_list();
	free(inLine);

	return EXIT_SUCCESS;
}
