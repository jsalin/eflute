Electric Flute
==============

Arduino based MIDI controller with capacitive and air flow sensors.

Copyright (C) 2016 Jussi Salin <salinjus@gmail.com> under GPLv3.

Hardware
--------

Use a 50k to 50M resistor between send and receive pins. 10M is good starting point. Receive pin has the sensor foil in parallel. Arduino Nano was chosen as base for the default pin choices because it has suitable size for a flute shaped case and the USB port can be used to act as a USB MIDI device to control some software synthesizer. The air flow sensor is connected to analog input and the value changes on how much air is blown. MPXV7002DP is a good sensor and breakout boards are available for it.

Compiling
---------

Project depends on following libraries, which you have to install first:

* Capacitive Sensing Library, available at http://playground.arduino.cc/Main/CapacitiveSensor?from=Main.CapSense

References that helped
----------------------

* http://www.electronics.dit.ie/staff/tscarff/Music_technology/midi/midi_note_numbers_for_octaves.htm
* http://www.thingiverse.com/thing:1090461
* http://www.squeakysrecorderplayhouse.com/FingeringChart.htm
* http://www.aliexpress.com/snapshot/7403717092.html?orderId=73379505997070
* https://www.midi.org/specifications/item/table-1-summary-of-midi-message
* http://www.instructables.com/id/What-is-MIDI/