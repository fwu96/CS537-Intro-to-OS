#include "types.h"
#include "stat.h"
#include "user.h"
#include "pstat.h"

int main (int argc, char* argv[]) {
    if (argc != 2) {
        printf(1, "usage: mytest1 counter");
        exit();
    }
    struct pstat st;
    int pid;
    int fst;
    
    int x = 0;
    fst = fork();

    for (int i = 1; i < atoi(argv[1]); i++) {
        x = x + i;
    }
    getprocinfo(&st);
    pid = getpid();
    for (int idx = 0; idx < NPROC; idx++) {
        if (st.pid[idx] == pid) {
            printf(1, "PID: %d, { P%d:%d, P%d:%d, P%d:%d, P%d:%d }\n", pid, 3, st.ticks[idx][3], 2, st.ticks[idx][2], 1, st.ticks[idx][1], 0, st.ticks[idx][0]);
        }
    }
    if (fst > 0)
        wait();
    exit();
    return 0;
}
