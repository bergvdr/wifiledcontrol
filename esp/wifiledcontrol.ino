/*
 *  Control WS2812 leds over Wifi with ESP8266
 *  Version 0.3
 *  https://github.com/bergvdr/wifiledcontrol
 *
 *  Uses Websockets
 *  Uses WifiManager for easy Wifi setup
 *  Uses Neopixelbus for controlling LEDs
 *
 *  TODO:
 *	Support OTA updates
 *
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
NeoTopology<MyPanelLayout>* topo = NULL;
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod>* strip = NULL;
const uint8_t PixelPin = 2;  // make sure to set this to the correct pin, ignored for Esp8266

// Neopixel locations for use in panel (topo)
uint16_t left;
uint16_t right;
uint16_t top;
uint16_t bottom;

// Colors
#define colorSaturation 128
RgbColor red(colorSaturation / 4, 0, 0);
RgbColor green(0, colorSaturation / 4, 0);
RgbColor black(0);


// Neopixel gamma correction
// NeoGamma<NeoGammaEquationMethod> colorGamma;
NeoGamma<NeoGammaTableMethod> colorGamma;

// > Websocket
WebSocketsServer webSocket = WebSocketsServer(81);

// > User editable settings (put these in a struct?)
uint16_t PanelWidth = 8;
uint16_t PanelHeight = 8;
uint32_t PixelCount = PanelWidth * PanelHeight;
uint32_t pixelindex = 0;
uint32_t delay_amount = 500;
uint8_t orientation = 0;
uint8_t style = 0;
uint8_t textstyle = 0;
bool color_correct = true;

/*
 * === WiFi Functions ===
 */

void configModeCallback (WiFiManager *myWiFiManager) {
	Serial.println("Entered config mode");
	Serial.println(WiFi.softAPIP());
	Serial.println(myWiFiManager->getConfigPortalSSID());
}

void startWifi(bool reset = false) {
	// WiFi
	WiFiManager wifiManager;

	// Reset settings - for testing
    if (reset) {
		Serial.println("Resetting WiFi settings...");
        wifiManager.resetSettings();
    }

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
}


/*
 * === Setting Functions ===
 */

void setPanelColumns(uint8_t *nrofpixels, size_t length) {
    PanelWidth = charstr2int(nrofpixels, length);
    Serial.printf("Setting height/rows to %d\n", PanelWidth);
	SetPixelTopo(PanelWidth, PanelHeight);
}

void setPanelRows(uint8_t *nrofpixels, size_t length) {
    PanelHeight = charstr2int(nrofpixels, length);
    Serial.printf("Setting height/rows to %d\n", PanelHeight);
	SetPixelTopo(PanelWidth, PanelHeight);
}

void SetPixelTopo(uint16_t PanelWidth, uint16_t PanelHeight) {
    if (topo != NULL) {  
       delete topo; 
    }
	topo = new NeoTopology<MyPanelLayout>(PanelWidth, PanelHeight);
    // Neopixel locations for use in panel
    right = PanelWidth - 1;
    bottom = PanelHeight - 1;
	PixelCount = PanelWidth * PanelHeight;
	SetPixelCount(PixelCount);
}
void SetPixelCount(uint32_t PixelCount) {
	// Shameless copy-paste of NeoPixelBus documentation
    if (strip != NULL) {  
       delete strip; 
    }
    strip = new NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod>(PixelCount, PixelPin);
    strip->Begin();
    Serial.printf("Created strip of %d columns (width) and %d rows (height) totalling %d pixels\n", PanelWidth, PanelHeight, PixelCount);
}


/*
 * === Display Functions ===
 */

// This function creates a (non) gamma-corrected RGB color from RGB values
RgbColor getColor(uint8_t *rgb, bool color_correct = color_correct) {
	RgbColor color;
	if (color_correct) {
		color = colorGamma.Correct(RgbColor(rgb[0], rgb[1], rgb[2]));
	} else {
		color = RgbColor(rgb[0], rgb[1], rgb[2]);
	}

	return color;
}

// Clear the strip (off)
void clear() {
	strip->ClearTo(black);
	strip->Show();
}

// Rotate the strip
void rotate(uint16_t rotationCount = PanelWidth) {
	if(orientation == 0) {
		strip->RotateLeft(rotationCount);
	} else {
		strip->RotateRight(rotationCount);
	}
	strip->Show();
}

void shift(uint16_t shiftCount = PanelWidth) {
	if(orientation == 0) {
		strip->ShiftLeft(shiftCount);
	} else {
		strip->ShiftRight(shiftCount);
	}
	strip->Show();
}

// Set the entire strip to a single color
void singleColor(uint8_t *rgb) {
	RgbColor color = getColor(rgb);

	if(style == 0) {
		strip->ClearTo(color);
	} else {
		strip->SetPixelColor(++pixelindex % PixelCount, color);
	}
	strip->Show();
}

// Set a gradient from two consecutive rgb values
void setGradient(uint8_t *rgb) {
	HslColor startColor = HslColor(getColor(rgb, false));
	HslColor stopColor = HslColor(getColor(rgb + 3, false));

	if (orientation == 0) {
		// Vertical
		for (uint16_t index = 0; index < PanelWidth; index++)
		{
			float progress = index / static_cast<float>(PanelWidth - 1);
			RgbColor color = HslColor::LinearBlend<NeoHueBlendShortestDistance>(startColor, stopColor, progress);
			if(color_correct) color = colorGamma.Correct(color);
			for (uint16_t row = 0; row < PanelHeight; row++) {
				strip->SetPixelColor(index * PanelWidth + row, color);
			}
		}
	} else if (orientation == 1) {
		// Horizontal
		for (uint16_t index = 0; index < PanelWidth; index++)
		{
			float progress = index / static_cast<float>(PanelWidth - 1);
			RgbColor color = HslColor::LinearBlend<NeoHueBlendShortestDistance>(startColor, stopColor, progress);
			if(color_correct) color = colorGamma.Correct(color);
			for (uint16_t row = 0; row < PanelHeight; row++) {
				strip->SetPixelColor(row * PanelWidth + index, color);
			}
		}
	} else {
		// Consecutive, for strips
		for (uint32_t index = 0; index < PixelCount; index++)
		{
			float progress = index / static_cast<float>(PixelCount - 1);
			RgbColor color = HslColor::LinearBlend<NeoHueBlendShortestDistance>(startColor, stopColor, progress);
			if(color_correct) color = colorGamma.Correct(color);
			strip->SetPixelColor(index, color);
		}
	}
	strip->Show();
}

// Set a specific pixel according to topology
// The first two values are row, col; the next three are rgb
void setPixel(uint8_t *poscol) {
	RgbColor color = getColor(poscol + 2);
	strip->SetPixelColor(topo->Map(poscol[0], poscol[1]), color);
	strip->Show();
}

// Set the entire board sequentially
// Expect pixels to point to RGB values
void setPanel(uint8_t *pixels) {
	for(uint32_t i = 0; i < PixelCount; i++) {
		strip->SetPixelColor(i, getColor(pixels));
		pixels += 3;
	}
	strip->Show();
}

// Set multiple panels
void setPanels(uint8_t *pixels, uint8_t nrofpanels, uint8_t client) {
	/*
	 * Multiple entire board/strips received
	 * Display them with configured delay
	 */
	if(textstyle == 0) { // One by one
		for(uint16_t j = 0; j < nrofpanels; j++) {
			strip->ClearTo(black);
			strip->Show();
			for(uint32_t i = 0; i < PixelCount; i++) {
				strip->SetPixelColor(i, getColor(pixels));
				pixels += 3;
			}
			strip->Show();
			delay(delay_amount); // TODO: change this so the esp doesn't hang
			webSocket.sendTXT(client, "<"); //Send a pong because the delay is blocking
		}
	} else { // Scrolling
		if(orientation == 0) { //Orientation effects the shifting
			for(uint16_t j = 0; j < nrofpanels; j++) {
				for(uint16_t h = 0; h < PanelHeight; h++) {
					shift(8);
					for(uint16_t i = 0; i < PanelWidth; i++) {
						strip->SetPixelColor(PixelCount - PanelWidth + i, getColor(pixels));
						pixels += 3;
					}
					strip->Show();
					delay(delay_amount / 8); // TODO: change this so the esp doesn't hang
				}
				webSocket.sendTXT(client, "<"); //Send a pong because the delay is blocking
			}
		} else { //This currently displays chars mirrored :)
			for(uint16_t j = 0; j < nrofpanels; j++) {
				for(uint16_t h = 0; h < PanelHeight; h++) {
					shift(8);
					for(uint16_t i = 0; i < PanelWidth; i++) {
						strip->SetPixelColor(i, getColor(pixels));
						pixels += 3;
					}
					strip->Show();
					delay(delay_amount / 8); // TODO: change this so the esp doesn't hang
				}
				webSocket.sendTXT(client, "<"); //Send a pong because the delay is blocking
			}
		}
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

        // TODO: Send current settings to client

		// Set an initial configuration to orient the panel
		strip->ClearTo(black);
		strip->SetPixelColor(topo->Map(left, top), green);
		strip->SetPixelColor(topo->Map(right, top), red);
		strip->SetPixelColor(topo->Map(left, bottom), red);
		strip->SetPixelColor(topo->Map(right, bottom), black);
		strip->Show();
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
		case 'c':
		case 'C': // Change whether we gamma correct
			if(payload[1] == 'y') {
				color_correct = true;
				webSocket.sendTXT(client, "ITurned gamma correction on");
			} else {
				webSocket.sendTXT(client, "ITurned gamma correction off");
				color_correct = false;
			}
			Serial.printf("[%u] INFO: color_correct: %d with length %u\n", client, color_correct, plength);
			break;
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
		case 'p':
		case 'P': // Change panel (strip) size
			if(payload[1] == 'c') {
				// Set column size
                setPanelColumns(payload + 2, plength - 2);
                webSocket.sendTXT(client, "ISet panel width (columns)");
			} else if(payload[1] == 'r') {
                setPanelRows(payload + 2, plength - 2);
                webSocket.sendTXT(client, "ISet panel height (rows)");
            }
			break;
		case 'r':
		case 'R': // Change the style of updating, one at a time or rolling (scrolling)
			if(payload[1] == 'o') {
				textstyle = 0; // One by one
				webSocket.sendTXT(client, "IChanged charcter style to one-by-one");
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
		case 'w':
		case 'W': // Change wifi settings
			if(payload[1] == 'r') {
                // Reset settings
                startWifi(true);
			}
			else if(payload[1] == 's') {
				// Connect to specified SSID
				webSocket.sendTXT(client, "ITrying to connect to SSID");
                // 2 byte length of SSID

			}
			break;
		case '>': // ping, will reply '<' to show we are alive
			// Serial.printf("[%u] Got message: %s with length %u\n", client, payload, plength);
			if(plength > 1) {
				if(strcmp( (const char*) ++payload, "ping") == 0) {
					webSocket.sendTXT(client, "pong");
					strip->ClearTo(black);
					strip->Show();
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
			case 253: // Shift the strip
				shift(payload[1]);
				break;
			case 254: // Rotate the strip
				rotate(payload[1]);
				break;
			case 255: // Clear the strip
				clear();
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


void setup() {
	delay(500);

	// Set up serial
	Serial.begin(115200);

	// WiFi
	startWifi();

	// Start Websocket
	webSocket.begin();
	webSocket.onEvent(webSocketEvent);

	// Reset LED strip
	SetPixelTopo(PanelWidth, PanelHeight); //Configure an initial strip
	strip->ClearTo(green);
	strip->Show();
}

void loop() {
	webSocket.loop();
}
