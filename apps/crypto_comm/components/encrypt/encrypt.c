#include <camkes.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils/util.h>
#include <gmssl/rand.h>

void pre_init(void) {
    uint32_t seed = rng_rand_num();
    gmssl_rand_seed_init(seed);
    ZF_LOGI("encrypt get random seed: 0x%08x", seed);
}

int run()
{
    printf("Hello World!\n");
    while(1);
}