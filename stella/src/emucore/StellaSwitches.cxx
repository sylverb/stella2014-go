//============================================================================
//
//   SSSS    tt          lll  lll       
//  SS  SS   tt           ll   ll        
//  SS     tttttt  eeee   ll   ll   aaaa 
//   SSSS    tt   ee  ee  ll   ll      aa
//      SS   tt   eeeeee  ll   ll   aaaaa  --  "An Atari 2600 VCS Emulator"
//  SS  SS   tt   ee      ll   ll  aa  aa
//   SSSS     ttt  eeeee llll llll  aaaaa
//
// Copyright (c) 1995-2014 by Bradford W. Mott, Stephen Anthony
// and the Stella Team
//
// See the file "License.txt" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//
// $Id: Switches.cxx 2838 2014-01-17 23:34:03Z stephena $
//============================================================================

#include "Event.hxx"
#include "Props.hxx"
#include "Switches.hxx"
#ifdef TARGET_GNW
extern uint8_t a2600_difficulty;
#endif

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
#ifndef TARGET_GNW
Switches::Switches(const Event& event, const Properties& properties)
#else
Switches::Switches(const Event& event)
#endif
  : myEvent(event),
    mySwitches(0xFF)
{
#ifndef TARGET_GNW
  if(properties.get(Console_RightDifficulty) == "B")
  {
    mySwitches &= ~0x80;
  }
  else
#endif
  {
    mySwitches |= 0x80;
  }

// TODO Sylver : use global variable for difficulty setting
#ifndef TARGET_GNW
  if(properties.get(Console_LeftDifficulty) == "B")
#else
  if (a2600_difficulty == 1)
#endif
  {
    mySwitches &= ~0x40;
  }
  else
  {
    mySwitches |= 0x40;
  }

#ifndef TARGET_GNW
  if(properties.get(Console_TelevisionType) == "COLOR")
  {
    mySwitches |= 0x08;
  }
  else
  {
    mySwitches &= ~0x08;
  }
#else
    mySwitches |= 0x08;
#endif
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
Switches::~Switches()
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
void Switches::update()
{
  if(myEvent.get(Event::ConsoleColor) != 0)
  {
    mySwitches |= 0x08;
  }
  else if(myEvent.get(Event::ConsoleBlackWhite) != 0)
  {
    mySwitches &= ~0x08;
  }

  if(myEvent.get(Event::ConsoleRightDiffA) != 0)
  {
    mySwitches |= 0x80;
  }
  else if(myEvent.get(Event::ConsoleRightDiffB) != 0) 
  {
    mySwitches &= ~0x80;
  }

  if(myEvent.get(Event::ConsoleLeftDiffA) != 0)
  {
    mySwitches |= 0x40;
  }
  else if(myEvent.get(Event::ConsoleLeftDiffB) != 0)
  {
    mySwitches &= ~0x40;
  }

  if(myEvent.get(Event::ConsoleSelect) != 0)
  {
    mySwitches &= ~0x02;
  }
  else 
  {
    mySwitches |= 0x02;
  }

  if(myEvent.get(Event::ConsoleReset) != 0)
  {
    mySwitches &= ~0x01;
  }
  else 
  {
    mySwitches |= 0x01;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
bool Switches::save(Serializer& out) const
{
#ifndef TARGET_GNW
  try
  {
    out.putByte(mySwitches);
  }
  catch(...)
  {
    return false;
  }
  return true;
#else
  return true;
#endif
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
bool Switches::load(Serializer& in)
{
#ifndef TARGET_GNW
  try
  {
    mySwitches = in.getByte();
  }
  catch(...)
  {
    return false;
  }
  return true;
#else
  return true;
#endif
}
