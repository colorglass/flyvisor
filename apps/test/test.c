#include <camkes.h>
#include <camkes/allocator.h>
#include <sel4/sel4.h>
#include <stdint.h>
#include <simple/simple.h>

simple_t camkes_simple;

int run() {
    seL4_CPtr cnode = simple_get_cnode(&camkes_simple);
    seL4_CPtr vspace = simple_get_nth_cap(&camkes_simple, seL4_CapInitThreadPD);
    camkes_make_simple(&camkes_simple);
    seL4_CPtr tcb0 = camkes_alloc(seL4_TCBObject, 0, 0);
    seL4_CPtr ipc_buf0 = camkes_alloc(seL4_UntypedObject, 0x1000, 0);
    seL4_ARM_Page_Map(ipc_buf0, vspace, NULL, )
}