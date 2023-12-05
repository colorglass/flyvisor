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

### stucked after initialize xhci
[    2.099508] xhci_hcd 0000:01:00.0: enabling device (0000 -> 0002)
[    2.105772] xhci_hcd 0000:01:00.0: xHCI Host Controller
[    2.111111] xhci_hcd 0000:01:00.0: new USB bus registered, assigned bus number 1
OnDemandInstall: Created device-backed memory for addr 0x600000000
[    2.122794] xhci_hcd 0000:01:00.0: hcc params 0x002841eb hci version 0x100 quirks 0x00000e0000000890
[    2.135405] usb usb1: New USB device found, idVendor=1d6b, idProduct=0002, bcdDevice= 5.10
[    2.143775] usb usb1: New USB device strings: Mfr=3, Product=2, SerialNumber=1
[    2.151105] usb usb1: Product: xHCI Host Controller
[    2.156061] usb usb1: Manufacturer: Linux 5.10.92-v8 xhci-hcd
[    2.161890] usb usb1: SerialNumber: 0000:01:00.0
[    2.167316] hub 1-0:1.0: USB hub found
[    2.171291] hub 1-0:1.0: 1 port detected
[    2.175943] xhci_hcd 0000:01:00.0: xHCI Host Controller
[    2.181277] xhci_hcd 0000:01:00.0: new USB bus registered, assigned bus number 2
[    2.188779] xhci_hcd 0000:01:00.0: Host supports USB 3.0 SuperSpeed
[    2.204470] random: fast init done
[    3.111580] random: crng init done
#### solution
change kernel version from 5.10 to 6.1

### sd card initialising failed
[    2.922529] mmc1: ADMA error: 0x02000000
[    2.926657] mmc1: error -5 whilst initialising SD card
#### reason
bcm2711 in B0 step has the emmc2bus dma-ranges <0x00 0xc0000000 0x00 0x00000000 0x40000000>; while
in C0 step it's changed to dma-ranges <0x00 0x00000000 0x00 0x00000000 0xfc000000>. 
Normally, the raspi firmware will override the range according to the SOC stepping, but here the dtb file is directly passed by vm.
#### sulotion
change dma-ranges of emmc2bus node according to soc stepping.