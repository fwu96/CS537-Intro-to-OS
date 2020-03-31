# p1a - README

### Files
* wis-grep.c
    * Basically, it does the similar work as unix command `grep` did. 
    * When the input file exists / can be open, the program would read the contents line by line and find whether the required search term exists
    * When user only given search term, the program would take standard input and do the search
    * Briefly implementation
        * `getline()` used to read input file each line, then using `strstr()` to check if the line contains search term
* wis-tar.c
    * It does the work to compress single or multiple files into one file
    * It will open each input files, store and write the file name, size, as well as contents into the output tar file
    * Briefly implementation
        * `strlen()` is mainly used to check if the input file name longer than required length
        * using `fwrite()` to write to the output tar file
        * `getline()` is used to read contents of input files when the file is non-empty
* wis-untar.c
    * It does the reverse work as wis-tar did
    * It will open the given tar file, figure out how many files contained in the tar file, and the contents of each file
    * At the end the program would output every contained files with their own contents
    * Briefly implementation
        * `fread()` is mainly used to read the contents of given tar file
        * the information of contained file is read with the order of filename, filesize, and contents
        * obtained filename is used as output filename, `fwrite()` is used to write the related contents to the output file
