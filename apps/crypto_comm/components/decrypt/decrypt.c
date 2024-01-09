#include <camkes.h>
#include <stdint.h>
#include <stdlib.h>
#include <utils/util.h>
#include <gmssl/rand.h>
#include <gmssl/sm2.h>
#include <gmssl/sm3.h>
#include <gmssl/sm4.h>
#include <mavlink2/mavlink_types.h>
#include <crypto/global.h>
#include <crypto/ring_buffer.h>
#include <fsclient.h>

#define HEADER_MAGIC 0xe7
#define PACKAGE_MAX_DATA_LEN 288

enum package_fun {
    REQUEST_CONN = 0x01,
    REQUEST_AUTH = 0x02,
    DATA = 0x04,
    RESPONSE = 0x10,
};

struct encrypt_package {
    uint8_t magic;
    uint8_t len;
    uint8_t fun;
    uint8_t data[PACKAGE_MAX_DATA_LEN];
} __attribute__((packed));

static volatile struct pl011_regs *gcs_uart;

static SM4_KEY sm4_encrypt_key;
static SM4_KEY sm4_decrypt_key;
static uint8_t iv[16];

static uint8_t decrypt_buffer[PACKAGE_MAX_DATA_LEN];
static size_t decrypt_buffer_len;
static uint8_t encrypt_buffer[PACKAGE_MAX_DATA_LEN];
static size_t encrypt_buffer_len;

static struct encrypt_package package;

void *uart_reg_translate_paddr(uintptr_t paddr, size_t size);

static uint8_t read_byte()
{
    while(ring_buffer_is_empty(recv_buf))
        gcs_ready_wait();
    
    return ring_buffer_get(recv_buf);
}

static uint8_t read_chiper_package(struct encrypt_package* package) {
    assert(package);
    uint8_t len;
    uint8_t fun;

    while(read_byte() != HEADER_MAGIC);
    len = read_byte();
    fun = read_byte();
    if (fun & (fun - 1) != 0) 
        goto error;

    for(int i = 0; i < len; i++)
        package->data[i] = read_byte();

    package->magic = HEADER_MAGIC;
    package->len = len;
    package->fun = fun;

error:
    return fun;
}

static inline void send_response(struct encrypt_package* package) {
    assert(package);
    
    for(int i = 0; i < package->len + 3; i++)
        uart_putchar(gcs_uart, ((uint8_t*)package)[i]);
}

static void create_response(struct encrypt_package* package, uint8_t* data, uint8_t len) {
    assert(package);

    package->magic = HEADER_MAGIC;
    package->len = len;
    package->fun = RESPONSE;
    for(int i = 0; i < len; i++)
        package->data[i] = data[i];
}

static int create_connection(struct encrypt_package* package)
{
    SM2_KEY sm2_key_pair;
    SM2_POINT shared_point;
    SM3_KDF_CTX kdf_ctx;
    uint8_t public_key[65];
    uint8_t shared_secret_key[32];
    
    // wait for conn request
    while(read_chiper_package(package) != REQUEST_CONN);

    // establish secure channel
    // create ecdhe key pair
    sm2_key_generate(&sm2_key_pair);
    sm2_ecdh(&sm2_key_pair, package->data, package->len, &shared_point);

    // send public key
    sm2_point_to_uncompressed_octets(&sm2_key_pair.public_key, public_key);
    create_response(package, public_key, 65);
    send_response(package);

    // derive shared secret key
    sm3_kdf_init(&kdf_ctx, 32);
    sm3_kdf_update(&kdf_ctx, (uint8_t*)&shared_point, sizeof(SM2_POINT));
    sm3_kdf_finish(&kdf_ctx, shared_secret_key);

    // save shared secret key
    sm4_set_decrypt_key(&sm4_decrypt_key, shared_secret_key);
    sm4_set_encrypt_key(&sm4_encrypt_key, shared_secret_key);
    memcpy(iv, &shared_secret_key[16], 16);

    // next is an auth request
    if(read_chiper_package(package) != REQUEST_AUTH)
        goto error;

    // decrypt auth request
    sm4_cbc_padding_decrypt(&sm4_decrypt_key, iv, package->data, package->len, decrypt_buffer, &decrypt_buffer_len);

    // read the client public key pem
    char *client_public_key_path = (char*)decrypt_buffer;
    client_public_key_path[decrypt_buffer_len] = '\0';
    FILE *fp = fopen(client_public_key_path, "r");
    if(!fp) {
        goto error;
        ZF_LOGE("[decrypt]: can not open client public key file: %s", client_public_key_path);
    }

    memset(&sm2_key_pair, 0, sizeof(SM2_KEY));
    if(sm2_public_key_info_from_pem(&sm2_key_pair, fp) < 0)
        goto error;

    fclose(fp);
    uint32_t random_num = rng_rand_num();
    sm2_encrypt(&sm2_key_pair, (uint8_t*)&random_num, sizeof(uint32_t), decrypt_buffer, &decrypt_buffer_len);
    sm4_cbc_padding_encrypt(&sm4_encrypt_key, iv, decrypt_buffer, decrypt_buffer_len, encrypt_buffer, &encrypt_buffer_len);
    create_response(package, encrypt_buffer, encrypt_buffer_len);
    send_response(package);

    if(read_chiper_package(package) != REQUEST_AUTH)
        goto error;

    // decrypt auth request
    sm4_cbc_padding_decrypt(&sm4_decrypt_key, iv, package->data, package->len, decrypt_buffer, &decrypt_buffer_len);

    // compare digest
    uint8_t temp[36];
    *(uint32_t*)&temp[0] = random_num;
    memcpy(&temp[4], shared_secret_key, 32);
    sm3_digest(temp, 36, encrypt_buffer);

    if(memcmp(decrypt_buffer, encrypt_buffer, 32) != 0)
        goto error;

    // send auth response
    create_response(package, NULL, 0);
    send_response(package);

    return 0;

error:
    return -1;
}

char buffer[1042];
int run()
{
    gcs_uart = uart_reg_translate_paddr(GCS_UART_PADDR, 0x200);
    while(create_connection(&package));
    while(1) {
        read_chiper_package(&package);
        sm4_cbc_padding_decrypt(&sm4_decrypt_key, iv, package.data, package.len, decrypt_buffer, &decrypt_buffer_len);
        printf("[decrypt]: recieve data:");
        for(int i = 0; i < decrypt_buffer_len; i++)
            printf("%c", decrypt_buffer[i]);
        printf("\n");
    }
    return 0;
}

extern void* fs_buf;
void pre_init(void) {
    uint32_t seed = rng_rand_num();
    gmssl_rand_seed_init(seed);
    ZF_LOGI("[decrypt]: get initial random seed: 0x%08x", seed);
    
    install_fileserver(FILE_SERVER_INTERFACE(fs));
}