#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#define TITLE_LENGTH 100  // define the required length of file name
#define PADDING '\0'  // the char that need to be padded when file name not long enough

int main(int argc, char* argv[]) {
    if (argc <= 2) {
        printf("wis-tar: tar-file file [...]\n");
        exit(1);
    } else {
        FILE* fp;  // file pointer for input files
        FILE* tar = fopen(argv[1], "w");  // file pointer for the output tar file
        char* line = NULL;  // use to store contents of input files
        size_t length = 0;
        ssize_t rt;
        for (int i = 2; i < argc; i++) {
            fp = fopen(argv[i], "r");
            if (fp == NULL) {  // input file cannot be open
                printf("wis-tar: cannot open file\n");
                fclose(tar);  // nothing to output
                if (line) {
                    free(line);
                    line = NULL;
                }
                exit(1);
            }
            char filename[strlen(argv[i])];  // store filename
            strcpy(filename, argv[i]);
            char storename[TITLE_LENGTH];  // desired-length filename
            if (strlen(filename) > TITLE_LENGTH) {  // input filename longer than required
                strncpy(storename, filename, TITLE_LENGTH);
            } else {
                strcpy(storename, filename);
                for (int i = strlen(filename); i < TITLE_LENGTH; i++) {
                    storename[i] = PADDING;  // padding until required length
                }
            }
            fwrite(storename, sizeof(storename), 1, tar);  // write filename to output tar file
            struct stat buf;  // use to get information of input files
            int err = stat(argv[i], &buf);
            if (err == -1) {  // error checking
                fclose(tar);
                fclose(fp);
                if (line) {
                    free(line);
                    line = NULL;
                }
                exit(1);
            }
            long filesize = buf.st_size;  // used to store input file size
            fwrite(&filesize, sizeof(filesize), 1, tar);  // write size info to output tar file
            while ((rt = getline(&line, &length, fp)) != -1) {
                fwrite(line, strlen(line), 1, tar);  // read contents of input files, write to tar file
            }
            fclose(fp);  // free input file ptr each run
        }
        free(line);
        line = NULL;
        fclose(tar);  // free tar file pointer
    }
}
