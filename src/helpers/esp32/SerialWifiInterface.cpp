#include "SerialWifiInterface.h"
#include <WiFi.h>

void WiFiEvent(WiFiEvent_t event);

IPAddress serverIp(192, 168, 1, 1);
IPAddress NMask(255, 255, 255, 0);

#define PWD "mypasswd"
#define SSID "myssid"

void SerialWifiInterface::begin() {
  IPAddress IP;
  Serial.println("Setting up wifi network : " + String(SSID));
 
  // Connect to Wi-Fi network with SSID and password
  Serial.println("Setting AP (Access Point)…");
  // Remove the password parameter, if you want the AP (Access Point) to be open
  WiFi.mode(WIFI_AP);
  WiFi.softAP(SSID, PWD);
  Serial.println("Wait 100 ms for AP_START...");
  delay(100);
 
  Serial.println("Set softAPConfig");
  WiFi.softAPConfig(serverIp, serverIp, NMask);
 
  IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  server.begin();
}

// ---------- public methods

void SerialWifiInterface::enable() { 
  if (_isEnabled) return;

  _isEnabled = true;
  clearBuffers();
}

void SerialWifiInterface::disable() {
  _isEnabled = false;
}

size_t SerialWifiInterface::writeFrame(const uint8_t src[], size_t len) {
  if (len > MAX_FRAME_SIZE) {
    Serial.printf("writeFrame(), frame too big, len=%d\n", len);
    return 0;
  }

  if (deviceConnected && len > 0) {
    if (send_queue_len >= FRAME_QUEUE_SIZE) {
      Serial.println("writeFrame(), send_queue is full!");
      return 0;
    }

    send_queue[send_queue_len].len = len;  // add to send queue
    memcpy(send_queue[send_queue_len].buf, src, len);
    send_queue_len++;

    return len;
  }
  return 0;
}

#define  SER_WRITE_MIN_INTERVAL   20


bool SerialWifiInterface::isWriteBusy() const {
  return millis() < _last_write + SER_WRITE_MIN_INTERVAL;   // still too soon to start another write?
}


size_t SerialWifiInterface::checkRecvFrame(uint8_t dest[]) {
  if (isWriteBusy())
    return 0;

  if (!client) client = server.available();

  if (client.connected()) {
    if (!deviceConnected) {
      Serial.println("Got connexion");
      deviceConnected = true;
    }
  } else {
    if (deviceConnected) {
      deviceConnected = false;
      Serial.println("Disconnected");
    }
  }

  if (deviceConnected) {
    if (send_queue_len > 0) {   // first, check send queue
      _last_write = millis();
      client.write(send_queue[0].buf, send_queue[0].len);
      send_queue_len--;
      for (int i = 0; i < send_queue_len; i++) {   // delete top item from queue
        send_queue[i] = send_queue[i + 1];
      }
    } else {
      int len = client.available();
      if (len > 0) {
        client.readBytes(dest, len);
        return len;
      }
    }
  }

  return 0;
}

bool SerialWifiInterface::isConnected() const {
  return deviceConnected;  //pServer != NULL && pServer->getConnectedCount() > 0;
}