#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <wait.h>

const char ERROR_MESSAGE[30] = "An error has occurred\n";

struct pathentry {
	char entry[256];
	struct pathentry *next;
};

void free_path_list(struct pathentry *head) {
	struct pathentry *tmp;

	while (head != NULL) {
		tmp = head;
		head = head->next;
		tmp->next = NULL;
		free(tmp);
	}
}

int main(int argc, char *argv[]) {
	if (argc > 2) {
		write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
		return EXIT_FAILURE;
	}

	struct pathentry *path = malloc(sizeof(struct pathentry));
	strcpy(path->entry, "/bin");
	path->next = malloc(sizeof(struct pathentry));
	strcpy(path->entry, "/usr/bin");
	path->next->next = NULL;

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

	if (interactive) write(STDOUT_FILENO, "dash> ", 6);

	while ((nRead = getline(&inLine, &inLen, stdin)) != -1) {
		inLine[strcspn(inLine, "\r\n")] = 0; // strip \r\n
		char *cmd = strtok(inLine, " ");

		if (strncmp(cmd, "exit", 4) == 0) {
			if (strtok(NULL, " ")) {
				write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
			} else return EXIT_SUCCESS;
		} else if (strncmp(cmd, "cd", 2) == 0) {
			char *cdPath = strtok(NULL, "");
			if (!cdPath) {
				write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
			} else {
				if (chdir(cdPath) == -1) {
					write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
				}
			}
		} else if (strncmp(cmd, "path", 4) == 0) {
			char *pathDir;
			free_path_list(path);
			path = NULL;
			struct pathentry *tmp = NULL;
			while ((pathDir = strtok(NULL, " ")) != NULL) {
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
			char search[512];
			struct pathentry *searchPath = path;
			int found = 0;

			while (searchPath != NULL) {
				snprintf(search, sizeof(search), "%s/%s", searchPath->entry, cmd);
				if (access(search, X_OK) == 0) {
					found = 1;
					break;
				}
				searchPath = searchPath->next;
			}

			if (!found) {
				write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
			} else {
				char** args = malloc(2 * sizeof(char*));
				args[0] = search;
				args[1] = NULL;
				size_t argN = 2;
				char *arg;
				while ((arg = strtok(NULL, " ")) != NULL) {
					args = realloc(args, ++argN * sizeof(char*));
					args[argN-2] = arg;
					args[argN-1] = NULL;
				}

				pid_t child = fork();
				if (child == -1) {
					// fork failed
					write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
				} else if (child == 0) {
					// we are the child, exec now
					if (execv(search, args) == -1) {
						write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
						return EXIT_FAILURE;
					}
				} else {
					// we are the parent, child is executing subprogram
					// wait until all child processes exit
					while (wait(NULL) > 0);
				}

				free(args);
			}
		}

		if (interactive) write(STDOUT_FILENO, "dash> ", 6);
	}

	free_path_list(path);
	free(inLine);

	return EXIT_SUCCESS;
}
