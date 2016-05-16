/*
 *  Control WS2812 leds over Wifi with ESP8266
 *  Version 0.1
 *
 *  Uses Websockets
 *  Uses WifiManager for easy Wifi setup
 *  Uses Neopixelbus for controlling LEDs
 *
 *  TODO: Implement controlling pixels
 *  TODO: Support OTA updates
 */

#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <Hash.h>
#include <NeoPixelBus.h>
#include <WebSocketsServer.h>
#include <WiFiManager.h>

/*
 * === Settings ===
 */

//Neopixel related
const uint16_t PixelCount = 7; // this example assumes 4 pixels, making it smaller will cause a failure
const uint8_t PixelPin = 2;  // make sure to set this to the correct pin, ignored for Esp8266
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin);

//Colors
#define colorSaturation 128
RgbColor red(colorSaturation, 0, 0);
RgbColor green(0, colorSaturation, 0);
RgbColor blue(0, 0, colorSaturation);
RgbColor white(colorSaturation);
RgbColor black(0);
HslColor hslRed(red);
HslColor hslGreen(green);
HslColor hslBlue(blue);
HslColor hslWhite(white);
HslColor hslBlack(black);

//Websocket
WebSocketsServer webSocket = WebSocketsServer(81);

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t plength) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                webSocket.sendTXT(num, "Connected");
                Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
            }
            break;
        case WStype_TEXT:
            /*
             * We cannot reply to every incoming message, maybe because the receive/send buffers get full?
             * This can be easily seen if many 'singlecolor' messages are send to the server
             * There are also problem with large packets
             */
            switch(payload[0]){
                char buf[10]; //Output buffer for small messages
                case 's': case 'S': // Set pixels to a single color
                    if(plength == 7) {
                        sprintf(buf, "s%u", ++payload);
                        webSocket.sendTXT(num, payload);
                        Serial.printf("[%u] Got single color: %s\n", num, payload);
                        RgbColor singlecolor(payload);
                        strip.clearTo(singlecolor);
                    } else {
                        webSocket.sendTXT(num,"too short");  
                    }
                    // TODO send the singlecolor to the pixels
                    break;
                case 'h': case 'H': // Request free heap size
                    sprintf(buf, "h%lu", ESP.getFreeHeap());
                    webSocket.sendTXT(num, buf);
                    Serial.printf("Heap: [%u]\n",ESP.getFreeHeap());
                    break;
                case 'p': case 'P': // ping, will reply 'a' to show we are alive
                    Serial.printf("[%u] Got message: %s with length %u\n", num, payload, plength);
                    if(plength > 1) {
                        if(strcmp( (const char*) ++payload,"ping")==0) {
                            webSocket.sendTXT(num, "pong");
                        } else {
                            webSocket.sendTXT(num, payload);
                        }
                    } else {
                        webSocket.sendTXT(num,"a");  
                    }
                    break;
                default:
                    Serial.printf("[%u] Unknown command: %s with length %u\n", num, payload, plength);
                    webSocket.sendTXT(num, "Eunknown command!\n");
                    break;
            }
            break;
        case WStype_BIN:
            Serial.printf("[%u] get binary lenght: %u\n", num, plength);
            hexdump(payload, plength);
            webSocket.sendBIN(num, payload, plength);
            break;
    }
}

void configModeCallback (WiFiManager *myWiFiManager) {
    Serial.println("Entered config mode");
    Serial.println(WiFi.softAPIP());
    Serial.println(myWiFiManager->getConfigPortalSSID());
}

void setup() {
    delay(500);

    // Set up serial
    Serial.begin(115200);

    // WiFi
    WiFiManager wifiManager;
    // Reset settings - for testing
    // WifiManager.resetSettings();
    wifiManager.setAPCallback(configModeCallback);
    if(!wifiManager.autoConnect("WifiLedControl","ledblink")) {
        Serial.println("failed to connect and hit timeout");
        ESP.reset();
        delay(1000);
    } 
    Serial.println("Connected");
    Serial.println(WiFi.localIP());
    Serial.println(WiFi.gatewayIP());
    Serial.println(WiFi.subnetMask());

    // Start Websocket
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    // Reset LED strip
    strip.Begin();
    strip.Show();
}

void loop() {
    webSocket.loop();
}
