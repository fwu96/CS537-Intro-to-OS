#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#define WHITE " \f\n\r\t\v"  // All possible white spaces

char ERR_MESSAGE[30] = "An error has occurred\n";  // Error Message
char* PATHLIST[1024];  // The path list
int pathNum;  // Count for exiting path number

/**
 * Print error message when someting wrong
 */
void print_err () {
    write(STDERR_FILENO, ERR_MESSAGE, strlen(ERR_MESSAGE));
}
/**
 * Execute commands
 */
int smash_execute (char* args[], char* redir[]) {
    int pid;  // store fork() result
    int status;  // use for waitpid()
    char* execPath = malloc(128 * sizeof(char*));
    int index = 0;
    int invalid = 1;
    if (PATHLIST[0] == NULL) {  // There is no path in the path list
        print_err();
        return 1;
    }
    // No arguments passed in
    if (args == NULL)
        return 1;
    if (args[0] == NULL)
        return 1;
    while (1) {
        // There are no more path in the path list
        if (PATHLIST[index] == NULL)
            break;
        char* path = malloc(128 * sizeof(char));
        if (!strcpy(path, PATHLIST[index])) {  // Something wrong with the stored path
            print_err();
            return 1;
        }
        int ridx = strlen(path);
        path[ridx] = '/';
        path[ridx + 1] = '\0';
        strcat(path, args[0]);
        if (access(path, X_OK) == 0) {  // Access successfully
            strcpy(execPath, path);
            invalid = 0;
            free(path);
            break;
        }
        free(path);
        index++;
    }
    if (invalid) {  
        print_err();
        return 1;
    }
    pid = fork();
    if (pid == -1) {
        print_err();
        return 1;
    } else if (pid == 0) {
        if (redir) {  //redirection
            int fd = open(redir[0], O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU);
            if (fd > 0) {
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                fflush(stdout);
            }
        }
        execv(execPath, args);
        exit(0);
    } else {
        waitpid(pid, &status, 0);
    }
    return 0;
}
/**
 * The cd command
 */
int smash_cd (int argc, char* args[]) {
    if (argc != 2) {  // Number of commands is not 2
        print_err();
        return 1;
    }
    if (chdir(args[1]) == -1) {  // The direction is invalid
        print_err();
        return 1;
    }
    return 0;
}
/**
 * The path command
 */
int smash_path (int argc, char* args[]) {
    if (argc > 3) {  // Number of arguments more than 3
        print_err();
        return 1;
    }
    if (argc == 3) {  // path add || path remove
        if (strcmp(args[1], "add") == 0) {  // add path to path list
            pathNum++;
            if (pathNum == 1) {
                PATHLIST[0] = args[2];
            } else {
                for (int i = pathNum - 1; i > 0; i--) {
                    PATHLIST[i] = PATHLIST[i - 1];
                }
                PATHLIST[0] = args[2];
            }
        }
        if (strcmp(args[1], "remove") == 0) {  // remove required path from path list
            int found = 0;
            if (pathNum == 0) {  // There is no path in the path list at the beginning
                print_err();
                return 1;
            } else {  
                for (int i = 0; i < pathNum; i++) {
                    if (strcmp(args[2], PATHLIST[i]) == 0) {  // There is path matched
                        found = 1;
                        pathNum--;  // remove valid, number of path decreased
                        if (pathNum == 0) {  // There is only one path in the list
                            PATHLIST[0] = "";
                        } else {
                            for (int j = i; j < pathNum; j++) {
                                PATHLIST[j] = PATHLIST[j + 1];
                                PATHLIST[j + 1] = "";
                            }
                        }
                    }
                }
            }
            if (found == 0) {  // There are no matched pathes in the list
                print_err();
                return 1;
            }
        }
    }
    if (argc == 2) {  // path clear || invalid commands
        if (strcmp(args[1], "clear") == 0) {  // path clear
            for (int i = 0; i < pathNum; i++) {
                PATHLIST[i] = "";
            }
            pathNum = 0;
        } else {
            print_err();
            return 1;
        }
    }
    return 0;
}
/**
 * Seperate commands lines with all possible white spaces
 */
int smash_separate (char* line, char* args[], char* whites) {
    char* saveptr;
    int index = 0;
    if (!args)
        args = malloc(128 * sizeof(char));
    args[index] = strtok_r(line, whites, &saveptr);
    index++;
    while (1) {
        // parse string
        args[index] = strtok_r(NULL, whites, &saveptr);
        if (args[index] == NULL)
            break;
        index++;
    }
    if (args[0] == NULL)
        return 0;
    return index;
}
/**
 * Redirecing commands
 */
int smash_redirection (char* redir, char* line) {
    char* args[128];
    char* redr[128];
    redir[0] = '\0';
    redir = redir + 1;
    // Dealing with commands
    int commNum = smash_separate(line, args, WHITE);
    if (commNum == 0) {
        print_err();
        return 1;
    }
    // Dealing with redirection
    int redrNum = smash_separate(redir, redr, WHITE);
    if (redrNum != 1) {  // more than or less than 1 valid output (redirection)
        print_err();
        return 1;
    }
    // executing redirection commands
    smash_execute(args, redr);
    return 0;
}
/**
 * Multiple commands
 */
int smash_multiple (char* argv, char* line) {
    char** commands = malloc(128 * sizeof(char*));
    int commNum = smash_separate(line, commands, ";");
    char** args = malloc(128 * sizeof(char*));
    char* redr = NULL;
    for (int i = 0; i < commNum; i++) {
        // Handle redirection witnin multiple commands
        if ((redr = strchr(commands[i], '>'))) {
            smash_redirection(redr, commands[i]);
            continue;
        }
        smash_separate(commands[i], args, WHITE);
        smash_execute(args, NULL);
    }
    free(args);
    free(commands);
    return 0;
}
/**
 * Parallel commands
 */
int smash_parallel (char* argv, char* line) {
    char** commands = malloc(128 * sizeof(char*));
    int commNum = smash_separate(line, commands, "&");
    char** args = malloc(128 * sizeof(char*));
    char* redr = NULL;
    char* sub = NULL;
    for (int i = 0; i < commNum; i++) {
        // Handle multiple commands within parallel
        if ((sub = strchr(commands[i], ';'))) {
            smash_multiple(sub, commands[i]);
            continue;
        }
        // Handle redirection within parallel
        if ((redr = strchr(commands[i], '>'))) {
            smash_redirection(redr, commands[i]);
            continue;
        }
        smash_separate(commands[i], args, WHITE);
        smash_execute(args, NULL);
    }
    free(args);
    free(commands);
    return 0;
}
/**
 * Reading commands
 */
int smash_read_commands (char* args[], FILE* fp) {
    char* line = malloc(128 * sizeof(char));
    size_t len = 0;
    ssize_t nread;
    char* redr = NULL;
    char* para = NULL;
    char* mult = NULL;
    fflush(stdin);
    // EOF
    if ((nread = getline(&line, &len, fp)) == -1)
        return 1;
    // Empty line
    if ((strcmp(line, "\n") == 0) || strcmp(line, "") == 0)
        return -1;
    // Ends with new line
    if (line[strlen(line) - 1] == '\n')
        line[strlen(line) - 1] = '\0';
    // Parallel commands
    if ((para = strchr(line, '&'))) {
        smash_parallel(para, line);
        return -1;
    }
    // Multiple commands
    if ((mult = strchr(line, ';'))) {
        smash_multiple(mult, line);
        return -1;
    }
    // Redirection request
    if ((redr = strchr(line, '>'))) {
        smash_redirection(redr, line);
        return -1;
    }
    // get arguments number by seperating with white spaces
    int argNum = smash_separate(line, args, WHITE);
    // there is not arguments
    if (argNum == 0)
        return 0;
    // exit
    if (strcmp(args[0], "exit") == 0) {
        if (args[1])
            print_err();
        exit(0);
    }
    // cd
    if (strcmp(args[0], "cd") == 0) {
        smash_cd(argNum, args);
        return -1;
    }
    // path
    if (strcmp(args[0], "path") == 0) {
        smash_path(argNum, args);
        return -1;
    }
    
    return 0;
}
/**
 * Main 
 */
int main (int argc, char* argv[]) {
    PATHLIST[0] = "/bin";  // Initial path
    pathNum = 1;  // There is one path (/bin) in the path list at the beginning
    FILE* fp;  // Handle batch mode
    char** commands;
    int batchMode = 0;
    if (argc == 2) {  // Batch mode
        if (!(fp = fopen(argv[1], "r"))) {  // The batch mode file cannot be opened
            print_err();
            exit(1);
        }
        batchMode = 1;
    } else if (argc < 1 || argc > 2) {  // invalid argument number
        print_err();
        exit(1);
    }
    while (1) {
        if (batchMode == 1) {  // batch mode
            while (1) {
                commands = malloc(128 * sizeof(char));
                int readRes = smash_read_commands(commands, fp);
                fflush(fp);
                if (readRes == -1)  // continue to next line
                    continue;
                if (readRes == 1) {  // EOF
                    break;
                }
                // executing commands
                int execRes = smash_execute(commands, NULL);
                free(commands);
                if (execRes == 1)  // new turn
                    continue;
            }
            fclose(fp);
            break;
        } else {  // user input
            fprintf(stdout, "smash> ");
            fflush(stdout);
            commands = malloc(128 * sizeof(char));
            // exectuing commands
            int readRes = smash_read_commands(commands, stdin);
            fflush(stdin);
            if (readRes == -1)  // next input
                continue;
            if (readRes == 1) {  // input ends
                break;
            }
            if (smash_execute(commands, NULL) == 1)  // next turn when error (print new prompt)
                continue;
            free(commands);
        }
    }
    printf("\n");
    return 0;
}
