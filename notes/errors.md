### seL4 failed assertion 'isSchedulable(candidate)'
seL4 failed assertion 'isSchedulable(candidate)' at /host/seL4/kernel/src/kernel/thread.c:371 in function schedule
halting...
Kernel entry via Interrupt, irq 0
#### reason
memory range error
#### solution
set cmake variable `RPI4_MEMORY` to the correct value

### unable to register 8250 port
[    1.840227] bcm2835-aux-uart fe215040.serial: error -ENOSPC: unable to register 8250 port
#### solution
add `8250.nr_uarts=1` to `bootargs` in dtb

### console [ttyS0] disabled
[    1.886904] Serial: 8250/16550 driver, 1 ports, IRQ sharing enabled
[    1.894627] printk: console [ttyS0] disabled
[    1.899164] fe215040.serial: ttyS0 at MMIO 0xfe215040 (irq = 23, base_baud = 62499999) is a 16550
#### solution
change `console=S0` of `bootargs` in dtb to `console=ttyS0,115200`