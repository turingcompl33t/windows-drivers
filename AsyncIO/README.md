## Asynchronous IO

This repository contains a simple kernel driver and its associated client application(s) that illustrate various asynchronous IO techniques.

### Contents

`AsyncIO/`

This directory contains the kernel driver that implements queueing, buffering, asynchronous IO, and cancellation operations.

`AsyncIoClient/`

This directory contains a simple, synchronous version of the kernel driver client. Because the application is synchronous, multiple active instances are required to interact with the driver in any meaningful way.

`AsyncIoClientV2/` 

This directory contains an improved version of the kernel driver client that implements client-side asynchronous IO operations.
