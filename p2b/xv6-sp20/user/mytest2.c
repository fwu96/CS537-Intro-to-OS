#include "types.h"
#include "user.h"
#include "stat.h"
#include "pstat.h"

int main (int argc, char* argv[]) {
    struct pstat st;
    if (argc != 2) {
        printf(1, "usage: mytest2 counter");
        exit();
    }
    int i, x, l, j;
    int pid = getpid();
    for (i = 1; i < atoi(argv[1]); i++) {
        x = x + i;
    }
    boostproc();
    for (i = 1; i < atoi(argv[1]); i++) {
        x = x + i;
    }
    getprocinfo(&st);
    for (j = 0; j < NPROC; j++) {
        if (st.inuse[j] && st.pid[j] >= 3 && st.pid[j] == pid) {
            for (l = 3; l >= 0; l--) {
                printf(1, "level:%d \t ticks-used:%d\n", l, st.ticks[j][l]);
            }
        }
    }
    exit();
    return 0;
}
