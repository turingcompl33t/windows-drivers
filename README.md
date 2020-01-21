### Windows Kernel Drivers

This repository contains a number of experiments in kernel driver development. 

### Driver Descriptions

**AsyncIO**

A simple driver to illustrate various asynchronous IO techniques.

**DelProtect**

A filesystem minifilter to block the deletion of specified files.

**Hello**

The obligatory "Hello World" driver. 

**PriorityBooster**

A kernel driver with a user-space client that allows one to set the thread priority of a target thread to an arbitrary value, regardless of the priority class of the thread's containing process. 

**SingleInstance**

A driver that exercises the device creation routine to ensure that only a single open handle to the device is permitted at any one time.

**SysMon**

A kernel driver with a user-space client that allows one to monitor process events (creation, deletion) using the power of driver process callbacks. 

No relation to the Sysinternals tool of the same name. 

**SysmonV2**

An updated version of the system monitoring driver with extended functionality. Currently incomplete.

**VerifierTest**

A driver to exercise the functionality of Driver Verifier.
