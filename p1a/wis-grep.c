#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]) {
    FILE* fp;  // pointer for input file
    char* line = NULL;  // store command line arguments
    size_t length;  // hold the size of input buffer
    ssize_t rt;  // the return value of getline, used to check if read ends
    if (argc <= 1) {  // insufficient command line arguments
        printf("wis-grep: searchterm [file ...]\n");
        exit(1);
    } else if (argc == 2) {  // only have search term, taking standard input
       fp = stdin;
       while ((rt = getline(&line, &length, fp)) != -1) {  // read the standard inputs until nothing
            if (strstr(line, argv[1])) {  // check if search term is in the inputs
                printf("%s", line);
            } else {
                continue;
            }
        }
    } else {
        for (int i = 2; i < argc; i++) {
            fp = fopen(argv[i], "r");
            if (fp == NULL) {  // cannot open file
                printf("wis-grep: cannot open file\n");
                if (line) {
                    free(line);
                    line = NULL;
                }
                exit(1);
            }
            while ((rt = getline(&line, &length, fp)) != -1) {  // read input file
                if (strstr(line, argv[1])) {  // search term in file
                    printf("%s", line);
                } else {
                    continue;
                }
            }
            fclose(fp);  // free file pointer for each run
        }
    }
    free(line);
    line = NULL;
    fclose(fp);  // free file ptr
    return 0;
}
