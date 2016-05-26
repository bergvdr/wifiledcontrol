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
typedef ColumnMajorAlternatingLayout MyPanelLayout;
const uint8_t PanelWidth = 8;  // 8 pixel x 8 pixel matrix of leds
const uint8_t PanelHeight = 8;
const uint8_t PixelPin = 2;  // make sure to set this to the correct pin, ignored for Esp8266
NeoTopology<MyPanelLayout> topo(PanelWidth, PanelHeight);

const uint16_t PixelCount = PanelWidth * PanelHeight;
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin);

const uint16_t left = 0;
const uint16_t right = PanelWidth - 1;
const uint16_t top = 0;
const uint16_t bottom = PanelHeight - 1;

//Colors
#define colorSaturation 128
RgbColor red(colorSaturation, 0, 0);
RgbColor greend(0, colorSaturation/8, 0);
RgbColor black(0);

//Websocket
WebSocketsServer webSocket = WebSocketsServer(81);

uint32_t x2i(uint8_t *hexstring) {
	uint32_t x = 0;
	for(uint8_t pos =0; pos<6; pos++) {
		char c = *hexstring;
		if (c >= '0' && c <= '9') {
		  x *= 16;
		  x += c - '0';
		}
		else if (c >= 'A' && c <= 'F') {
		  x *= 16;
		  x += (c - 'A') + 10;
		}
		else if (c >= 'a' && c <= 'f') {
		  x *= 16;
		  x += (c - 'a') + 10;
		} else {
          Serial.printf("Could not convert: %c\n", c);
		  return 0xFF0000;
		}
		hexstring++;
	}
	return x;
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t plength) {
	//HtmlColor singlecolor;
	RgbColor singlecolor;
    uint32_t hc;
	uint8_t row = 0;
	uint8_t col = 0;

    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                webSocket.sendTXT(num, "Connected");
                Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
				strip.ClearTo(black);
                strip.SetPixelColor(topo.Map(left, top), white);
                strip.SetPixelColor(topo.Map(right, top), red);
                strip.SetPixelColor(topo.Map(right, bottom), green);
                strip.SetPixelColor(topo.Map(left, bottom), blue);
                strip.Show();
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
                    if(plength == 4) {
                        strip.ClearTo(RgbColor(payload[1],payload[2],payload[3]));
                        sprintf(buf, "s%u", ++payload);
                        hc = x2i(payload);
                        Serial.printf("[%u] Converted to: %d\n", num, hc);
                        singlecolor = HtmlColor(hc);                
                        strip.ClearTo(singlecolor);
                        strip.Show();  

                        Serial.printf("[%u] Got single color: %s\n", num, payload);
                        webSocket.sendTXT(num, payload);
                    } else {
                        webSocket.sendTXT(num,"too short");  
                    }
                    // TODO send the singlecolor to the pixels
                    break;
                case 'h': case 'H': // Request free heap size
                    strip.ClearTo(red);
                    strip.Show();
                    sprintf(buf, "h%lu", ESP.getFreeHeap());
                    webSocket.sendTXT(num, buf);
                    Serial.printf("Heap: [%u]\n",ESP.getFreeHeap());
                    break;
                case 'I': // Set the pixels all at once
                    strip.ClearTo(black);
					payload++;
					for(uint16_t i=0; i < PixelCount; i++) {
						strip.SetPixelColor(i,HtmlColor(x2i(payload)));
						payload += 6;
					}
                    strip.Show();
                    webSocket.sendTXT(num, "individual man!");
                    break;
                case 'i': // Set just one pixel
                    Serial.printf("[%u] Got message: %s with length %u\n", num, payload, plength);
					payload++;
                    Serial.printf("\n\n==>>> %c", *payload);
					while(++payload != '\0' && *payload != 'C') {
						Serial.printf("\n==>>> %c", *payload);
						if (*payload >= '0' && *payload <= '9') {
							row = 10*row + (*payload - '0');
						} else {
							Serial.printf("\n==>>> ERROR %c", *payload);
						}
					}
					while(++payload != '\0' && *payload != '#') {
						Serial.printf("\n==>>> %c", *payload);
						if (*payload >= '0' && *payload <= '9') {
							col = 10*col + (*payload - '0');
						} else {
							Serial.printf("\n==>>> ERROR %c", *payload);
						}
					}
					payload++;
                	strip.SetPixelColor(topo.Map(row, col), HtmlColor(x2i(payload)));
                    strip.Show();
                    webSocket.sendTXT(num, "individual man!");
                    Serial.printf("\n[%u] edit ind row %d col %d with color# %d\n", num, row, col, x2i(payload));
                    break;
                case 'p': case 'P': // ping, will reply 'a' to show we are alive
                    Serial.printf("[%u] Got message: %s with length %u\n", num, payload, plength);
                    if(plength > 1) {
                        if(strcmp( (const char*) ++payload,"ping")==0) {
                            webSocket.sendTXT(num, "pong");
                            strip.ClearTo(black);
                            strip.Show();

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
    //wifiManager.resetSettings();
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
    strip.ClearTo(greend);
    strip.Show();
}

void loop() {
    webSocket.loop();
}
