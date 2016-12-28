/*
  Joystick.h modified from Keyboard.h

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

#ifndef JOYSTICK_h
#define JOYSTICK_h

#include "HID.h"

#if !defined(_USING_HID)

#warning "Using legacy HID core (non pluggable)"

#else

//================================================================================
//================================================================================
//  JOYSTICK

// Axis names
#define AXIS_X 0
#define AXIS_Y 1
#define AXIS_Z 2
#define AXIS_Xrot 3
#define AXIS_Yrot 4
#define AXIS_Zrot 5
#define AXIS_Throttle 6
#define AXIS_Rudder 7
#define AXIS_Hat1 8
#define AXIS_Hat2 9


typedef struct      // Pretty self explanitory. Simple state to store all the joystick parameters
{
  uint8_t   xAxis;
  uint8_t   yAxis;
  uint8_t   zAxis;

  uint8_t   xRotAxis;
  uint8_t   yRotAxis;
  uint8_t   zRotAxis;

  uint8_t   throttle;
  uint8_t   rudder;

  uint8_t   hatSw1;
  uint8_t   hatSw2;

  uint32_t  buttons;    // 32 general buttons

} JoyState;

class Joystick_
{
private:
  JoyState _JoyState;
  void sendReport(JoyState* joyState);
public:
  Joystick_(void);

  void begin(void);
  void end(void);
  size_t buttonUp(uint8_t k);
  size_t buttonDown(uint8_t k);
  size_t buttonPress(uint8_t k);
  size_t axisSet(uint8_t axis, uint8_t val);
  size_t axisReset(uint8_t axis);
  void releaseAll(void);
};
extern Joystick_ Joystick;

#endif
#endif
