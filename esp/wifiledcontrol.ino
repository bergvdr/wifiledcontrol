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
typedef ColumnMajorLayout MyPanelLayout;
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
RgbColor red(colorSaturation/4, 0, 0);
RgbColor green(0, colorSaturation/4, 0);
RgbColor black(0);

//Websocket
WebSocketsServer webSocket = WebSocketsServer(81);

//User editable settings
uint32_t delay_amount = 500;

uint32_t charstr2int(uint8_t *charstr, size_t length) {
	uint32_t result = 0;
	for(uint8_t pos =0; pos<length; pos++) {
		char c = *charstr;
		if (c >= '0' && c <= '9') {
			result *= 10;
			result += c - '0';
		}
		charstr++;
	}
	return result;
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t plength) {
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
				strip.ClearTo(black); //Set some initial configuration so you can orient the board
                strip.SetPixelColor(topo.Map(left, top), green);
                strip.SetPixelColor(topo.Map(right, top), red);
                strip.SetPixelColor(topo.Map(left, bottom), red);
                strip.SetPixelColor(topo.Map(right, bottom), black);
                strip.Show();
            }
            break;
        case WStype_TEXT:
            /*
             * We cannot reply to every incoming message, maybe because the receive/send buffers get full?
             * This can be easily seen if many 'singlecolor' messages are send to the server
             * There are also problem with large packets
			 *
			 * So I try to send only answers to changes in settings.
			 * Updating the LEDs is done in a binary message and without sending an reply
             */
            switch(payload[0]){
                char buf[10]; //Output buffer for small messages
                case 'd': case 'D': // Configure delay
					delay_amount = charstr2int(++payload, plength-1);
                    webSocket.sendTXT(num,"IUpdated delay value");  
					Serial.printf("[%u] INFO: Delay: %u with length %u\n", num, delay_amount, plength);
                    break;
                case 'h': case 'H': // Request free heap size
                    sprintf(buf, "h%lu", ESP.getFreeHeap());
                    webSocket.sendTXT(num, buf);
                    Serial.printf("[%u] Requested free heap size: [%u]\n",num, ESP.getFreeHeap());
                    break;
                case '>': // ping, will reply '<' to show we are alive
                    // Serial.printf("[%u] Got message: %s with length %u\n", num, payload, plength);
                    if(plength > 1) {
                        if(strcmp( (const char*) ++payload, "ping") == 0) {
                            webSocket.sendTXT(num, "pong");
                            strip.ClearTo(black);
                            strip.Show();
                        } else {
                            webSocket.sendTXT(num, payload);
                        }
                    } else {
                        webSocket.sendTXT(num,"<");  
                    }
                    
                    break;
                default:
                    Serial.printf("[%u] Unknown command: %s with length %u\n", num, payload, plength);
                    webSocket.sendTXT(num, "EUnknown command!\n");
                    break;
            }
            break;
        case WStype_BIN:
			/*
			 * We process mostly binary data
			 *
			 */

            //Serial.printf("[%u] got binary length: %u\n", num, plength); //Debug

			switch(plength) {
				case 3: // Received a single color, three uint8 representing RGB values
					strip.ClearTo(RgbColor(payload[0], payload[1], payload[2]));
					strip.Show();
					break;
				case 5: // Received a pixel index (row, col) and RGB color
                    strip.SetPixelColor(topo.Map(payload[0],payload[1]), 
						RgbColor(payload[2], payload[3], payload[4]));
					strip.Show();
					break;
				case PixelCount * 3: // Received an entire board/strip of RGB values
					for(uint16_t i = 0; i < PixelCount; i++) {
						strip.SetPixelColor(i, RgbColor(*payload++, *payload++, *payload++));
					}
					strip.Show();
					break;
                default:
					if(plength > PixelCount*3 && (plength % (PixelCount*3)) == 0) {
						/* 
						 * Multiple entire board/strips received
						 * Display them with configured delay
						 */
						for(uint16_t j = 0; j < plength/(PixelCount*3); j++) {
							strip.ClearTo(black);
							strip.Show();
							for(uint16_t i = 0; i < PixelCount; i++) {
								strip.SetPixelColor(i, RgbColor(*payload++, *payload++, *payload++));
							}
							strip.Show();
							delay(delay_amount); // TODO: change this so the esp doesn't hang
							webSocket.sendTXT(num, "<"); //Send a ping because the delay is blocking  
						}
					} else {
						Serial.printf("[%u] Unknown command with length %u\n", num, plength);
						webSocket.sendTXT(num, "Eunknown command!\n");
					}
                    break;
			}
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
    strip.ClearTo(green);
    strip.Show();
}

void loop() {
    webSocket.loop();
}
