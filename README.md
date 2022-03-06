Z80BusAccess with an Arduino

Based on features from my earlier projects the ROMEmu and the Z80Exersizer, this is a way to access memory and I/O from
a working Z80 CPU based computer. It uses a simple DMA technique by applying the BUSREQ* signal (and wait for BUSACK*).
For systems with non-critical timing constraints, the CPU doesn't even have to be reset. For the first testing system, a MicroProfessor MPF-IB it provides an easy way to get programs into memory.

The current hardware is a Arduino Mega 2650 plus shield. The shield is passive, bringing 37 I/O-pins to a Z80 CPU bus.

A more compact version, based on an Arduino Nano (Every) and a port extender is planned.
