#include <camkes.h>
#include <stdint.h>
#include <utils/util.h>

#define RNG_CTRL_OFFSET					0x00
#define RNG_CTRL_RNG_RBGEN_MASK				0x00001FFF
#define RNG_CTRL_RNG_RBGEN_ENABLE			0x00000001

#define RNG_SOFT_RESET_OFFSET				0x04
#define RNG_SOFT_RESET					0x00000001

#define RBG_SOFT_RESET_OFFSET				0x08
#define RBG_SOFT_RESET					0x00000001

#define RNG_INT_STATUS_OFFSET				0x18
#define RNG_INT_STATUS_MASTER_FAIL_LOCKOUT_IRQ_MASK	0x80000000
#define RNG_INT_STATUS_STARTUP_TRANSITIONS_MET_IRQ_MASK	0x00020000
#define RNG_INT_STATUS_NIST_FAIL_IRQ_MASK		0x00000020
#define RNG_INT_STATUS_TOTAL_BITS_COUNT_IRQ_MASK	0x00000001

#define RNG_FIFO_DATA_OFFSET				0x20

#define RNG_FIFO_COUNT_OFFSET				0x24
#define RNG_FIFO_COUNT_RNG_FIFO_COUNT_MASK		0x000000FF

static volatile uint32_t* rng_reg = NULL; 

uint32_t rng_rand_num(void) {
    if((rng_reg[6] & (RNG_INT_STATUS_MASTER_FAIL_LOCKOUT_IRQ_MASK | RNG_INT_STATUS_NIST_FAIL_IRQ_MASK)) != 0) {
        ZF_LOGE("RNG read error");
        return 0;
    }

    int c = 1000000;
    while((rng_reg[9] & RNG_FIFO_COUNT_RNG_FIFO_COUNT_MASK) == 0) {
        if(c-- == 0) {
            ZF_LOGE("RNG read timeout");
            return 0;
        }
    }

    return rng_reg[8];
}

void rng__init(void) {
    rng_reg = (volatile uint32_t*)dtb_0;
    uint32_t val;

    // reset RNG
    val = rng_reg[0];
    val &= ~RNG_CTRL_RNG_RBGEN_MASK;
    val |= RNG_CTRL_RNG_RBGEN_ENABLE;
    rng_reg[0] = val;

    // clear irq
    rng_reg[6] = 0xFFFFFFFFUL;
}