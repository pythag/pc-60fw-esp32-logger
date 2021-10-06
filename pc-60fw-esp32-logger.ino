/*    Simple BLE client application that connects to a PC-60FW Viatom Fingertip 
 *    Pulse Oximeter and stores the readings in an SPIFFS file system in flash.
 *    
 *    The BLE code is largely copied from the ESP32 BLE Arduino/BLE_client
 *    example (author unknown).
 *    
 *    This particular code is designed to work with the Olimex ESP32-Gateway
 *    board that has a wired Ethernet connection provided by a Microchip LAN8710A.
 *    
 *    Wired ethernet is required as the ESP32 can't run the Wifi radio and
 *    the BLE radio simultaniously.
 *    
 *    If you only have a regular ESP32 with no wired ethernet then change
 *    ETHENABLED define.
 *  
 */

/*
 *   --------- An important note on building using the ESP32-Gateway board
 *   
 *   Because the ESP32 BLE library is HUGE it is necessary to change the
 *   partitioning scheme when uploading the code to the ESP32. Some boards
 *   allow this via options under the tools menu, sadly the default definition
 *   for the Olimex esp32-gateway board doesn't do this.
 *   
 *   So it is necessary to edit the boards.txt file and add the following
 *   text in order to add the partition options. This should be done in the
 *   esp32-gateway section. 
 *   
 *   On Linux my boards.txt file is located in:
 *   ~/.arduino15/packages/esp32/hardware/esp32/1.0.1/boards.txt
 *   
 *   Once this is done you should set the partition scheme to:
 *   'Minimal SPIFFS (Large APPS with OTA)'.
 *   

esp32-gateway.menu.PartitionScheme.default=Default
esp32-gateway.menu.PartitionScheme.default.build.partitions=default
esp32-gateway.menu.PartitionScheme.minimal=Minimal (2MB FLASH)
esp32-gateway.menu.PartitionScheme.minimal.build.partitions=minimal
esp32-gateway.menu.PartitionScheme.no_ota=No OTA (Large APP)
esp32-gateway.menu.PartitionScheme.no_ota.build.partitions=no_ota
esp32-gateway.menu.PartitionScheme.no_ota.upload.maximum_size=2097152
esp32-gateway.menu.PartitionScheme.huge_app=Huge APP (3MB No OTA)
esp32-gateway.menu.PartitionScheme.huge_app.build.partitions=huge_app
esp32-gateway.menu.PartitionScheme.huge_app.upload.maximum_size=3145728
esp32-gateway.menu.PartitionScheme.min_spiffs=Minimal SPIFFS (Large APPS with OTA)
esp32-gateway.menu.PartitionScheme.min_spiffs.build.partitions=min_spiffs
esp32-gateway.menu.PartitionScheme.min_spiffs.upload.maximum_size=1966080
esp32-gateway.menu.PartitionScheme.fatflash=16M Fat
esp32-gateway.menu.PartitionScheme.fatflash.build.partitions=ffat

 */

/*

MIT License

Copyright (c) 2021 Martin Whitaker

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

// Requires the Time library available from:
// https://github.com/PaulStoffregen/Time

#include "BLEDevice.h"
#include <TimeLib.h>
#include "SPIFFS.h"

/* If you only have a regular ESP32 board (with no wired ethernet) don't
 *  define ETHENABLED. Although you'll have no way of viewing the data
 *  using a web browser and the system will have no way of setting the
 *  clock from NTP, it will still log data to flash.
 */
#define ETHENABLED  1

#ifdef ETHENABLED

#include <ETH.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include "staticpages.h"

static bool eth_connected = false;
WiFiUDP timeudp;
WebServer *httpserver;

#define NTP_PORT 123

IPAddress timeServerIP; 
const char* ntpServerName = "uk.pool.ntp.org";
const int NTP_PACKET_SIZE = 48; 
byte NTPpacketBuffer[NTP_PACKET_SIZE];

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress& address) {
  memset(NTPpacketBuffer, 0, NTP_PACKET_SIZE);
  NTPpacketBuffer[0] = 0b11100011;
  NTPpacketBuffer[1] = 0;
  NTPpacketBuffer[2] = 6;
  NTPpacketBuffer[3] = 0xEC;
  
  // 8 bytes of zero for Root Delay & Root Dispersion
  NTPpacketBuffer[12]  = 49;
  NTPpacketBuffer[13]  = 0x4E;
  NTPpacketBuffer[14]  = 49;
  NTPpacketBuffer[15]  = 52;

  timeudp.beginPacket(address, 123); //NTP requests are to port 123
  timeudp.write(NTPpacketBuffer, NTP_PACKET_SIZE);
  timeudp.endPacket();
}

void gettime() {
  IPAddress timeServerIP; 

  //get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP);
  int retries=5;
  int waitcount;
  int cb;

  do {
    Serial.println("Sending NTP request...");
    sendNTPpacket(timeServerIP); // send an NTP packet to a time server
    // wait to see if a reply is available
    waitcount=10;
    do {
      delay(100);
      cb = timeudp.parsePacket();
    } while ((!cb)&&(--waitcount>0));
  } while((!cb)&&(--retries>0));
  
  if (cb) {
    // We've received a packet, read the data from it
    timeudp.read(NTPpacketBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
    unsigned long highWord = word(NTPpacketBuffer[40], NTPpacketBuffer[41]);
    unsigned long lowWord = word(NTPpacketBuffer[42], NTPpacketBuffer[43]);
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    const unsigned long seventyYears = 2208988800UL;
    unsigned long epoch = secsSince1900 - seventyYears;
    setTime(epoch);
    Serial.println("Time set from NTP response.");
  }
  
}

void ServeReadings() {
  int l;
  int firstrecord=1;

  char buffer[1024];
  char *tbuf;
  
  char devicemac[20];
  int saturation;
  int pulse;
  time_t readingtime;

  File file = SPIFFS.open("/readings.csv", FILE_READ);
  if (!file) {
    return;
  }

  httpserver->setContentLength(CONTENT_LENGTH_UNKNOWN);
  httpserver->send(200, "text/json", graph_json_header);
 
  do {

    memset(devicemac,0,20);
    saturation=0;
    pulse=0;
    readingtime=0;
    
    memset(buffer,0,64);
    l=file.readBytesUntil(',', buffer, 64);
    if (l>0) {
      strcpy(devicemac,buffer);
      memset(buffer,0,64);
      l=file.readBytesUntil(',', buffer, 64);
      if (l>0) {
        saturation=atoi(buffer);
        memset(buffer,0,64);
        l=file.readBytesUntil(',', buffer, 64);
        if (l>0) {
          pulse=atoi(buffer);
          memset(buffer,0,64);
          l=file.readBytesUntil('\n', buffer, 64);
          if (l>0) {
            readingtime=atol(buffer);
            buffer[0]=',';
            if (firstrecord) {
              tbuf=buffer;
              firstrecord=0;
            } else {
              tbuf=buffer+1;
            }
            sprintf(tbuf,"{\"c\":[{\"v\":\"%04d-%02d-%02d %02d:%02d:%02d\",\"f\":null},{\"v\":%d,\"f\":null},{\"v\":%d,\"f\":null}]}",year(readingtime), month(readingtime), day(readingtime), hour(readingtime), minute(readingtime), second(readingtime),saturation, pulse);
            httpserver->sendContent(buffer);
          }        
        }
      } 
    }
  } while (l>0);
  file.close();  
  httpserver->sendContent(graph_json_footer);  
  httpserver->sendContent("");
}

void WiFiEvent(WiFiEvent_t event)
{
  switch (event) {
    case SYSTEM_EVENT_ETH_START:
      Serial.println("ETH Started");
      //set eth hostname here
      ETH.setHostname("pc-60fwproxy");
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:
      Serial.print("ETH MAC: ");
      Serial.print(ETH.macAddress());
      Serial.print(", IPv4: ");
      Serial.print(ETH.localIP());
      if (ETH.fullDuplex()) {
        Serial.print(", FULL_DUPLEX");
      }
      Serial.print(", ");
      Serial.print(ETH.linkSpeed());
      Serial.println("Mbps");
      httpserver->begin();
      eth_connected = true;
      gettime();
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      eth_connected = false;
      break;
    case SYSTEM_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      eth_connected = false;
      break;
    default:
      break;
  }
}

#endif

// The remote service we wish to connect to.
static BLEUUID serviceUUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e");

// The characteristic of the remote service we are interested in.
static BLEUUID    charUUID("6e400003-b5a3-f393-e0a9-e50e24dcca9e");

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;
static BLEScan* pBLEScan;

// These bytes always appear before the sat and hr data on 20 byte BLE packets
static const unsigned char searchheader[5] = { 170, 85, 15, 8, 1 };

int havenewdata=0;
int l_sat=0;
int l_hr=0;
char l_device[128];

time_t lastrecordedtimestamp;

static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify)
{
  int i;
  if (length==20) {
    // Find searchheader in the data stream
    for(i=0;i<20-5;i++) {
      if (memcmp(pData+i,searchheader,5)==0) {
        if (havenewdata==0) {
          l_sat=pData[i+5];
          l_hr=pData[i+6];
          havenewdata=1;
          strcpy(l_device,myDevice->getAddress().toString().c_str());
        }
      }
    }
  }
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    doConnect = false;
    doScan = false;
    Serial.println("onDisconnect");
    // restartscan();
    // Attempts to gracefully resume have eluded me... given up and now reboot the ESP after the device disconnects
    SPIFFS.end();
    delay(1000);
    ESP.restart();
  }
};

bool connectToServer() {
    Serial.print("Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());
    
    BLEClient*  pClient  = BLEDevice::createClient();
    Serial.println(" - Created client");

    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remove BLE Server.
    pClient->connect(myDevice); 
    Serial.println(" - Connected to server");

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(serviceUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our service");

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(charUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our characteristic");

    if(pRemoteCharacteristic->canNotify()) {
      pRemoteCharacteristic->registerForNotify(notifyCallback);
    } else {
      Serial.print("Unable to regiser for notify callback.");
      pClient->disconnect();
      return false;      
    }

    connected = true;
    return true;
}

/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {

      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;

    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks

void restartscan() {
  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 5 seconds.
  Serial.println("Starting scan...");  
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(0, false);
}

void setup() {
  Serial.begin(115200);

  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
  }

  lastrecordedtimestamp=0;
 
#ifdef ETHENABLED
  WiFi.onEvent(WiFiEvent);
  // Have to use these specific pin assignments on the ESP32-Gateway board
  ETH.begin(0, -1, 23, 18, ETH_PHY_LAN8720, ETH_CLOCK_GPIO17_OUT);  

    httpserver = new WebServer(80);
    
    httpserver->on("/", []() {
      httpserver->send(200, "text/html", root_page);
    });
    
    httpserver->on("/eraseconfirm", []() {
      httpserver->send(200, "text/html", eraseconfirm_page);
    });

    httpserver->on("/erase", []() {
      // Redirect back to the main page when done
      SPIFFS.remove("/readings.csv");
      Serial.print("Clearing file from flash\n");
      httpserver->sendHeader("Location", String("/"), true);
      httpserver->send (302, "text/plain", "");
    });

    httpserver->on("/readings", HTTP_GET, ServeReadings);

    xTaskCreatePinnedToCore(
      TaskWebHandler
      ,  "TaskWebHandler"
      ,  3072  // This stack size can be checked & adjusted by reading the Stack Highwater
      ,  NULL
      ,  2  // Priority (3=High, 0=Low)
      ,  NULL 
      ,  0); // Arduino loop is run on core 1, so use core 0 here
    
#else
    File file = SPIFFS.open("/readings.csv", FILE_READ);
    if (file) {
      Serial.println("Readings in flash:");
      while(file.available()) {
        Serial.write(file.read());
      }
      file.close();
    } else {
      Serial.println("No readings currenty stored in flash");
    }
#endif

  Serial.print("File system stats: Used space ");
  Serial.print(SPIFFS.usedBytes());
  Serial.print(" free space ");
  Serial.println(SPIFFS.totalBytes()-SPIFFS.usedBytes());        

  Serial.println("Starting PC-60FW Pulse Oximeter proxy...");

  restartscan();

} // End of setup.

#ifdef ETHENABLED
void TaskWebHandler(void *pvParameters) {
  (void) pvParameters;
  while(1) {
    if (eth_connected) {
      httpserver->handleClient();
    }
    vTaskDelay(50);
  }
}
#endif

void loop() {

  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are 
  // connected we set the connected flag to be true.
  if (doConnect == true) {
    if (connectToServer()) {
      Serial.println("We are now connected to the PC-60FW.");
    } else {
      Serial.println("Failed to connect to BLE device. Rebooting");
      SPIFFS.end();
      delay(1000);
      ESP.restart();      
    }
    doConnect = false;
  }

  if ((!connected)&&(doScan)) {
    delay(500);
    BLEDevice::getScan()->start(0);  // this is just eample to start scan after disconnect, most likely there is better way to do it in arduino
  }

  if (havenewdata) {
    char tempbuf[200];
    sprintf(tempbuf,"%s,%d,%d,%ld",l_device,l_sat,l_hr,now());
    Serial.println(tempbuf);
    // Only write to the file a maximum of once every 60 seconds if left connected
    if ((l_sat>50)&&(l_hr>20)&&(now()-lastrecordedtimestamp>60)) {
      File file = SPIFFS.open("/readings.csv", FILE_APPEND);
      if (file) {
        file.println(tempbuf);
        file.close();

        Serial.print("Record written - used space ");
        Serial.print(SPIFFS.usedBytes());
        Serial.print(" free space ");
        Serial.println(SPIFFS.totalBytes()-SPIFFS.usedBytes());        
      }
      lastrecordedtimestamp=now();
    }

   havenewdata=0;
  }
  
  delay(100); 
} 

