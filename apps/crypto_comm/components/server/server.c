#include <camkes.h>
#include <camkes/io.h>
#include <sel4/sel4.h>
#include <simple/simple.h>
#include <simple/simple_helpers.h>
#include <allocman/allocman.h>
#include <allocman/bootstrap.h>
#include <allocman/vka.h>
#include <vka/vka.h>
#include <vspace/vspace.h>
#include <sel4utils/vspace.h>
#include <sel4utils/helpers.h>
#include <sel4utils/thread.h>

#include <fsclient.h>

#include <stdint.h>

#include <crypto/chiper_package.h>
#include <crypto/ring_buffer.h>
#include <crypto/global.h>
#include <rpi4-dma.h>

#include <gmssl/rand.h>
#include <gmssl/sm2.h>
#include <gmssl/sm3.h>
#include <gmssl/sm4.h>

#include <mavlink2/ardupilotmega/mavlink.h>


#define SERVER_PRIORITY 253

#define ALLOCATOR_MEMPOOL_SIZE 0x1000000
static char allocator_mempool[ALLOCATOR_MEMPOOL_SIZE] ALIGN(PAGE_SIZE_4K) SECTION("align_12bit");

void *uart_reg_translate_paddr(uintptr_t paddr, size_t size);
extern void camkes_make_simple(simple_t *simple);
extern void gcs_ready_wait(void);
extern void fc_ready_wait(void);


simple_t simple;
vka_t vka;
vspace_t vspace;
allocman_t *allocman;
sel4utils_alloc_data_t alloc_data;

sel4utils_thread_config_t thread_config;
sel4utils_thread_t decrypt_thread;
sel4utils_thread_t encrypt_thread;
sel4utils_checkpoint_t decrypt_checkpoint;
sel4utils_checkpoint_t encrypt_checkpoint;

seL4_CPtr root_cnode;
seL4_CPtr root_vspace;
seL4_CPtr root_tcb;


static volatile struct pl011_regs *gcs_uart;
static volatile struct pl011_regs *fc_uart;
extern ring_buffer_t *gcs_msg_buf;
extern ring_buffer_t *fc_msg_buf;
extern void *fs_buf;

static SM4_KEY sm4_encrypt_key;
static SM4_KEY sm4_decrypt_key;
static uint8_t sm4_iv[16];

static struct dma_channel dma_channel0;
static struct dma_channel dma_channel1;
static struct dma_uart_config gcs_uart_config;
static struct dma_uart_config fc_uart_config;

static inline uint8_t ring_read_gcs() 
{
    while(ring_buffer_is_empty(gcs_msg_buf))
        gcs_ready_wait();

    return ring_buffer_get(gcs_msg_buf);
}

static inline uint8_t ring_read_fc()
{
    while(ring_buffer_is_empty(fc_msg_buf))
        fc_ready_wait();

    return ring_buffer_get(fc_msg_buf);
}

static inline void chiper_package_send_dma(struct chiper_package* package)
{
    dma_transform_send_uart(&dma_channel0, package, package->len + 4, &gcs_uart_config);
}

static inline void chiper_package_send(struct chiper_package* package)
{
    for(int i = 0; i < package->len + 4; i++)
        uart_putchar(gcs_uart, ((uint8_t*)package)[i]);
}

static int chiper_package_read(struct chiper_package *package)
{
    uint8_t fun = 0;
    uint16_t len = 0;

    while (ring_read_gcs() != HEADER_MAGIC)
        ;
    fun = ring_read_gcs();
    if (fun & (fun - 1) != 0)
        return 0;

    *(uint8_t *)&len = ring_read_gcs();
    *((uint8_t *)&len + 1) = ring_read_gcs();

    for (int i = 0; i < len; i++)
        package->data[i] = ring_read_gcs();

    package->magic = HEADER_MAGIC;
    package->fun = fun;
    package->len = len;

    return fun;
}

void encryption_server(void *arg0, void *arg1, void *ipc_buf) 
{
    int result = 0;
    struct chiper_package chiper_package = {0};
    uint8_t plain_buf[PACKAGE_MAX_DATA_LEN] = {0};
    size_t plain_len = 0;
    uint8_t chiper_buf[PACKAGE_MAX_DATA_LEN] = {0};
    size_t chiper_len = 0;

    mavlink_message_t mav_msg = {0};
    mavlink_status_t mav_status = {0};
    mavlink_channel_t mav_chan = MAVLINK_COMM_0;

    ZF_LOGI("[encryption_server]: start encryption_server");

    // discard the outdata mavlink message
    ring_buffer_init(fc_msg_buf, MSG_BUFFER_SIZE);

    while(1) {
        do{
            uint8_t c = ring_read_fc();
            result = mavlink_parse_char(mav_chan, c, &mav_msg, &mav_status);
        }while(result != MAVLINK_FRAMING_OK);

        plain_len = mavlink_msg_to_send_buffer(plain_buf, &mav_msg);
        if(plain_len == 0) {
            ZF_LOGI("[encryption_server]: mavlink_msg_to_send_buffer failed");
            continue;
        }

#ifdef DEBUG_SERVER
        ZF_LOGI("[encryption_server]: FC msg: %s", mavlink_get_message_info(&mav_msg)->name);
#endif

        result = sm4_cbc_padding_encrypt(&sm4_encrypt_key, sm4_iv, plain_buf, plain_len, chiper_buf, &chiper_len);
        if(result != 1) {
            ZF_LOGE("[encryption_server]: encrypt failed for package with mavlink msgid: %d", mav_msg.msgid);
            continue;
        }

        chiper_package_create(&chiper_package, DATA, chiper_buf, chiper_len);
        
        chiper_package_send_dma(&chiper_package);
    }
}

void decryption_server(void *arg0, void *arg1, void *ipc_buf)
{
    int result = 0;
    struct chiper_package chiper_package = {0};
    uint8_t plain_buf[PACKAGE_MAX_DATA_LEN] = {0};
    size_t plain_len = 0;

#ifdef DEBUG_SERVER
        mavlink_message_t mav_msg = {0};
        mavlink_status_t mav_status = {0};
        mavlink_channel_t mav_chan = MAVLINK_COMM_1;
#endif

    ZF_LOGI("[decryption_server]: start decryption_server");

    while(1) {
        result = chiper_package_read(&chiper_package);
        if(result != DATA) {
            ZF_LOGI("[decryption_server]: unexpected package with fun %d", result);
            continue;
        }

        result = sm4_cbc_padding_decrypt(&sm4_decrypt_key, sm4_iv, chiper_package.data, chiper_package.len, plain_buf, &plain_len);
        if(result != 1) {
            ZF_LOGE("[decryption_server]: decrypt failed for package with data len: %d", chiper_package.len);
            continue;
        }

#ifdef DEBUG_SERVER
        uint8_t result = 0;
        for(int i=0; i < plain_len; i++) {
            uint8_t result = mavlink_parse_char(mav_chan, plain_buf[i], &mav_msg, &mav_status);
            if(result == MAVLINK_FRAMING_OK) {
                ZF_LOGI("[decryption_server]: GCS msg: %s", mavlink_get_message_info(&mav_msg)->name);
            }
        }
#endif

        // for(size_t i = 0; i < plain_len; i++)
        //     uart_putchar(fc_uart, plain_buf[i]);
        dma_transform_send_uart(&dma_channel1, plain_buf, plain_len, &fc_uart_config);
    }
}

static int create_connection(void)
{
    SM2_KEY sm2_key_pair; // temporary sm2 key pairs
    SM2_POINT ecdh_shared_point;
    uint8_t ecdh_public_key[65];

    SM3_KDF_CTX kdf_ctx;
    uint8_t shared_secret_key[32];

    uint8_t reason;

    struct chiper_package package = {0};
    uint8_t buffer[512] = {0};
    size_t buffer_len = 0;

    /************************ secure connection stage *******************************/
    // wait for conn request, blocking loop
    while (chiper_package_read(&package) != REQUEST_CONN)
        ;

    if (package.len != 65)
    {
        ZF_LOGE("[connection]: invalid conn request");
        reason = ERROR_CONN;
        goto error_conn;
    }

    ZF_LOGI("[connection]: conn request received");

    // assert the follow funcation is all success

    // establish secure channel
    // create ecdhe key pair
    sm2_key_generate(&sm2_key_pair);
    sm2_ecdh(&sm2_key_pair, package.data, package.len, &ecdh_shared_point);

    // send public key
    sm2_point_to_uncompressed_octets(&sm2_key_pair.public_key, ecdh_public_key);
    chiper_package_create(&package, RESPONSE_OK, ecdh_public_key, 65);
    chiper_package_send(&package);

    ZF_LOGI("[connection]: public key sent");

    // derive shared secret key
    sm3_kdf_init(&kdf_ctx, 32);
    sm3_kdf_update(&kdf_ctx, (uint8_t *)&ecdh_shared_point, sizeof(SM2_POINT));
    sm3_kdf_finish(&kdf_ctx, shared_secret_key);

    // save shared secret key (session)
    sm4_set_decrypt_key(&sm4_decrypt_key, shared_secret_key);
    sm4_set_encrypt_key(&sm4_encrypt_key, shared_secret_key);
    memcpy(sm4_iv, &shared_secret_key[16], 16);

    ZF_LOGI("[connection]: shared secret key established");

    /************************ authentication stage *******************************/
    // next is an auth request with the client public key path
    if (chiper_package_read(&package) != REQUEST_AUTH)
    {
        ZF_LOGE("[connection]: invalid auth request 0");
        reason = ERROR_FUN;
        goto error_auth;
    }

    ZF_LOGI("[connection]: auth request received");

    // decrypt auth request
    sm4_cbc_padding_decrypt(&sm4_decrypt_key, sm4_iv, package.data, package.len, buffer, &buffer_len);

    // read the client public key pem
    char *client_public_key_path = (char *)buffer;
    client_public_key_path[buffer_len] = '\0';
    FILE *fp = fopen(client_public_key_path, "r");
    if (!fp)
    {
        reason = ERROR_PUBKEY;
        ZF_LOGE("[connection]: can not open client public key file: %s", client_public_key_path);
        goto error_auth;
    }
    memset(&sm2_key_pair, 0, sizeof(SM2_KEY));
    sm2_public_key_info_from_pem(&sm2_key_pair, fp);
    fclose(fp);

    ZF_LOGI("[connection]: client public key read");

    // send public key encrypted random number
    uint32_t random_num = rng_rand_num();
    sm2_encrypt(&sm2_key_pair, (uint8_t *)&random_num, sizeof(uint32_t), buffer, &buffer_len);

    uint8_t* chiper_buffer = (uint8_t*)&buffer[buffer_len];
    size_t chiper_buffer_len = 0;
    sm4_cbc_padding_encrypt(&sm4_encrypt_key, sm4_iv, buffer, buffer_len, chiper_buffer, &chiper_buffer_len);
    chiper_package_create(&package, RESPONSE_OK, chiper_buffer, chiper_buffer_len);
    chiper_package_send(&package);

    ZF_LOGI("[connection]: random number sent");

    // next is an auth request with the digest
    if (chiper_package_read(&package) != REQUEST_AUTH)
    {
        ZF_LOGE("[connection]: invalid auth request 1");
        reason = ERROR_FUN;
        goto error_auth;
    }

    ZF_LOGI("[connection]: auth verify request received");

    // decrypt auth request
    sm4_cbc_padding_decrypt(&sm4_decrypt_key, sm4_iv, package.data, package.len, buffer, &buffer_len);

    if(buffer_len != 32) {
        ZF_LOGE("[connection]: invalid auth request 2");
        reason = ERROR_PARAM;
        goto error_auth;
    }

    // compare digest
    uint8_t *origin_concate = (uint8_t *)&buffer[buffer_len];
    *(uint32_t *)&origin_concate[0] = random_num;
    memcpy(&origin_concate[4], shared_secret_key, 32);
    uint8_t *digest = (uint8_t*)&origin_concate[36];
    sm3_digest(origin_concate, 36, digest);

    if (memcmp(buffer, digest, 32) != 0)
    {
        ZF_LOGE("[connection]: auth failed");
        reason = ERROR_AUTH;
        goto error_auth;
    }

    ZF_LOGI("[connection]: auth success");

    // send auth response
    chiper_package_create(&package, RESPONSE_OK, NULL, 0);
    chiper_package_send(&package);

    return 0;

error_auth:
    memset(&sm4_encrypt_key, 0, sizeof(SM4_KEY));
    memset(&sm4_decrypt_key, 0, sizeof(SM4_KEY));
    memset(sm4_iv, 0, 16);
error_conn:
    chiper_package_create(&package, RESPONSE_ERROR, &reason, 1);
    chiper_package_send(&package);
    return -1;
}

static void init_device()
{
    ps_io_ops_t io_ops;
    camkes_io_ops(&io_ops);
    dma_init(&io_ops.dma_manager, &dma_channel0, 0);
    dma_init(&io_ops.dma_manager, &dma_channel1, 1);

    gcs_uart_config.io_bus_addr = 0x7e201800;
    gcs_uart_config.permap_in = UART4_RX;
    gcs_uart_config.permap_out = UART4_TX;

    fc_uart_config.io_bus_addr = 0x7e201a00;
    fc_uart_config.permap_in = UART5_RX;
    fc_uart_config.permap_out = UART5_TX;

    gcs_uart = (volatile struct pl011_regs*)uart_reg_translate_paddr(GCS_UART_PADDR, 0x200);
    fc_uart = (volatile struct pl011_regs*)uart_reg_translate_paddr(FC_UART_PADDR, 0x200);

    uint32_t seed = rng_rand_num();
    gmssl_rand_seed_init(seed);
    ZF_LOGD("[server]: get initial random seed: 0x%08x", seed);
}

int run() {

    init_device();

    camkes_make_simple(&simple);

    root_cnode = simple_get_init_cap(&simple, seL4_CapInitThreadCNode);
    root_vspace = simple_get_init_cap(&simple, seL4_CapInitThreadPD);
    root_tcb = simple_get_init_cap(&simple, seL4_CapInitThreadTCB);

    allocman = bootstrap_use_current_simple(&simple, ALLOCATOR_MEMPOOL_SIZE, allocator_mempool);
    allocman_make_vka(&vka, allocman);
    sel4utils_bootstrap_vspace(&vspace, &alloc_data, root_vspace, &vka, NULL, NULL, NULL);

    install_fileserver(FILE_SERVER_INTERFACE(fs));

    thread_config = thread_config_default(&simple, root_cnode, seL4_NilData, seL4_CapNull, SERVER_PRIORITY - 1);
    sel4utils_configure_thread_config(&vka, &vspace, &vspace, thread_config, &decrypt_thread);
    sel4utils_configure_thread_config(&vka, &vspace, &vspace, thread_config, &encrypt_thread);

    ZF_LOGI("[server]: waiting for connection");
    while(create_connection());
    ZF_LOGI("[server]: connection established");
    // when waiting for a new connection, there may be already a timeout event
    // so we need to clear the signal
    data_timeout_poll();

    sel4utils_start_thread(&decrypt_thread, decryption_server, NULL, NULL, true);
    sel4utils_start_thread(&encrypt_thread, encryption_server, NULL, NULL, true);

    // root thread has more prio than sub threads, so we can save the initial state here 
    sel4utils_checkpoint_thread(&decrypt_thread, &decrypt_checkpoint, false);
    sel4utils_checkpoint_thread(&encrypt_thread, &encrypt_checkpoint, false);
    
    while(1) {
        data_timeout_wait();

        ZF_LOGI("[server]: time out event");

        sel4utils_suspend_thread(&decrypt_thread);
        sel4utils_suspend_thread(&encrypt_thread);

        memset(&sm4_encrypt_key, 0, sizeof(SM4_KEY));
        memset(&sm4_decrypt_key, 0, sizeof(SM4_KEY));
        memset(sm4_iv, 0, 16);

        ZF_LOGI("[server]: waiting for connection");
        while(create_connection());
        ZF_LOGI("[server]: connection established");
        data_timeout_poll();
        
        sel4utils_checkpoint_restore(&decrypt_checkpoint, false, true);
        sel4utils_checkpoint_restore(&encrypt_checkpoint, false, true);
    }
}