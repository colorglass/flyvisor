### ~4% package loss rate (bad crc)
#### direct connect
fc --> cp2102 --> win11 --> gcs, with 0% package loss

#### rpi receive
The rpi only receives data and get the package loss
##### interrupt
using interrupt to receive data, fc --> uart5 (pl011), with ~4% bad crc.
##### polling

##### 8250

### rpi gpu mailbox
- bare metal: ok
- linux api: ok
- uboot -> bare metal: fail
- uboot -> linux api: ok