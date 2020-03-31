#include "rand.h"

unsigned int rand_seed = -1;

void xv6_srand (unsigned seed) {
    rand_seed = seed;
}

int xv6_rand () {
    if (rand_seed == -1)
        rand_seed = 1;
    unsigned int s = rand_seed;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    rand_seed = s;
    return s % XV6_RAND_MAX;
}
