#include <camkes.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils/util.h>
#include <gmssl/rand.h>
#include <gmssl/sm4.h>
#include <mavlink2/checksum.h>
#include <crypto/global.h>
#include <crypto/ring_buffer.h>

static volatile struct pl011_regs *gcs_uart;
static volatile struct pl011_regs *fc_uart;

static SM4_KEY sm4_encrypt_key;
static SM4_KEY sm4_decrypt_key;
static uint8_t iv[16];

static uint8_t buffer[4096];
static uint8_t decrypt_buffer[PACKAGE_MAX_DATA_LEN];
static size_t decrypt_buffer_len;
static struct encrypt_package package;

void pre_init(void) {
    uint32_t seed = rng_rand_num();
    gmssl_rand_seed_init(seed);
    ZF_LOGI("encrypt get random seed: 0x%08x", seed);
}

int run()
{
    while(1);
}