### Cross virtual device

#### memory map
page 0: 
| offset | size | rw | description |
| --- | --- | --- | --- |
| 0 | 4 | w | write to notify the backed device, used for synchronization |
| 4 | 4 | rw | read to get event numbers, write to zero |
| 8 | 50 | r | read to get device name (string) |

page 1 -> N: data buffer for 4K page aligned


#### execution flow for synchronized communication
1. vm wake from wait
2. vm read write buffer
3. vm send notification
4. vm wait for intrrupt
5. back recive notification
6. back read write buffer
7. back send intrrupt
8. back wait for notification

