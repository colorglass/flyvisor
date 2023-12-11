### seL4 failed assertion 'isSchedulable(candidate)'
seL4 failed assertion 'isSchedulable(candidate)' at /host/seL4/kernel/src/kernel/thread.c:371 in function schedule
halting...
Kernel entry via Interrupt, irq 0
#### reason
memory range error
#### solution
set cmake variable `RPI4_MEMORY` to the correct value

### unable to register 8250 port
```
[    1.840227] bcm2835-aux-uart fe215040.serial: error -ENOSPC: unable to register 8250 port
```
#### solution
add `8250.nr_uarts=1` to `bootargs` in dtb

### console [ttyS0] disabled
```
[    1.886904] Serial: 8250/16550 driver, 1 ports, IRQ sharing enabled
[    1.894627] printk: console [ttyS0] disabled
[    1.899164] fe215040.serial: ttyS0 at MMIO 0xfe215040 (irq = 23, base_baud = 62499999) is a 16550
```
#### solution
change `console=S0` of `bootargs` in dtb to `console=ttyS0,115200`

### stucked after initialize xhci
```
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
```
#### solution
change kernel version from 5.10 to 6.1

### sd card initialising failed
```
[    2.922529] mmc1: ADMA error: 0x02000000
[    2.926657] mmc1: error -5 whilst initialising SD card
```
#### reason
bcm2711 in B0 step has the emmc2bus dma-ranges <0x00 0xc0000000 0x00 0x00000000 0x40000000>; while
in C0 step it's changed to dma-ranges <0x00 0x00000000 0x00 0x00000000 0xfc000000>. 
Normally, the raspi firmware will override the range according to the SOC stepping, but here the dtb file is directly passed by vm.
#### sulotion
change dma-ranges of emmc2bus node according to soc stepping.

### pci-uio interrupt not enabled 
```
[    8.556658] ------------[ cut here ]------------
[    8.561463] HW irq 32 has invalid type
[    8.589223] WARNING: CPU: 0 PID: 133 at drivers/irqchip/irq-gic.c:1118 gic_irq_domain_translate+0x11c/0x150
[    8.599162] Modules linked in: connection(O+) uio fuse drm drm_panel_orientation_quirks backlight dm_mod ip_tables x_tables ipv6
[    8.610958] CPU: 0 PID: 133 Comm: systemd-modules Tainted: G        W  O       6.1.63-v8 #2
[    8.619439] Hardware name: Raspberry Pi 4 Model B (DT)
[    8.624651] pstate: 60000005 (nZCv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
[    8.631719] pc : gic_irq_domain_translate+0x11c/0x150
[    8.636847] lr : gic_irq_domain_translate+0x11c/0x150
[    8.641973] sp : ffffffc009a636d0
[    8.645333] x29: ffffffc009a636d0 x28: 0000000000000000 x27: 0000000000000000
[    8.652586] x26: 0000000000000000 x25: ffffffc009a63818 x24: ffffff8011d50000
[    8.659837] x23: 0000000000000001 x22: 000000000000001f x21: ffffff8011d50000
[    8.667087] x20: 0000000000000001 x19: ffffffc00903b718 x18: 0000000000000000
[    8.674336] x17: 0000000000000000 x16: 0000000000000000 x15: 0000007fe560b208
[    8.681587] x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
[    8.688836] x11: 0000000000000000 x10: 0000000000001a90 x9 : ffffffc0081474c0
[    8.696086] x8 : ffffffc009a63468 x7 : 0000000000000000 x6 : 0000000000004760
[    8.703336] x5 : ffffffc00950e000 x4 : 0000000000000000 x3 : ffffffc00950e118
[    8.710584] x2 : 0000000000000000 x1 : 0000000000000000 x0 : ffffff801540be00
[    8.717834] Call trace:
[    8.720311]  gic_irq_domain_translate+0x11c/0x150
[    8.725084]  gic_irq_domain_alloc+0x54/0xd0
[    8.729328]  irq_domain_alloc_irqs_locked+0x108/0x3c0
[    8.734459]  irq_create_fwspec_mapping+0x22c/0x380
[    8.739322]  irq_create_of_mapping+0x88/0xc0
[    8.743653]  of_irq_parse_and_map_pci+0xfc/0x200
[    8.748344]  pci_assign_irq+0x94/0x100
[    8.752155]  pci_device_probe+0x3c/0x130
[    8.756135]  really_probe+0xc4/0x2f0
[    8.759766]  __driver_probe_device+0x80/0x120
[    8.764189]  driver_probe_device+0xe0/0x170
[    8.768435]  __driver_attach+0x9c/0x1b0
[    8.772329]  bus_for_each_dev+0x7c/0xe0
[    8.776221]  driver_attach+0x2c/0x40
[    8.779849]  bus_add_driver+0x15c/0x210
[    8.783742]  driver_register+0x7c/0x140
[    8.787634]  __pci_register_driver+0x54/0x60
[    8.791975]  connector_pci_driver_init+0x30/0x1000 [connection]
[    8.798004]  do_one_initcall+0x60/0x2d0
[    8.801899]  do_init_module+0x50/0x1e0
[    8.805707]  load_module+0x1b08/0x1f60
[    8.809515]  __do_sys_finit_module+0xa8/0x100
[    8.813939]  __arm64_sys_finit_module+0x28/0x40
[    8.818539]  invoke_syscall+0x50/0x120
[    8.822349]  el0_svc_common.constprop.0+0x68/0x130
[    8.827216]  do_el0_svc+0x34/0xd0
[    8.830584]  el0_svc+0x30/0xa0
[    8.833689]  el0t_64_sync_handler+0xf4/0x120
[    8.838026]  el0t_64_sync+0x18c/0x190
[    8.841743] ---[ end trace 0000000000000000 ]---
```
#### reason
When probe a pci device without device node (here is 2 uio devices) and MSI support, the routing will go through and parse the <interrupt-map> property of the pci brige dt node. For example `interrupt-map = <0x800 0x00 0x00 0x01 0x01 0x00 0x00 0x00 0x59 0x04>;`
| 0x800 0x00 0x00 | 0x01 | 0x01 | 0x00 0x00 | 0x00 0x59 0x04 |
|---|---|---|---|---|
| the address of the pci device (in pci's address format) | the pci irq pin numbers of device | the phandle of the parent interrupt controller | the address of the parent irq controller, the address-cells of irq controller is the size of this field | the interrupt data, the interrupt-cells of irq controller is the size of this field |
While the gicv2 dt node don't have the `#address-cells` in the original dtb file, this causes the error irq parsing result.
#### solution
Adding `#address-cells = <0x2>;` to the gicv2 dt node just for irq parsing:
```
		interrupt-controller@40041000 {
			interrupt-controller;
			#interrupt-cells = <0x03>;
			#size-cells = <0x2>;
			#address-cells = <0x2>;
			compatible = "arm,gic-400";
			reg = <0x40041000 0x1000 0x40042000 0x2000 0x40044000 0x2000 0x40046000 0x2000>;
			interrupts = <0x01 0x09 0xf04>;
			phandle = <0x01>;
		};
```
