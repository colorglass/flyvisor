#include <camkes.h>
#include <stdint.h>
#include <stdlib.h>
#include <utils/util.h>
#include <gmssl/rand.h>
#include <gmssl/sm2.h>
#include <gmssl/sm3.h>
#include <gmssl/sm4.h>
#include <mavlink2/checksum.h>
#include <mavlink2/mavlink_types.h>
#include <crypto/global.h>
#include <crypto/ring_buffer.h>
#include <fsclient.h>

static volatile struct pl011_regs *gcs_uart;
static volatile struct pl011_regs *fc_uart;

static SM4_KEY sm4_encrypt_key;
static SM4_KEY sm4_decrypt_key;
static uint8_t iv[16];

static uint8_t buffer[4096];
static uint8_t decrypt_buffer[PACKAGE_MAX_DATA_LEN];
static size_t decrypt_buffer_len;
static struct encrypt_package package;

void *uart_reg_translate_paddr(uintptr_t paddr, size_t size);

// block read
static uint8_t read_byte()
{
    while(ring_buffer_is_empty(recv_buf))
        gcs_ready_wait();
    
    return ring_buffer_get(recv_buf);
}

static uint8_t read_chiper_package(struct encrypt_package* package)
{
    assert(package);
    uint8_t len = 0;
    uint8_t fun = 0;
    uint16_t crc = 0;

    while(read_byte() != HEADER_MAGIC);
    len = read_byte();
    fun = read_byte();
    if (fun & (fun - 1) != 0) 
        goto error;

    *(uint8_t*)&crc = read_byte();
    *((uint8_t*)&crc + 1) = read_byte();

    for(int i = 0; i < len; i++)
        package->data[i] = read_byte();

    package->magic = HEADER_MAGIC;
    package->len = len;
    package->fun = fun;
    package->crc = crc;

error:
    return fun;
}

static int verify_package(struct encrypt_package* package)
{
    assert(package);
    uint16_t crc = 0;
    uint16_t crc_cal = 0;

    crc = package->crc;
    package->crc = 0;
    crc_cal = crc_calculate((uint8_t*)package, package->len + 5);
    package->crc = crc;

    return crc == crc_cal;
}

static inline void send_response(struct encrypt_package* package)
{
    assert(package);
    
    for(int i = 0; i < package->len + 5; i++)
        uart_putchar(gcs_uart, ((uint8_t*)package)[i]);
}

static void create_response(struct encrypt_package* package, uint8_t* data, uint8_t len, int error)
{
    assert(package);

    package->magic = HEADER_MAGIC;
    package->len = len;
    package->fun = error? RESPONSE_ERROR: RESPONSE_OK;
    package->crc = 0;
    for(int i = 0; i < len; i++)
        package->data[i] = data[i];

    uint16_t crc_cal = crc_calculate((uint8_t*)package, package->len + 5);
    package->crc = crc_cal;
    
}

static int create_connection(struct encrypt_package* package)
{
    SM2_KEY sm2_key_pair;   // temporary sm2 key pairs
    SM2_POINT ecdh_shared_point;
    uint8_t ecdh_public_key[65];

    SM3_KDF_CTX kdf_ctx;
    uint8_t shared_secret_key[32];

    uint8_t reason;
    
    /************************ secure connection stage *******************************/
    // wait for conn request, blocking loop
    while(read_chiper_package(package) != REQUEST_CONN);
    if(!verify_package(package)) {
        ZF_LOGE("[decrypt]: conn request crc error");
        reason = ERROR_CRC;
        goto error_conn;
    }

    if(package->len != 65) {
        ZF_LOGE("[decrypt]: invalid conn request");
        reason = ERROR_CONN;
        goto error_conn;
    }

    // assert the follow funcation is all success

    // establish secure channel
    // create ecdhe key pair
    sm2_key_generate(&sm2_key_pair);
    sm2_ecdh(&sm2_key_pair, package->data, package->len, &ecdh_shared_point);

    // send public key
    sm2_point_to_uncompressed_octets(&sm2_key_pair.public_key, ecdh_public_key);
    create_response(package, ecdh_public_key, 65, 0);
    send_response(package);

    // derive shared secret key
    sm3_kdf_init(&kdf_ctx, 32);
    sm3_kdf_update(&kdf_ctx, (uint8_t*)&ecdh_shared_point, sizeof(SM2_POINT));
    sm3_kdf_finish(&kdf_ctx, shared_secret_key);

    // save shared secret key
    sm4_set_decrypt_key(&sm4_decrypt_key, shared_secret_key);
    sm4_set_encrypt_key(&sm4_encrypt_key, shared_secret_key);
    memcpy(iv, &shared_secret_key[16], 16);



    /************************ authentication stage *******************************/
    // next is an auth request with the client public key path
    if(read_chiper_package(package) != REQUEST_AUTH) {
        ZF_LOGE("[decrypt]: invalid auth request 0");
        reason = ERROR_FUN;
        goto error_auth;
    }

    // decrypt auth request
    sm4_cbc_padding_decrypt(&sm4_decrypt_key, iv, package->data, package->len, decrypt_buffer, &decrypt_buffer_len);

    // read the client public key pem
    char *client_public_key_path = (char*)decrypt_buffer;
    client_public_key_path[decrypt_buffer_len] = '\0';
    FILE *fp = fopen(client_public_key_path, "r");
    if(!fp) {
        reason = ERROR_PUBKEY;
        ZF_LOGE("[decrypt]: can not open client public key file: %s", client_public_key_path);
        goto error_auth;
    }
    memset(&sm2_key_pair, 0, sizeof(SM2_KEY));
    sm2_public_key_info_from_pem(&sm2_key_pair, fp);
    fclose(fp);

    // send public key encrypted random number
    uint32_t random_num = rng_rand_num();
    size_t buffer_len = 0;
    sm2_encrypt(&sm2_key_pair, (uint8_t*)&random_num, sizeof(uint32_t), buffer, &buffer_len);

    uint8_t* encrypt_buffer = buffer + buffer_len; // this should not be out of buffer
    size_t encrypt_buffer_len = 0;
    sm4_cbc_padding_encrypt(&sm4_encrypt_key, iv, buffer, buffer_len, encrypt_buffer, &encrypt_buffer_len);
    create_response(package, encrypt_buffer, encrypt_buffer_len, 0);
    send_response(package);

    // next is an auth request with the digest
    if(read_chiper_package(package) != REQUEST_AUTH) {
        ZF_LOGE("[decrypt]: invalid auth request 1");
        reason = ERROR_FUN;
        goto error_auth;
    }

    // decrypt auth request
    sm4_cbc_padding_decrypt(&sm4_decrypt_key, iv, package->data, package->len, decrypt_buffer, &decrypt_buffer_len);

    // compare digest
    *(uint32_t*)&buffer[0] = random_num;
    memcpy(&buffer[4], shared_secret_key, 32);
    uint8_t* digest = buffer + 36;
    sm3_digest(buffer, 36, digest);

    if(memcmp(decrypt_buffer, digest, 32) != 0) {
        ZF_LOGE("[decrypt]: auth failed");
        reason = ERROR_AUTH;
        goto error_auth;
    }

    // send auth response
    create_response(package, NULL, 0, 0);
    send_response(package);

    return 0;

error_auth:
    memset(&sm4_encrypt_key, 0, sizeof(SM4_KEY));
    memset(&sm4_decrypt_key, 0, sizeof(SM4_KEY));
    memset(iv, 0, 16);
error_conn:
    create_response(package, &reason, 1, 1);
    send_response(package);
    return -1;
}

int run()
{
reset:
    printf("[decrypt]: waiting for connection...\n");
    while(create_connection(&package));
    // send sm4 key to encryption component
    while(1) {
        // all the mavlink package was encrypted
        if(read_chiper_package(&package) != DATA) {
            ZF_LOGE("[decrypt]: invalid data package");
            continue;
        }

        if(!verify_package(&package)) {
            ZF_LOGE("[decrypt]: data package crc error");
            continue;
        }

        sm4_cbc_padding_decrypt(&sm4_decrypt_key, iv, package.data, package.len, decrypt_buffer, &decrypt_buffer_len);
        decrypt_buffer[decrypt_buffer_len] = '\0';
        
        if(strcmp((char*)decrypt_buffer, "reset") == 0) {
            memset(&sm4_encrypt_key, 0, sizeof(SM4_KEY));
            memset(&sm4_decrypt_key, 0, sizeof(SM4_KEY));
            memset(iv, 0, 16);
            goto reset;
        }
        printf("[decrypt]: receive mavlink package:");
        for(int i = 0; i < decrypt_buffer_len; i++)
            printf(" %02x", decrypt_buffer[i]);
        printf("\n");
        printf("%s\n", decrypt_buffer);
            // uart_putchar(fc_uart, decrypt_buffer[i]);
    }

    return 0;
}

extern void* fs_buf;
void pre_init(void)
{
    uint32_t seed = rng_rand_num();
    gmssl_rand_seed_init(seed);
    ZF_LOGI("[decrypt]: get initial random seed: 0x%08x", seed);
    
    install_fileserver(FILE_SERVER_INTERFACE(fs));
    gcs_uart = uart_reg_translate_paddr(GCS_UART_PADDR, 0x200);
    fc_uart = uart_reg_translate_paddr(FC_UART_PADDR, 0x200);
}