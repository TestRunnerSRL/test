/******************************************************************************
 * Program:     Arduino - Timer Pedal
 * Author:      TestRunner
 * Description: This program is used for the Arduino for the timer controls.
 *              This will send key presses to the PC to control the main timer.
 * Inputs:      There is 1 button for starting/stopping the timer
 * Outputs:     There are no outputs
 ******************************************************************************/

#include <Keyboard.h>

#define DEBOUNCE_DELAY 10 //In milliseconds

const byte PIN_SPLIT = 10; // PIN for display toggle
int pinState;
int pinSplitLast = HIGH;


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
	pinMode(PIN_SPLIT, INPUT_PULLUP);
  Keyboard.begin();
}

// Runs repeatedly in a loop. Handles the UI and Display.
void loop() {
	long now = millis();

	// Read Split Button
	pinState = digitalRead(PIN_SPLIT);
	if (pinState != pinSplitLast) {
		deBounce(PIN_SPLIT, pinState); //Debounce signal  
		pinSplitLast = pinState;

		if (pinState == LOW) {
			// Press (FALLING EDGE)
			Keyboard.write('X');
		}
	}
}

