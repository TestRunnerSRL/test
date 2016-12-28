/*
  Joystick.cpp modified from Keyboard.cpp

  Copyright (c) 2015, Arduino LLC
  Original code (pre-library): Copyright (c) 2011, Peter Barrett

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "Joystick.h"

#if defined(_USING_HID)

//================================================================================
//================================================================================
//	Joystick

static const uint8_t _hidReportDescriptor[] PROGMEM = {

  //  Joystick
	0x05, 0x01,			// USAGE_PAGE (Generic Desktop)
	0x09, 0x04,			// USAGE (Joystick)
	0xa1, 0x01,			// COLLECTION (Application)
	0x85, 0x04,			// REPORT_ID (3)  (This is important when HID_SendReport() is called)

	//Buttons:
		0x05, 0x09,			// USAGE_PAGE (Button)
		0x19, 0x01,			// USAGE_MINIMUM (Button 1)
		0x29, 0x20,			// USAGE_MAXIMUM (Button 32)
		0x15, 0x00,			// LOGICAL_MINIMUM (0)
		0x25, 0x01,			// LOGICAL_MAXIMUM (1)
		0x75, 0x01,			// REPORT_SIZE (1)
		0x95, 0x20,			// REPORT_COUNT (32)
		0x55, 0x00,			// UNIT_EXPONENT (0)
		0x65, 0x00,			// UNIT (None)
		0x81, 0x02,			// INPUT (Data,Var,Abs)

	// 8 bit Throttle and Steering
		0x05, 0x02,			// USAGE_PAGE (Simulation Controls)

		0x15, 0x00,			// LOGICAL_MINIMUM (0)
		0x26, 0xff, 0x00,		// LOGICAL_MAXIMUM (255)
		0xA1, 0x00,			// COLLECTION (Physical)
			0x09, 0xBB,			// USAGE (Throttle)
			0x09, 0xBA,			// USAGE (Steering)
			0x75, 0x08,			// REPORT_SIZE (8)
			0x95, 0x02,			// REPORT_COUNT (2)
			0x81, 0x02,			// INPUT (Data,Var,Abs)

		0xc0,				// END_COLLECTION

	// Two Hat switches
		0x05, 0x01,			// USAGE_PAGE (Generic Desktop)
		0x09, 0x39,			// USAGE (Hat switch)
		0x15, 0x00,			// LOGICAL_MINIMUM (0)
		0x25, 0x07,			// LOGICAL_MAXIMUM (7)
		0x35, 0x00,			// PHYSICAL_MINIMUM (0)
		0x46, 0x3B, 0x01,		// PHYSICAL_MAXIMUM (315)
		0x65, 0x14,			// UNIT (Eng Rot:Angular Pos)
		0x75, 0x04,			// REPORT_SIZE (4)
		0x95, 0x01,			// REPORT_COUNT (1)
		0x81, 0x02,			// INPUT (Data,Var,Abs)

		0x09, 0x39,			// USAGE (Hat switch)
		0x15, 0x00,			// LOGICAL_MINIMUM (0)
		0x25, 0x07,			// LOGICAL_MAXIMUM (7)
		0x35, 0x00,			// PHYSICAL_MINIMUM (0)
		0x46, 0x3B, 0x01,		// PHYSICAL_MAXIMUM (315)
		0x65, 0x14,			// UNIT (Eng Rot:Angular Pos)
		0x75, 0x04,			// REPORT_SIZE (4)
		0x95, 0x01,			// REPORT_COUNT (1)
		0x81, 0x02,			// INPUT (Data,Var,Abs)

		0x15, 0x00,			// LOGICAL_MINIMUM (0)
		0x26, 0xff, 0x00,		// LOGICAL_MAXIMUM (255)
		0x75, 0x08,			// REPORT_SIZE (8)

		0x09, 0x01,			// USAGE (Pointer)
		0xA1, 0x00,			// COLLECTION (Physical)
			0x09, 0x30,		// USAGE (x)
			0x09, 0x31,		// USAGE (y)
			0x09, 0x32,		// USAGE (z)
			0x09, 0x33,		// USAGE (rx)
			0x09, 0x34,		// USAGE (ry)
			0x09, 0x35,		// USAGE (rz)
			0x95, 0x06,		// REPORT_COUNT (2)
			0x81, 0x02,		// INPUT (Data,Var,Abs)
		0xc0,				// END_COLLECTION

	0xc0					// END_COLLECTION
};

Joystick_::Joystick_(void) 
{
	static HIDSubDescriptor node(_hidReportDescriptor, sizeof(_hidReportDescriptor));
	HID().AppendDescriptor(&node);
}

void Joystick_::begin(void)
{
	releaseAll();
}

void Joystick_::end(void)
{
}

#define joyBytes 13 // should be equivalent to sizeof(JoyState_t)
void Joystick_::sendReport(JoyState* joyState)
{
	uint8_t data[joyBytes];
	uint32_t buttonTmp;
	buttonTmp = joyState->buttons;

	data[0] = buttonTmp & 0xFF;		// Break 32 bit button-state out into 4 bytes, to send over USB
	buttonTmp >>= 8;
	data[1] = buttonTmp & 0xFF;
	buttonTmp >>= 8;
	data[2] = buttonTmp & 0xFF;
	buttonTmp >>= 8;
	data[3] = buttonTmp & 0xFF;

	data[4] = joyState->throttle;		// Throttle
	data[5] = joyState->rudder;		// Steering

	data[6] = (joyState->hatSw2 << 4) | joyState->hatSw1;		// Pack hat-switch states into a single byte

	data[7] = joyState->xAxis;		// X axis
	data[8] = joyState->yAxis;		// Y axis
	data[9] = joyState->zAxis;		// Z axis
	data[10] = joyState->xRotAxis;		// rX axis
	data[11] = joyState->yRotAxis;		// rY axis
	data[12] = joyState->zRotAxis;		// rZ axis

	//HID_SendReport(Report number, array of values in same order as HID descriptor, length)
	HID().SendReport(4,data,joyBytes);
}

// press() adds the specified key (printing, non-printing, or modifier)
// to the persistent key report and sends the report.  Because of the way 
// USB HID works, the host acts like the key remains pressed until we 
// call release(), releaseAll(), or otherwise clear the report and resend.
size_t Joystick_::buttonDown(uint8_t k) 
{
	_JoyState.buttons |= 1 << k;
	
	sendReport(&_JoyState);
	return 1;
}

// release() takes the specified key out of the persistent key report and
// sends the report.  This tells the OS the key is no longer pressed and that
// it shouldn't be repeated any more.
size_t Joystick_::buttonUp(uint8_t k) 
{
	_JoyState.buttons &= ~(1 << k);
	
	sendReport(&_JoyState);
	return 1;
}


size_t Joystick_::axisSet(uint8_t axis, uint8_t val) {
	switch (axis) {
		case AXIS_X:
			_JoyState.xAxis = val;
			break;
		case AXIS_Y:
			_JoyState.yAxis = val;
			break;
		case AXIS_Z:
			_JoyState.zAxis = val;
			break;
		case AXIS_Xrot:
			_JoyState.xRotAxis = val;
			break;
		case AXIS_Yrot:
			_JoyState.yRotAxis = val;
			break;
		case AXIS_Zrot:
			_JoyState.zRotAxis = val;
			break;
		case AXIS_Throttle:
			_JoyState.throttle = val;
			break;
		case AXIS_Rudder:
			_JoyState.rudder = val;
			break;
		case AXIS_Hat1:
			_JoyState.hatSw1 = val;
			break;
		case AXIS_Hat2:
			_JoyState.hatSw2 = val;
			break;
		default:
			return 0;
	}

	sendReport(&_JoyState);
	return 1;
}
 
size_t Joystick_::axisReset(uint8_t axis) {
	switch (axis) {
		case AXIS_X:
			_JoyState.xAxis = 0x80;
			break;
		case AXIS_Y:
			_JoyState.yAxis = 0x80;
			break;
		case AXIS_Z:
			_JoyState.zAxis = 0x00;
			break;
		case AXIS_Xrot:
			_JoyState.xRotAxis = 0x00;
			break;
		case AXIS_Yrot:
			_JoyState.yRotAxis = 0x00;
			break;
		case AXIS_Zrot:
			_JoyState.zRotAxis = 0x00;
			break;
		case AXIS_Throttle:
			_JoyState.throttle = 0x00;
			break;
		case AXIS_Rudder:
			_JoyState.rudder = 0x00;
			break;
		case AXIS_Hat1:
			_JoyState.hatSw1 = 0x00;
			break;
		case AXIS_Hat2:
			_JoyState.hatSw2 = 0x00;
			break;
		default:
			return 0;
	}

	sendReport(&_JoyState);
	return 1;
}

void Joystick_::releaseAll(void)
{
	_JoyState.buttons = 0x0000;	
	_JoyState.xAxis = 0x80;
	_JoyState.yAxis = 0x80;
	_JoyState.zAxis = 0x00;
	_JoyState.xRotAxis = 0x00;
	_JoyState.yRotAxis = 0x00;
	_JoyState.zRotAxis = 0x00;
	_JoyState.throttle = 0x00;
	_JoyState.rudder = 0x00;
	_JoyState.hatSw1 = 0x00;
	_JoyState.hatSw2 = 0x00;

	sendReport(&_JoyState);
}

size_t Joystick_::buttonPress(uint8_t c)
{	
	uint8_t p = buttonDown(c);  // Keydown
	buttonUp(c);            		// Keyup
	return p;              // just return the result of press() since release() almost always returns 1
}

Joystick_ Joystick;

#endif
