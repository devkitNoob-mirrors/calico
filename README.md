# calico

Calico is a system support library currently focused on the Nintendo DS(i). It provides operating system-like facilities for homebrew applications, and serves as a new foundation for libnds (and DS homebrew in general). Its main features include:

- Multitasking system, providing threads (inspired by Horizon). Scheduling is preemptive and based on multiple priority levels, similar to real-time operating systems.
- Synchronization primitives: mutex, condition variable, mailbox.
- Interrupt and timed event handling.
- Low-level support bits for integration with devkitARM (crt0, linker scripts, syscall implementations).
- Message passing between the two processors (ARM9 and ARM7).
- New device drivers for ARM7 peripherals, with programming interfaces accessible from the ARM9:
	- Power management, including sleep mode and application lifecycle management
	- Touch screen (both DS mode and DSi mode)
	- Real-time clock
	- Audio and microphone
	- Storage devices, including DLDI and DSi SD/eMMC
	- Wireless communications (Mitsumi/NDS and Atheros/DSi)
