#include "types.h"
#include "stat.h"
#include "user.h"

int
main (int argc, char* argv[]) {
    int filenum = getfilenum(getpid());
    printf(1, "number of file=%d\n", filenum);
    exit();
}
