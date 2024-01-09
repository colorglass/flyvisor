#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <gmssl/sm4.h>
#include <gmssl/sm3.h>
#include <gmssl/sm2.h>

#define HEADER_MAGIC 0xe7
#define PACKAGE_MAX_DATA_LEN 288

enum package_fun
{
    REQUEST_CONN = 0x01,
    REQUEST_AUTH = 0x02,
    DATA = 0x04,
    RESPONSE = 0x10,
};

struct encrypt_package
{
    uint8_t magic;
    uint8_t len;
    uint8_t fun;
    uint8_t data[PACKAGE_MAX_DATA_LEN];
} __attribute__((packed));

int set_interface_attribs(int fd, int speed, int parity)
{
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0)
    {
        printf("error %d from tcgetattr", errno);
        return -1;
    }

    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit chars
    // disable IGNBRK for mismatched speed tests; otherwise receive break
    // as \000 chars
    tty.c_iflag &= ~IGNBRK; // disable break processing
    tty.c_lflag = 0;        // no signaling chars, no echo,
                            // no canonical processing
    tty.c_oflag = 0;        // no remapping, no delays
    tty.c_cc[VMIN] = 0;     // read doesn't block
    tty.c_cc[VTIME] = 5;    // 0.5 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

    tty.c_cflag |= (CLOCAL | CREAD);   // ignore modem controls,
                                       // enable reading
    tty.c_cflag &= ~(PARENB | PARODD); // shut off parity
    tty.c_cflag |= parity;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
    {
        printf("error %d from tcsetattr", errno);
        return -1;
    }
    return 0;
}

void set_blocking(int fd, int should_block)
{
    struct termios tty;
    memset(&tty, 0, sizeof tty);
    if (tcgetattr(fd, &tty) != 0)
    {
        printf("error %d from tggetattr", errno);
        return;
    }

    tty.c_cc[VMIN] = should_block ? 1 : 0;
    tty.c_cc[VTIME] = 5; // 0.5 seconds read timeout

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
        printf("error %d setting term attributes", errno);
}

int wait_for_response(int port, struct encrypt_package *package)
{
    uint8_t buf[PACKAGE_MAX_DATA_LEN + 3];
    int len = 0;
    while (len < 3)
    {
        int n = read(port, buf + len, 3 - len);
        if (n < 0)
        {
            printf("error %d from read: %s", errno, strerror(errno));
            return -1;
        }
        len += n;
    }
    if (buf[0] != HEADER_MAGIC)
    {
        printf("error: invalid magic\n");
        return -1;
    }
    if (buf[2] & (buf[2] - 1) != 0)
    {
        printf("error: invalid fun\n");
        return -1;
    }
    while (len < buf[1] + 3)
    {
        int n = read(port, buf + len, buf[1] + 3 - len);
        if (n < 0)
        {
            printf("error %d from read: %s", errno, strerror(errno));
            return -1;
        }
        len += n;
    }
    memcpy(package, buf, len);
    return buf[2] & RESPONSE ? 0 : -1;
}

SM4_KEY sm4_encrypt_key;
SM4_KEY sm4_decrypt_key;
uint8_t iv[16];

static uint8_t decrypt_buffer[PACKAGE_MAX_DATA_LEN];
static size_t decrypt_buffer_len;
static uint8_t encrypt_buffer[PACKAGE_MAX_DATA_LEN];
static size_t encrypt_buffer_len;
static uint8_t buffer[256];
static size_t buffer_len;

int do_connection(int port)
{
    SM2_KEY sm2_key_pair;
    uint8_t public_key[65];

    sm2_key_generate(&sm2_key_pair);
    sm2_point_to_uncompressed_octets(&sm2_key_pair.public_key, public_key);

    struct encrypt_package *package;
    package = (struct encrypt_package *)malloc(sizeof(struct encrypt_package));
    package->magic = HEADER_MAGIC;
    package->len = 65;
    package->fun = REQUEST_CONN;
    memcpy(package->data, public_key, 65);

    write(port, (void *)package, package->len + 3);
    if (wait_for_response(port, package))
    {
        printf("error: connection failed\n");
        return -1;
    }

    uint8_t shared_secret_key[32];
    SM2_POINT shared_point;
    SM3_KDF_CTX kdf_ctx;
    sm2_ecdh(&sm2_key_pair, package->data, package->len, &shared_point);

    sm3_kdf_init(&kdf_ctx, 32);
    sm3_kdf_update(&kdf_ctx, (uint8_t *)&shared_point, sizeof(SM2_POINT));
    sm3_kdf_finish(&kdf_ctx, shared_secret_key);

    sm4_set_encrypt_key(&sm4_encrypt_key, shared_secret_key);
    sm4_set_decrypt_key(&sm4_decrypt_key, shared_secret_key);
    memcpy(iv, &shared_secret_key[16], 16);

    printf("connection established\n");

    char *public_key_path = "9ccf9c5d";
    sm4_cbc_padding_encrypt(&sm4_encrypt_key, iv, (uint8_t *)public_key_path, strlen(public_key_path), encrypt_buffer, &encrypt_buffer_len);

    package->magic = HEADER_MAGIC;
    package->len = encrypt_buffer_len;
    package->fun = REQUEST_AUTH;
    memcpy(package->data, encrypt_buffer, encrypt_buffer_len);
    write(port, (void *)package, package->len + 3);

    if (wait_for_response(port, package))
    {
        printf("error: auth failed\n");
        return -1;
    }

    sm4_cbc_padding_decrypt(&sm4_decrypt_key, iv, package->data, package->len, decrypt_buffer, &decrypt_buffer_len);

    SM2_KEY sm2_private_key;
    FILE *fp = fopen("private_key.pem", "r");
    if (!fp)
    {
        printf("error: cannot open private key file\n");
        return -1;
    }
    sm2_private_key_info_decrypt_from_pem(&sm2_private_key, "test_psk", fp);
    fclose(fp);

    sm2_decrypt(&sm2_private_key, decrypt_buffer, decrypt_buffer_len, buffer, &buffer_len);
    if (buffer_len != 4)
    {
        printf("error: invalid auth response\n");
        return -1;
    }
    uint32_t rand_num = *(uint32_t *)buffer;
    printf("get random number: %08x\n", rand_num);

    uint8_t temp[36];
    memcpy(temp, &rand_num, 4);
    memcpy(temp + 4, shared_secret_key, 32);

    uint8_t digest[32];
    sm3_digest(temp, 36, digest);

    sm4_cbc_padding_encrypt(&sm4_encrypt_key, iv, digest, 32, encrypt_buffer, &encrypt_buffer_len);
    package->magic = HEADER_MAGIC;
    package->len = encrypt_buffer_len;
    package->fun = REQUEST_AUTH;
    memcpy(package->data, encrypt_buffer, encrypt_buffer_len);
    write(port, (void *)package, package->len + 3);

    if (wait_for_response(port, package))
    {
        printf("error: auth failed 1\n");
        return -1;
    }

    free(package);
    return 0;
}

char *portname = "/dev/ttyUSB0";
int main()
{
    int fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0)
    {
        printf("error %d opening %s: %s", errno, portname, strerror(errno));
        return -1;
    }
    set_interface_attribs(fd, B115200, 0);
    set_blocking(fd, 0);

    if (do_connection(fd))
    {
        printf("error: connection failed\n");
        return -1;
    }

    char *str = "Hello World!";
    sm4_cbc_padding_encrypt(&sm4_encrypt_key, iv, str, strlen(str), encrypt_buffer, &encrypt_buffer_len);
    struct encrypt_package *package;
    package = (struct encrypt_package *)malloc(sizeof(struct encrypt_package));
    package->magic = HEADER_MAGIC;
    package->len = encrypt_buffer_len;
    package->fun = DATA;
    memcpy(package->data, encrypt_buffer, encrypt_buffer_len);
    write(fd, (void *)package, package->len + 3);
}
