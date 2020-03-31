#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TITLE_LENGTH 100  // define the required length of filename

int main(int argc, char* argv[]) {
    if (argc <= 1 || argc > 2) {
        printf("wis-untar: tar-file\n");
        exit(1);
    } else {
        FILE* fp;  // pointer for input file
        fp = fopen(argv[1], "rb");
        if (fp == NULL) {
            printf("wis-untar: cannot open file\n");
            exit(1);
        }
        char title[TITLE_LENGTH];  // store filename
        while (fread(title, sizeof(char), sizeof(title), fp) == TITLE_LENGTH) {  // tar still contain another file
            FILE* exc;  // pointer for files compressed inside tar file
            exc = fopen(title, "w");
            long filesize;  // size of current file
            fread(&filesize, sizeof(char), sizeof(filesize), fp);
            if (filesize != 0) {  // there are contents in current file
                char contents[(int)filesize];
                fread(contents, sizeof(char), (int)filesize, fp);
                fwrite(contents, sizeof(contents), 1, exc);  // read and write to output file     
            }
            fclose(exc);  // free pointer for current file each turn
        }
        fclose(fp);  // free pointer for input file
    }    
    return 0;
}
