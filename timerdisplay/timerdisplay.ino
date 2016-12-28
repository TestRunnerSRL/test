/******************************************************************************
 * Program:     Arduino - Timer Display
 * Author:      TestRunner
 * Description: This program is used for the Arduino for the timer display.
 *              It has an internal timer which is displayed on the LEDs.
 *              The timer will cycle display all the runners time when stopped.
 *              The server sends the timer state to the Arduino via Serial
 * Inputs:      - 1 Display button. Toggles the display on or off
 *              - SerialUSB comm with the timer server
 * Outputs:     There are also 3 outputs
 *               - 2 LED 7-Segment timer displays. Combined for hh:mm:ss
 ******************************************************************************/

#include <Wire.h>
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"

#include <ArduinoJson.h>

#define INTERFACE SerialUSB
#define DEBUG Serial
#define JSONBUFFER 1000
#define DEBOUNCE_DELAY 10 //In milliseconds
#define BLINK_SPEED 500 // Blink speed (ms)
#define CYCLE_SPEED 3000 // Player change speed (ms)
#define OVERRIDE_TIME 2000
#define PING_SEND_TIME 2000
#define PING_REC_TIME 10000
const int BUFFER_SIZE = JSON_ARRAY_SIZE(10) + JSON_OBJECT_SIZE(100);

char jsonBuffer[JSONBUFFER];
int jsonBufferLength = 0;

const byte CHAR_TO_DISPLAY[] = {
  0b00111111,  // 0
  0b00000110,  // 1
  0b01011011,  // 2
  0b01001111,  // 3
  0b01100110,  // 4
  0b01101101,  // 5
  0b01111101,  // 6
  0b00000111,  // 7
  0b01111111,  // 8
  0b01101111,  // 9
};

enum State {
	RESET, RUNNING, PAUSED, FINISHED
} state = RESET;
long overrideDisplay = 0;
long overrideTimout = 0;

const byte PIN_DISPLAY_RESET = 0; // Output pin to resets the displays.
const byte PIN_DISPLAY_TOGGLE = 40; // Input PIN for display toggle
int pinDisplayToggleLast = HIGH;
enum DisplayMode {
	HUNDREDTH, TENTH, SECOND, NONE
} displayMode = TENTH;

long timerStart = 0; 
long timerStop = 0;
long raceTimes[4] = { 0 };
bool raceFinish[4] = { false };
bool race = false;

long pauseTime = 0;

long racetimeTimeout = 0;
byte racetimePlayer = 0;

Adafruit_7segment timerRight = Adafruit_7segment();
Adafruit_7segment timerLeft = Adafruit_7segment();
const byte PIN_TIMER_RIGHT = 0x70; // PIN for display right
const byte PIN_TIMER_LEFT = 0x71; // PIN for display left
byte prevDisplayBuffer[20] = { 255 };

long lastSendPing = 0;
long lastRecPing = 0;
bool connected = false;


void initDisplays() {
  if (PIN_DISPLAY_RESET) {
	  digitalWrite(PIN_DISPLAY_RESET, HIGH);
  }
	delay(10);

	timerRight.begin(PIN_TIMER_RIGHT);
	timerLeft.begin(PIN_TIMER_LEFT);

	timerRight.setBrightness(15);
	timerLeft.setBrightness(15);

	for (int i = 0; i < 5; i++) {
		prevDisplayBuffer[i] = 0xFF;
		prevDisplayBuffer[i + 5] = 0xFF;
		timerLeft.writeDigitRaw(i, 0xFF);
		timerRight.writeDigitRaw(i, 0xFF);
	}
	timerLeft.writeDisplay();
	timerRight.writeDisplay();
}


void writeLeftDisplay() {
	if (timerLeft.writeDisplay() > 0 && PIN_DISPLAY_RESET) {
		digitalWrite(PIN_DISPLAY_RESET, LOW);
		delay(1000);
		initDisplays();
	}
}

void writeRightDisplay() {
	if (timerRight.writeDisplay() > 0 && PIN_DISPLAY_RESET) {
		digitalWrite(PIN_DISPLAY_RESET, LOW);
		delay(1000);
		initDisplays();
	}
}

void writeDisplays() {
	writeLeftDisplay();
	writeRightDisplay();
}



// Takes a time duration in millioseconds and puts the display byte
// into the byte buffer the corrospond to the 7-seg display
// If player == -1 then no player. If time == -1, then no display at all
void displayTime(long time, bool race) {
	byte buffer[20];

	// Don't display anything if time == -1
	if (time == -1) {
		for (int i = 0; i < 10; i++)
			buffer[i] = 0;

		return;
	}

	// Set player digit
	buffer[0] = 0;

	// Calculate timer digits
	long digits[7];
	digits[0] = (int)(time / 3600000) % 10; //1 hour = 60 * 60 * 1000 = 3600000ms
	digits[1] = ((int)(time / 60000) % 60) / 10;  //10 min = 10 * 60 * 1000 = 600000ms
	digits[2] = (int)(time / 60000) % 10;   //1 min  = 60 * 1000 = 60000ms
	digits[3] = ((int)(time / 1000) % 60) / 10;   //10 sec = 10 * 1000 = 10000ms
	digits[4] = (int)(time / 1000) % 10;    //1 sec    = 1000ms
	digits[5] = (int)(time / 100) % 10;     //0.1 sec  = 100ms
	digits[6] = (int)(time / 10) % 10;      //0.01 sec = 10ms

	const byte digitToBuffer[] = { 1, 3, 4, 5, 6, 8, 9 };

	// Calculate number of displayed digits. Minium is 0:00.00
	int minDigit = 0;
	for (int minDigit = 0; (digits[minDigit] == 0) && (minDigit <= 5); minDigit++) {
		buffer[digitToBuffer[minDigit]] = 0;
	}

	// Set the colons for number of digits
	if (minDigit == 0)
		buffer[2] = 0b00000011; // " 00:00"
	else
		buffer[2] = 0b00000000; // " 00 00"

	if (minDigit <= 2)
		buffer[7] = 0b00001111; // ":00.00"
	else
		buffer[7] = 0b00000011; // " 00.00"

								// Set the digits masks
	for (; minDigit < 7; minDigit++) {
		buffer[digitToBuffer[minDigit]] = CHAR_TO_DISPLAY[digits[minDigit]];
	}

	if (race) {
		buffer[8] = 0;
		buffer[7] = 0b00001100; // ":00 00"
	}

	drawDisplay(buffer);
}


// Sets the byte buffer to draw the number of players set
void numPlayersToDisplay(byte* buffer, byte players) {
	buffer[0] = CHAR_TO_DISPLAY[players];
	buffer[1] = 0;
	buffer[3] = 0b01110011;  // Player 'P'
	buffer[4] = 0b00111000;  // Player 'L'
	buffer[5] = 0b01110111;  // Player 'A'
	buffer[6] = 0b01100110;  // Player 'Y'
	buffer[8] = 0b01111001;  // Player 'E'
	buffer[9] = 0b01110111;  // Player 'R'

	buffer[2] = 0; // No colons
	buffer[7] = 0;
}


// Sets the 7-seg display with to the array of bitmasks
void drawDisplay(byte* buffer) {
	if (!connected) {
		buffer[0] = 0b01111001; // 'E'
	}

	if (displayMode == TENTH) {
		buffer[9] = 0;
	}
	else if (displayMode == SECOND) {
		buffer[8] = 0;
		buffer[9] = 0;
	}
	else if (displayMode == NONE) {
		clearDisplay();
		return;
	}

	bool change = true;// false;
	for (int i = 5; i < 10; i++) {
		if (buffer[i] != prevDisplayBuffer[i]) {
			prevDisplayBuffer[i] = buffer[i];
			timerRight.writeDigitRaw(i - 5, buffer[i]);
			change = true;
		}
	}
	if (change == true) {
		writeRightDisplay();
	}

	change = true;// false;
	for (int i = 0; i < 5; i++) {
		if (buffer[i] != prevDisplayBuffer[i]) {
			prevDisplayBuffer[i] = buffer[i];
			timerLeft.writeDigitRaw(i, buffer[i]);
			change = true;
		}
	}
	if (change == true) {
		writeLeftDisplay();
	}
}


// Clears the display
void clearDisplay() {
	bool change = false;
	for (int i = 0; i < 10; i++) {
		if (prevDisplayBuffer[i] != 0) {
			prevDisplayBuffer[i] = 0;
			change = true;
		}
	}

	if (!change) {
		return;
	}

	timerLeft.clear();
	timerRight.clear();

	writeDisplays();
}


// Parses the fetch request and updates all the values.
// Returns true is parsing is successful
bool parseTimes(char* buffer) {
	long now = millis();

	StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

	JsonObject& root = jsonBuffer.parseObject(buffer);

	if (!root.success()) {
		//DEBUG.println("JSON parse failure");
		return false;
	}

	if (strcmp(root["event"], "tick") == 0) {
		state = RUNNING;
		timerStart = now - ((long)root["arguments"][0] * 1000);
	}
	else if (strcmp(root["event"], "running") == 0) {
		switch (state) {
		case RESET:
			timerStart = now;
			break;
		case PAUSED:
			timerStart = now - pauseTime;
			break;
		case FINISHED:
			timerStart = now - timerStop;
			break;
		case RUNNING:
		default:
			break;
		}
		
		state = RUNNING;
	}
	else if (strcmp(root["event"], "stopped") == 0) {
		if (state != RESET) {
			state = PAUSED;
			pauseTime = now - timerStart;
		}
	}
	else if (strcmp(root["event"], "reset") == 0) {
		state = RESET;
		for (int i = 0; i < 4; i++) {
			raceFinish[i] = false;
			raceTimes[i] = 0;
		}
	}
	else if (strcmp(root["event"], "finished") == 0) {
		int i;
		for (i = 0; i < 4; i++) {
			raceFinish[i] = false;
		}

		timerStop = now - timerStart;

		JsonArray& finishJSONarray = root["arguments"][0].asArray();

		i = 0;
		byte completed = 0;
		for (JsonArray::iterator it = finishJSONarray.begin(); it != finishJSONarray.end(); ++it, i++)
		{
			// this also works: 
			JsonObject& finishJSON = it->asObject();
			raceTimes[i] = (finishJSON["raw"].as<long>() * 1000);
			raceFinish[i] = !finishJSON["forfeit"].as<bool>();

			if (raceTimes[i] == 0) {
				raceFinish[i] = false;
			}

			if (raceFinish[i]) {
				completed++;
			}
		}

		if (i > 1 && completed > 0) {
			race = true;
		}

		state = FINISHED;
	}
	else if (strcmp(root["event"], "runnerFinished") == 0) {
		overrideDisplay = now - timerStart;
		overrideTimout = now + OVERRIDE_TIME;
	}

	return true;
}


// Debounces the buttonPin pin. 
// Waits for DECOUNCE_DELAY milliseconds of stable signal.
void deBounce(int buttonPin, int state)
{
	unsigned long now = millis();

	do {
		// on bounce, reset time-out
		if (digitalRead(buttonPin) != state)
			now = millis();
	} while (digitalRead(buttonPin) != state ||
		(millis() - now) <= DEBOUNCE_DELAY);
}


// Runs once on Arduino startup. Sets up ethernet and pins.
void setup() {
	// Initialize the pinModes
  if (PIN_DISPLAY_RESET) {
    pinMode(PIN_DISPLAY_RESET, OUTPUT);
  }
	pinMode(PIN_DISPLAY_TOGGLE, INPUT_PULLUP);

  INTERFACE.begin(9600);
  initDisplays();

	while (!INTERFACE) {
	};

	connected = false;
	do {
		while (INTERFACE.available()) {
			char val = INTERFACE.read();
			if (val == '\n' || jsonBufferLength >= JSONBUFFER) {
				jsonBuffer[jsonBufferLength] = 0;
				jsonBufferLength = 0;

				if (strcmp(jsonBuffer, "handshake") == 0) {
					connected = true;
					break;
				}
			}
			else {
				jsonBuffer[jsonBufferLength++] = val;
			}
		}
	} while (!connected);

	INTERFACE.print("handshake\n");
  long now = millis();
  lastRecPing = now;
  lastSendPing = now;

	timerLeft.clear();
	timerRight.clear();
	writeDisplays();
}


// Runs repeatedly in a loop. Handles the UI and Display.
void loop() {
	delay(10);
	long now = millis();

	if (connected && abs(now - lastSendPing) > PING_SEND_TIME) {
		lastSendPing = now;
		INTERFACE.print("heartbeat\n");
	}

	// if there are incoming bytes available 
	// from the server, and store them in the json buffer
	while (INTERFACE.available()) {
		char val = INTERFACE.read();

		if (val == '\n' || jsonBufferLength >= JSONBUFFER) {
			jsonBuffer[jsonBufferLength] = 0;
			jsonBufferLength = 0;

			if (strcmp(jsonBuffer, "heartbeat") == 0) {
				INTERFACE.print("heartbeat\n");
        lastRecPing = now;
        lastSendPing = now;
			}
			else {
				//DEBUG.print(jsonBuffer);
				parseTimes(jsonBuffer);
			}
		}
		else {
			jsonBuffer[jsonBufferLength++] = val;
		}
	}

	if (connected && abs(now - lastRecPing) > PING_REC_TIME) {
    setup();
	}

	// Read Display Toggle Button
	int pinState = digitalRead(PIN_DISPLAY_TOGGLE);
	if (pinState != pinDisplayToggleLast) {
		deBounce(PIN_DISPLAY_TOGGLE, pinState); //Debounce signal  
		pinDisplayToggleLast = pinState;

		if (pinState == LOW) {
			// Press (FALLING EDGE)
			switch (displayMode) {
			case HUNDREDTH:
				displayMode = TENTH;
				break;
			case TENTH:
				displayMode = NONE; // SECOND;
				break;
			case SECOND:
				displayMode = NONE;
				break;
			case NONE:
			default:
				displayMode = TENTH; // HUNDREDTH;
				break;
			}
		}
	}

	// Read Display Toggle Button
	pinState = digitalRead(PIN_DISPLAY_TOGGLE);
	if (pinState != pinDisplayToggleLast) {
		deBounce(PIN_DISPLAY_TOGGLE, pinState); //Debounce signal  
		pinDisplayToggleLast = pinState;

		if (pinState == LOW) {
			// Press (FALLING EDGE)
			INTERFACE.print("startFinish\n");
			//DEBUG.print("startFinish\n");

			switch (state) {
			case RUNNING:
				timerStop = now - timerStart;
				state = FINISHED;
				break;
			case RESET:
				timerStart = now;
				state = RUNNING;
				break;
			case PAUSED:
				timerStart = now - pauseTime;
				state = RUNNING;
				break;
			case FINISHED:
				timerStart = now - timerStop;
				state = RUNNING;
				break;
			default:
				state = RUNNING;
			}
		}
	}

	if (now < overrideTimout) {
		if ((now / BLINK_SPEED) % 2 == 0) {
			displayTime(overrideDisplay, false);
		}
		else {
			clearDisplay();
		}
	}
	else {
		switch (state) {
		case RESET:
			displayTime(0, false);
			break;
		case RUNNING:
			displayTime(now - timerStart, false);
			break;
		case PAUSED:
			if ((now / BLINK_SPEED) % 2 == 0) {
				displayTime(pauseTime, false);
			}
			else {
				clearDisplay();
			}
			break;
		case FINISHED:
			if (race) {
				if (now > racetimeTimeout) {
					racetimeTimeout = now + CYCLE_SPEED;
					
					do {
						racetimePlayer = (racetimePlayer + 1) % 4;
					} while (!raceFinish[racetimePlayer]);	
				}

				displayTime(raceTimes[racetimePlayer], true);
			}
			else {
				displayTime(timerStop, false);
			}
			break;
		default:
			displayTime(123456789, false);
		};
	}
}

