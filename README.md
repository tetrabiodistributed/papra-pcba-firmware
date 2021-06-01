# papra-pcb-firmware

Firmware is intended to be loaded on this PCB: https://github.com/tetrabiodistributed/PAPRA-PCB

Use the following arduino settings, change the chip to match the one you are using.

![image](https://user-images.githubusercontent.com/57600622/120357153-fc25b500-c2b9-11eb-8603-38b121836ca3.png)

Note: for ATtiny204 and ATtiny214 have 2K flash memory for program space. The preprocessor for TWOKFLASHCHIP must be uncommented prior to compiling. See Line 9.
