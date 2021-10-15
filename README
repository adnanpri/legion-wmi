### Problem

The Legion line of laptops from Lenovo come with the ability to switch power modes between "quiet", "balanced" and "performance." On Windows this feature works as expected and you can clearly see the performance differences across benchmarks. On Linux too this seems to work out of the box, with the exception of the Nvidia GPU. No matter what is attempted, the GPU power limit is set to 80W and it never draws more than 80W, leading to significant performance limitations as compared to Windows.

Sample output of `nvidia-smi` on "performance" mode during benchmarks:
```
+-----------------------------------------------------------------------------+
| NVIDIA-SMI 495.29.05    Driver Version: 495.29.05    CUDA Version: 11.5     |
|-------------------------------+----------------------+----------------------+
| GPU  Name        Persistence-M| Bus-Id        Disp.A | Volatile Uncorr. ECC |
| Fan  Temp  Perf  Pwr:Usage/Cap|         Memory-Usage | GPU-Util  Compute M. |
|                               |                      |               MIG M. |
|===============================+======================+======================|
|   0  NVIDIA GeForce ...  Off  | 00000000:01:00.0  On |                  N/A |
| N/A   60C    P3    80W /  N/A |   1716MiB /  5946MiB |      2%      Default |
|                               |                      |                  N/A |
+-------------------------------+----------------------+----------------------+
```

This has been confirmed a Legion 5 15ACH6H with an AMD CPU (5800H) Nvidia laptop GPU (RTX 3060) and is likely the case on all Legion models equipped with Nvidia GPUs. On Windows the max power draw on the 3 modes appear to be 80W, 95W and 130W respectively.

This driver is an attempt to set the performance mode through the the Windows Management Instrumentation made available by the BIOS (+ an ACPI call to an NVIDIA controller), with the expectation that this call replicates the behavior found on Windows.

However making the same WMI call as the one made with the bundled Lenovo Vantage software on Windows does not seem to remedy the situation (more on this later). NOTE: THIS DRIVER DOES NOT WORK, AND WE NEED MORE EYES ON THIS PROBLEM.


### Disclaimer

I'm quite new to this and you should use this software at your own risk. The terminology/descriptions on this document may also be inaccurate.

### Systems/Environments tried

Kernels: `5.10.x` `5.13.x` `5.14.x`

Distros: `Pop!_OS 21.04` `Ubuntu 20.04` `Garuda (Arch)`

Nvidia Drivers: `460.x` `470.63.x` `470.74.x` `495.29.x`

All combinations have been attempted on both `Hybrid` and `Discrete` graphics modes.

### Underlying issues

The WMI call made to appears to hand over responsiblity of adjusting the TGP of the GPU with a `Notify (NPCF, 0xC0)` call (found on ACPI table DSDT). `\_SB_.NPCF.NPCF` is a method defined in SSDT16.

`NPCF` seems to refer to Nvidia Platform Controller and Framework, and it appears there are no drivers to handle the device `NVDA0820` found in `\_SB_.NPCF`. It's most likely the case that the event triggered by the WMI call is handled by the driver on Windows and which subsequently takes care of the dynamic power limit adjustments. On Windows the device driver responsible for interfacing with the `NVDA0820` device seems to be `nvpcf.sys` that comes with the Nvidia Drivers.

From this, it's likely the case that the Nvidia drivers either do not recognize `NPCF` at all on Linux, or its support is limited.

The ACPI call to `\_SB_.NPCF.NPCF` found in this driver does nothing to change the power limits and the implementation on Windows is likely very different.

### Usage

NOTE: please have `sudo dmesg -wH` running on another terminal to observe changes.

#### Installation

```
sudo make
sudo make install
```

The necessary `modprobe` commands are bundled inside the makefile. Please ignore compiler warnings, the code needs improvements.

#### Calls

The driver exposes procfs entries at `/proc/acpi/legion_acpi` intended to be used by userspace scripts.

To change power modes, use one of the following commands:
```
echo "quiet" | sudo tee /proc/acpi/legion_call
echo "balanced" | sudo tee /proc/acpi/legion_call
echo "perf" | sudo tee /proc/acpi/legion_call
```
The power button LED color should change to indicate that it has worked.

To make the ACPI call to the `NPCF` device, issue the following command:
```
echo "nvidia-npcf" | sudo tee /proc/acpi/legion_call
```

You should see a success message that looks like:
```
[  +0.000122] legion_acpi_call: Call successful: {0x20, 0x05, 0x10, 0x1c, 0x01, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x68, 0x01, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
```
However this changes nothing in terms of GPU power limits.

### Other methods tried

`sudo nvidia-smi -i 0 -pl 100` doesn't work on this hardware.
```
Changing power management limit is not supported for GPU: 00000000:01:00.0.
Treating as warning and moving on.
All done.
```

### Acknowledgements

The driver makes use of code found in

https://github.com/mkottman/acpi_call/blob/master/acpi_call.c
https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/tree/drivers/platform/x86/gigabyte-wmi.c?h=v5.14.1