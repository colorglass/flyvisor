#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define HEADER_MAGIC 0xe7
#define PACKAGE_MAX_DATA_LEN 288

enum package_fun {
    REQUEST_CONN = 0x01,
    REQUEST_AUTH = 0x02,
    DATA = 0x04,
    RESPONSE_OK = 0x10,
    RESPONSE_ERROR = 0x20,
};

enum package_response_error {
    ERROR_FUN = 0x01,
    ERROR_AUTH = 0x02,
    ERROR_CONN = 0x04,
    ERROR_PARAM = 0x08,
    ERROR_PUBKEY = 0x10,
};

struct chiper_package {
    uint8_t magic;
    uint8_t fun;
    uint16_t len;
    uint8_t data[PACKAGE_MAX_DATA_LEN];
} __attribute__((packed));

static int chiper_package_wait(struct chiper_package *package, uint8_t (*read_byte)(void))
{
    uint8_t fun = 0;
    uint16_t len = 0;

    while (read_byte() != HEADER_MAGIC)
        ;
    fun = read_byte();
    if (fun & (fun - 1) != 0)
        return 0;

    *(uint8_t *)&len = read_byte();
    *((uint8_t *)&len + 1) = read_byte();

    for (int i = 0; i < len; i++)
        package->data[i] = read_byte();

    package->magic = HEADER_MAGIC;
    package->fun = fun;
    package->len = len;

    return fun;
}


static inline void chiper_package_create(struct chiper_package* package, enum package_fun fun, uint8_t* data_buf, size_t data_len)
{
    package->magic = HEADER_MAGIC;
    package->fun = fun;
    package->len = (uint16_t)data_len;
    // data len should less then PACKAGE_MAX_DATA_LEN
    if(package->len > 0)
        memcpy(package->data, data_buf, package->len);
}