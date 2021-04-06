# Linux Kernel Loadable Module Red-Black Tree Character Device Driver for ASU Fulton's Embedded Systems CSE438 Class
Linux Kernel Loadable Module driver (rb438_drv) for accessing red-black tree (rbtree.h) kernel data structure.  Includes user-space program (assignment3) for demonstrating behavior of driver and trees.

Creates 2 character devices: /dev/rb438_dev1 & /dev/rb438_dev2.  Each represent a red-black tree in kernel space.

Driver supports open(), close(), read(), write(), and ioctl() functionality.

## Usage

use 'make' to build module AND demo interfacing executable

use 'sudo insmod rb438_drv.ko' to insert module into active kernel

demo executable ingests test scripts to interface with character devices.

use './assignment3 <script1> <script2> to test driver. 
Output of scripts will be placed in output1 & output2 files. 
  
#### Script example commands:
<p><pre>
w 1 50 abcd   write payload 'abcd' with key 50 to rb438_dev1 <br>
w 2 40 abcf   write payload 'abcd' with key 50 to rb438_dev2 <br>
w 1 78 cdef   write payload 'abcd' with key 50 to rb438_dev1 <br>
w 2 23 nnnn   write payload 'abcd' with key 50 to rb438_dev2 <br>
d 200         sleep this thread for 200 us <br>
s 1 1         following reads of rb438_dev1 will pop & return payload of min key node from tree<br>
s 2 0         following reads of rb438_dev2 will pop & return payload of max key node from tree<br>
r 1           read from rb438_dev1 -> will return node w/ key: 50, payload: abcd <br>
r 2           read from rb438_dev2 -> will return node w/ key: 40, payload: abcf <br></pre></p>
