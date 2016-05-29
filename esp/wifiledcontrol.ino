/*
 *  Control WS2812 leds over Wifi with ESP8266
 *  Version 0.2
 *  https://github.com/bergvdr/wifiledcontrol
 *
 *  Uses Websockets
 *  Uses WifiManager for easy Wifi setup
 *  Uses Neopixelbus for controlling LEDs
 *
 *  TODO: Support OTA updates
 */

#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <Hash.h>
#include <NeoPixelBus.h>
#include <WebSocketsServer.h>
#include <WiFiManager.h>

#include "wifiledcontrol.h"

/*
 * === Settings ===
 */

// > Neopixel related
typedef ColumnMajorLayout MyPanelLayout;
const uint8_t PanelWidth = 8;  // 8 pixel x 8 pixel matrix of leds
const uint8_t PanelHeight = 8;
const uint8_t PixelPin = 2;  // make sure to set this to the correct pin, ignored for Esp8266
NeoTopology<MyPanelLayout> topo(PanelWidth, PanelHeight);

const uint16_t PixelCount = PanelWidth * PanelHeight;
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin);

// Colors
#define colorSaturation 128
RgbColor red(colorSaturation / 4, 0, 0);
RgbColor green(0, colorSaturation / 4, 0);
RgbColor black(0);

// Neopixel locations for use in panel
const uint16_t left = 0;
const uint16_t right = PanelWidth - 1;
const uint16_t top = 0;
const uint16_t bottom = PanelHeight - 1;

// Neopixel gamma correction
// NeoGamma<NeoGammaEquationMethod> colorGamma;
NeoGamma<NeoGammaTableMethod> colorGamma;

// > Websocket
WebSocketsServer webSocket = WebSocketsServer(81);

// > User editable settings
uint32_t delay_amount = 500;
uint8_t orientation = 0;
uint8_t style = 0;
uint32_t pixelindex = 0;

/*
 * === Display Functions ===
 */


// Set the entire strip to a single color
void singleColor(uint8_t *color) {
	RgbColor gcolor = colorGamma.Correct(RgbColor(color[0], color[1], color[2]));
	if(style == 0) {
		strip.ClearTo(gcolor);
	} else {
		strip.SetPixelColor(++pixelindex%PixelCount, gcolor);
	}
	strip.Show();
}

// Set a gradient
void setGradient(uint8_t *colors) {
	HslColor startColor = HslColor(RgbColor(colors[0], colors[1], colors[2]));
    HslColor stopColor = HslColor(RgbColor(colors[3], colors[4], colors[5]));

	if (orientation == 0) {
		// Vertical
		for (uint16_t index = 0; index < PanelWidth; index++)
		{
			float progress = index / static_cast<float>(PanelWidth - 1);
			RgbColor color = HslColor::LinearBlend<NeoHueBlendShortestDistance>(startColor, stopColor, progress);
			color = colorGamma.Correct(color);
			for (uint16_t row = 0; row < PanelHeight; row++) {
				strip.SetPixelColor(index*PanelWidth + row, color);
			}
		}
	} else if (orientation == 1) {
		// Horizontal
		for (uint16_t index = 0; index < PanelWidth; index++)
		{
			float progress = index / static_cast<float>(PanelWidth - 1);
			RgbColor color = HslColor::LinearBlend<NeoHueBlendShortestDistance>(startColor, stopColor, progress);
			color = colorGamma.Correct(color);
			for (uint16_t row = 0; row < PanelHeight; row++) {
				strip.SetPixelColor(row*PanelWidth + index, color);
			}
		}
	} else {
		// Consecutive
		for (uint16_t index = 0; index < PixelCount; index++)
		{
			float progress = index / static_cast<float>(PixelCount - 1);
			RgbColor color = HslColor::LinearBlend<NeoHueBlendShortestDistance>(startColor, stopColor, progress);
			color = colorGamma.Correct(color);
			strip.SetPixelColor(index, color);
		}
	}
    strip.Show();
}

// Set a specific pixel according to topology
void setPixel(uint8_t *poscol) {
	RgbColor gcolor = colorGamma.Correct(RgbColor(poscol[2], poscol[3], poscol[4]));
	strip.SetPixelColor(topo.Map(poscol[0], poscol[1]), gcolor);
	strip.Show();
}

// Set the entire board sequentially
void setPanel(uint8_t *pixels) {
	for(uint16_t i = 0; i < PixelCount; i++) {
		RgbColor gcolor = colorGamma.Correct(RgbColor(*pixels++, *pixels++, *pixels++));
		strip.SetPixelColor(i, gcolor);
	}
	strip.Show();
}

// Set multiple panels
void setPanels(uint8_t *pixels, uint8_t nrofpanels, uint8_t client) {
	/*
	 * Multiple entire board/strips received
	 * Display them with configured delay
	 */
	for(uint16_t j = 0; j < nrofpanels; j++) {
		strip.ClearTo(black);
		strip.Show();
		for(uint16_t i = 0; i < PixelCount; i++) {
			strip.SetPixelColor(i, RgbColor(*pixels++, *pixels++, *pixels++));
		}
		strip.Show();
		delay(delay_amount); // TODO: change this so the esp doesn't hang
		webSocket.sendTXT(client, "<"); //Send a ping because the delay is blocking
	}
}

/*
 * === Websocket Events ===
 */
void webSocketEvent(uint8_t client, WStype_t type, uint8_t * payload, size_t plength) {
	switch(type) {
	case WStype_DISCONNECTED:
		Serial.printf("[%u] Disconnected!\n", client);
		break;
	case WStype_CONNECTED:
	{
		IPAddress ip = webSocket.remoteIP(client);
		webSocket.sendTXT(client, "Connected");
		Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", client, ip[0], ip[1], ip[2], ip[3], payload);

		// Set an initial configuration to orient the panel
		strip.ClearTo(black);
		strip.SetPixelColor(topo.Map(left, top), green);
		strip.SetPixelColor(topo.Map(right, top), red);
		strip.SetPixelColor(topo.Map(left, bottom), red);
		strip.SetPixelColor(topo.Map(right, bottom), black);
		strip.Show();
	}
	break;
	case WStype_TEXT:
		/*
		 * The ESP cannot reply to every incoming message with this code.
		 * Possibly because the receive/send buffers get full?
		 * There are also problem with large packets. In both cases the
		 * websocket connection is lost.
		 * So the websocket textmode is used only to change settings. It does send messages back
		 * Updating the LEDs is done in a binary message and without sending an reply
		 */
		switch(payload[0]) {
			char buf[10]; //Output buffer for small messages
		case 'd':
		case 'D': // Configure delay
			delay_amount = charstr2int(++payload, plength - 1);
			webSocket.sendTXT(client, "IUpdated delay value");
			Serial.printf("[%u] INFO: Delay: %u with length %u\n", client, delay_amount, plength);
			break;
		case 'h':
		case 'H': // Request free heap size
			sprintf(buf, "h%lu", ESP.getFreeHeap());
			webSocket.sendTXT(client, buf);
			Serial.printf("[%u] Requested free heap size: [%u]\n", client, ESP.getFreeHeap());
			break;
		case 'o':
		case 'O': // Change orientation for gradients, considering a panel
			if(payload[1] == 'h') {
				orientation = 0; // horizontal
				webSocket.sendTXT(client, "IChanged gradient orientation to horizontal");
			}
			else if(payload[1] == 'v') {
				orientation = 1; // vertical
				webSocket.sendTXT(client, "IChanged gradient orientation to vertical");
			}
			else if(payload[1] == 'c') {
				orientation = 2; // consecutive
				webSocket.sendTXT(client, "IChanged gradient orientation to consecutive");
			}
			break;
		case 's':
		case 'S': // Change the style of updating, all at once, consecutive 
			if(payload[1] == 'a') {
				style = 0; // All at once
				webSocket.sendTXT(client, "IChanged singlecolor style to all at once");
			}
			else if(payload[1] == 'c') {
				style = 1; // consecutive
				webSocket.sendTXT(client, "IChanged singlecolor style to consecutive");
			}
			break;
		case '>': // ping, will reply '<' to show we are alive
			// Serial.printf("[%u] Got message: %s with length %u\n", client, payload, plength);
			if(plength > 1) {
				if(strcmp( (const char*) ++payload, "ping") == 0) {
					webSocket.sendTXT(client, "pong");
					strip.ClearTo(black);
					strip.Show();
				} else {
					webSocket.sendTXT(client, payload);
				}
			} else {
				webSocket.sendTXT(client, "<");
			}

			break;
		default:
			Serial.printf("[%u] Unknown command: %s with length %u\n", client, payload, plength);
			webSocket.sendTXT(client, "EUnknown command!");
			break;
		}
		break;
	case WStype_BIN:
		/*
		 * Binary data is used to drive the pixels
		 */

		Serial.printf("[%u] got binary length: %u\n", client, plength); //Debug
		Serial.printf("[%u] command: %u\n", client, payload[0]); //Debug

		// The first byte indicates what should happen
		if(plength) {
			switch(payload[0]) {
			case 1: // Expect a single color, three uint8 representing RGB values
				// Call singlecolor with a pointer to the RGB values
				if(plength == 4) {
					singleColor(payload + 1);
				}
				break;
			case 2: // Expect two colors and set a gradient
				if(plength == 7) {
					setGradient(payload + 1);
				}
				break;
			case 3: // Expect a pixel index (row, col) and RGB color
				if(plength == 6) {
					setPixel(payload + 1);
				}
				break;
			case 4: // Received an entire board/strip of RGB values
				if(plength == 1 + PixelCount * 3) {
					setPanel(payload + 1);
				}
				break;
			case 5: // Expect multiple entire boards
				if((plength - 1) % (PixelCount * 3) == 0) {
					Serial.printf("multiple boards");
					setPanels(payload + 1, (plength - 1) / (PixelCount * 3), client);
				}
				break;
			default:
				Serial.printf("[%u] Unknown command [%d] with length %u\n",
                    payload[0], client, plength);
				webSocket.sendTXT(client, "Eunknown command!\n");
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
	if(!wifiManager.autoConnect("WifiLedControl", "ledblink")) {
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
