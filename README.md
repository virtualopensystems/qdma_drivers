# Xilinx DMA IP Reference drivers

## Xilinx QDMA

The Xilinx PCI Express Multi Queue DMA (QDMA) IP provides high-performance direct memory access (DMA) via PCI Express. The PCIe QDMA can be implemented in UltraScale+ devices.

Both the linux kernel driver and the DPDK driver can be run on a PCI Express root port host PC to interact with the QDMA endpoint IP via PCI Express.

These drivers are like two drivers in one; they are two because one is related to the Physical Function(PF) and the other to the Virtual Function(VF).

This means that the user will need to compile the drivers both on the host and the VMs.

## Getting Started
### On the host
<span style="color:red">The user must have root access (sudo) to install the prerequisites, load the kernel modules, and run the drivers app</span>.

Add the user to the “libvirt” group:
```bash
$ sudo usermod -a -G libvirt $USER
```

Add the following to the user “.bashrc” to autoload Xilinx libraries (needed to flash the bitstream) and to set the libvirt URI (to use the provided VMs without root permissions):

```
source /opt/xilinx/xrt/setup.sh
source /tools/Xilinx/Vitis/2022.1/settings64.sh
export LIBVIRT_DEFAULT_URI=qemu:///system
```

Download, compile and install the QDMA drivers (qdma_pf kernel module) and the PTDR API:
```bash
$ git clone -b everest --single-branch --depth=1 https://code.it4i.cz/everest/qdma_drivers.git
$ cd qdma_drivers/QDMA/linux-kernel/
$ make
$ sudo make install-mods
$ sudo make install-everest-api
```

Load the QDMA driver with:

```bash
$ sudo modprobe qdma_pf
```
 ### On the VM
 Download and compile the QDMA drivers
 ```bash
$ git clone -b everest --single-branch --depth=1 https://code.it4i.cz/everest/qdma_drivers.git
$ cd qdma_drivers/QDMA/linux-kernel/
$ make
$ sudo make install-mods
$ sudo make install-apps 	                #optional
$ sudo make install-everest-api
```

Load the QDMA driver (if not already loaded) with:
```bash
$ sudo modprobe qdma_vf
```

if the VF is attached to the VM, it is possible to run a test app available at the following path: `qdma_drivers/QDMA/linux-kernel/apps/everest` and then run:
```bash
$ sudo ./ptdr-test –i test_files/ptdr_test_in
```

Control the Kernel with the QDMA VF and the PTDR API

The app should include the API like this (the ptdr_api.h header file is under /usr/local/include/everest):
```C
#include <everest/ptdr_api.h>
```

To compile an app and link it with libptdr, run:

```bash
$ gcc app_src.c -L/usr/local/lib/everest -lptdr -o app.bin
```

If the library is not found (e.g. soon after it has been compiled and installed), run:

```bash
$ sudo ldconfig
```

To run the compiled app, use:

```bash
$ sudo LD_LIBRARY_PATH=/usr/local/lib/everest ./app.bin
```
