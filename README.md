# Linux Kernel Loadable Module Red-Black Tree Character Device Driver
Linux Kernel Loadable Module driver for accessing red-black tree (rbtree.h) kernel data structure.

Creates 2 character devices: /dev/rb438

Driver supports open, close, read, write, and ioctl functionality.

## Usage

use 'make' to build module AND demo interfacing executable

use 'sudo insmod rb438_drv.ko' to insert module into active kernel

demo executable igests test scripts to interface with character devices.

use './assignment3 <script1> <script2> to test driver. 
Output of scripts will be placed in output1 & output2 files. 
