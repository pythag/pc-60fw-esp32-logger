# pc-60fw-esp32-logger
An Arduino sketch for ESP32 that uses Bluetooth to communicate with a PC-60FW BLE Pulse Oximeter and logs readings in flash.

If you have an ESP32 board with wired Ethernet present (e.g. the Olimex ESP32-Gateway board) then compile with:

'#define ETHENABLED  1'

Then the Ethernet connection will be used to set the internal clock from NTP (to timestamp results) and provide a web interface for viewing the stored data.

On boards with no Ethernet port, the data stored is dumped to the serial console at every boot.

NOTE: As far as I can tell it is not possible to run BLE and Wifi at the same time on an ESP32 due to a shared RF frontend. If anybody knows different please submit a patch / PR!

It's also worth noting that after the BLE device disconnects the ESP32 is rebooted - this is because I couldn't find a way of getting it to reliably listen again for connections. Suggestions to fix this a welcome!

See my other repositories for a UDP streaming version and a LoraWan version.

Also standard disclaimer: Whilst this does touch on medical applications it comes with no warranty. It is intended for educational purposes only!

Build:
* Requires the Time library available from: https://github.com/PaulStoffregen/Time
* See the notes in the comments at the top of the code regarding the partition scheme

Usage:
Simply upload the code then use the PC-60FW as normal (it should connect automatically).
If using on an Olimex ESP32-Gateway bord then to view the results just go to the IP address assigned to the device by DHCP in your web browser.

TODO:
The graph displayed isn't a proper time series scatter graph (i.e. the distance along the X-axis isn't determined by the time elapsed).
Support for multiple Oximeters logging to the same ESP32
Auto-refreshing of the graph

