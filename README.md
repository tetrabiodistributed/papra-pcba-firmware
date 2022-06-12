# papra-pcb-firmware

Firmware is intended to be loaded on this PCB: https://github.com/tetrabiodistributed/PAPRA-PCB

In the code, set the #define for the PCB revision you are using, see lines 16-18. 

Use the following arduino settings, change the chip to match the one you are using.

![image](https://user-images.githubusercontent.com/57600622/173251179-0eeb292c-e689-4997-9e30-4efc9564d7ec.png)

Note: for ATtiny204 and ATtiny214 have 2K flash memory for program space. The preprocessor for TWOKFLASHCHIP must be uncommented prior to compiling. See Line 9.
