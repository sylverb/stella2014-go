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
// $Id: Console.cxx 2838 2014-01-17 23:34:03Z stephena $
//============================================================================

#include "AtariVox.hxx"
#include "Booster.hxx"
#include "Cart.hxx"
#include "Control.hxx"
#include "Cart.hxx"
#include "Driving.hxx"
#include "Event.hxx"
#include "EventHandler.hxx"
#include "Joystick.hxx"
#include "Keyboard.hxx"
#ifndef TARGET_GNW
#include "KidVid.hxx"
#include "CompuMate.hxx"
#else
extern uInt8 a2600_y_offset;
extern uInt16 a2600_height;
extern char a2600_control[];
extern bool a2600_control_swap;
extern bool a2600_swap_paddle;
extern char a2600_display_mode[];
extern bool a2600_fastscbios;
#endif
#include "Genesis.hxx"
#include "MindLink.hxx"
#include "M6502.hxx"
#include "M6532.hxx"
#include "Paddles.hxx"
#ifndef TARGET_GNW
#include "Props.hxx"
#include "PropsSet.hxx"
#endif
#include "SaveKey.hxx"
#include "Settings.hxx" 
#include "Sound.hxx"
#include "Switches.hxx"
#include "System.hxx"
#include "TIA.hxx"
#include "TrackBall.hxx"
#include "FrameBuffer.hxx"
#include "OSystem.hxx"
//#include "Menu.hxx"
//#include "CommandMenu.hxx"
#include "Serializable.hxx"
#include "Version.hxx"

#ifdef CHEATCODE_SUPPORT
  #include "CheatManager.hxx"
#endif

#if defined(XBGR8888)
#define R_SHIFT 0
#define G_SHIFT 8
#define B_SHIFT 16
#else
#define R_SHIFT 16
#define G_SHIFT 8
#define B_SHIFT 0
#endif

#include "Console.hxx"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef TARGET_GNW
Console::Console(OSystem* osystem, Cartridge* cart, const Properties& props)
#else
Console::Console(OSystem* osystem, Cartridge* cart)
#endif
  : myOSystem(osystem),
    myEvent(osystem->eventHandler().event()),
#ifndef TARGET_GNW
    myProperties(props),
#endif
    myTIA(0),
    mySwitches(0),
    mySystem(0),
    myCart(cart),
    myCMHandler(0),
    myDisplayFormat(""),  // Unknown TV format @ start
    myFramerate(0.0),     // Unknown framerate @ start
    myCurrentFormat(0),   // Unknown format @ start
    myUserPaletteDefined(false)
{
  // Load user-defined palette for this ROM
#ifndef TARGET_GNW
  loadUserPalette();
#endif

  // Create switches for the console
#ifndef TARGET_GNW
  mySwitches = new Switches(myEvent, myProperties);
#else
  mySwitches = new Switches(myEvent);
#endif

  // Construct the system and components
  mySystem = new System(13, 6);

  // The real controllers for this console will be added later
  // For now, we just add dummy joystick controllers, since autodetection
  // runs the emulation for a while, and this may interfere with 'smart'
  // controllers such as the AVox and SaveKey
  // Note that the controllers must be added directly after the system
  // has been created, and before any other device is added
  // (particularly the M6532)
  myControllers[0] = new Joystick(Controller::Left, myEvent, *mySystem);
  myControllers[1] = new Joystick(Controller::Right, myEvent, *mySystem);

  M6502* m6502 = new M6502(1, myOSystem->settings());

  myRiot = new M6532(*this, myOSystem->settings());
  myTIA  = new TIA(*this, myOSystem->sound(), myOSystem->settings());

  mySystem->attach(m6502);
  mySystem->attach(myRiot);
  mySystem->attach(myTIA);
  mySystem->attach(myCart);

  // Auto-detect NTSC/PAL mode if it's requested
  string autodetected = "";

#ifndef TARGET_GNW
  myDisplayFormat = myProperties.get(Display_Format);
#else
  myDisplayFormat = a2600_display_mode;
#endif

#ifndef TARGET_GNW
  if(myDisplayFormat == "AUTO" || myOSystem->settings().getBool("rominfo"))
#else
  if(myDisplayFormat == "AUTO")
#endif
  {
    // Run the TIA, looking for PAL scanline patterns
    // We turn off the SuperCharger progress bars, otherwise the SC BIOS
    // will take over 250 frames!
    // The 'fastscbios' option must be changed before the system is reset
#ifndef TARGET_GNW
    bool fastscbios = myOSystem->settings().getBool("fastscbios");
    myOSystem->settings().setValue("fastscbios", true);
#else
    bool fastscbios = a2600_fastscbios;
    a2600_fastscbios = true;
#endif
    mySystem->reset(true);  // autodetect in reset enabled
    for(int i = 0; i < 60; ++i)
      myTIA->update();
    myDisplayFormat = myTIA->isPAL() ? "PAL" : "NTSC";
#ifndef TARGET_GNW
    if(myProperties.get(Display_Format) == "AUTO")
#else
    if(strcmp(a2600_display_mode,"AUTO") == 0)
#endif
    {
      autodetected = "*";
      myCurrentFormat = 0;
    }

    // Don't forget to reset the SC progress bars again
#ifndef TARGET_GNW
    myOSystem->settings().setValue("fastscbios", fastscbios);
#else
    a2600_fastscbios = fastscbios;
#endif
  }
#ifndef TARGET_GNW
  myConsoleInfo.DisplayFormat = myDisplayFormat + autodetected;
#endif

  // Set up the correct properties used when toggling format
  // Note that this can be overridden if a format is forced
  //   For example, if a PAL ROM is forced to be NTSC, it will use NTSC-like
  //   properties (60Hz, 262 scanlines, etc), but likely result in flicker
  // The TIA will self-adjust the framerate if necessary
  setTIAProperties();
  if(myDisplayFormat == "NTSC")         myCurrentFormat = 1;
  else if(myDisplayFormat == "PAL")     myCurrentFormat = 2;
  else if(myDisplayFormat == "SECAM")   myCurrentFormat = 3;
  else if(myDisplayFormat == "NTSC50")  myCurrentFormat = 4;
  else if(myDisplayFormat == "PAL60")   myCurrentFormat = 5;
  else if(myDisplayFormat == "SECAM60") myCurrentFormat = 6;

  // Add the real controllers for this system
  // This must be done before the debugger is initialized
  #ifndef TARGET_GNW
  const string& md5 = myProperties.get(Cartridge_MD5);
  #else
  const string& md5 = "";
  #endif
  setControllers(md5);

  // Bumper Bash always requires all 4 directions
  // Other ROMs can use it if the setting is enabled
#ifndef TARGET_GNW
  bool joyallow4 = md5 == "aa1c41f86ec44c0a44eb64c332ce08af" ||
                   md5 == "1bf503c724001b09be79c515ecfcbd03" ||
                   myOSystem->settings().getBool("joyallow4");
  myOSystem->eventHandler().allowAllDirections(joyallow4);
#else
  myOSystem->eventHandler().allowAllDirections(false);
#endif

  // Reset the system to its power-on state
  mySystem->reset();

  // Finally, add remaining info about the console
  #ifndef TARGET_GNW
  myConsoleInfo.CartName   = myProperties.get(Cartridge_Name);
  myConsoleInfo.CartMD5    = myProperties.get(Cartridge_MD5);
  myConsoleInfo.Control0   = myControllers[0]->about();
  myConsoleInfo.Control1   = myControllers[1]->about();
  myConsoleInfo.BankSwitch = cart->about();

  myCart->setRomName(myConsoleInfo.CartName);
  #endif
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Console::~Console()
{
  delete mySystem;
  delete mySwitches;
#ifndef TARGET_GNW
  delete myCMHandler;
#endif
  delete myControllers[0];
  delete myControllers[1];
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Console::save(Serializer& out) const
{
#ifndef TARGET_GNW
  try
  {
    // First save state for the system
    if(!mySystem->save(out))
      return false;

    // Now save the console controllers and switches
    if(!(myControllers[0]->save(out) && myControllers[1]->save(out) &&
         mySwitches->save(out)))
      return false;
  }
  catch(...)
  {
    return false;
  }

  return true;  // success
#else
  return true;
#endif
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Console::load(Serializer& in)
{
#ifndef TARGET_GNW
  try
  {
    // First load state for the system
    if(!mySystem->load(in))
      return false;

    // Then load the console controllers and switches
    if(!(myControllers[0]->load(in) && myControllers[1]->load(in) &&
         mySwitches->load(in)))
      return false;
  }
  catch(...)
  {
    return false;
  }

  return true;  // success
#else
  return true;
#endif
}

#ifndef TARGET_GNW
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Console::toggleFormat(int direction)
{
  string saveformat;

  if(direction == 1)
    myCurrentFormat = (myCurrentFormat + 1) % 7;
  else if(direction == -1)
    myCurrentFormat = myCurrentFormat > 0 ? (myCurrentFormat - 1) : 6;

  switch(myCurrentFormat)
  {
    case 0:  // auto-detect
      myTIA->update();
      myDisplayFormat = myTIA->isPAL() ? "PAL" : "NTSC";
      saveformat = "AUTO";
      break;
    case 1:
      saveformat = myDisplayFormat  = "NTSC";
      break;
    case 2:
      saveformat = myDisplayFormat  = "PAL";
      break;
    case 3:
      saveformat = myDisplayFormat  = "SECAM";
      break;
    case 4:
      saveformat = myDisplayFormat  = "NTSC50";
      break;
    case 5:
      saveformat = myDisplayFormat  = "PAL60";
      break;
    case 6:
      saveformat = myDisplayFormat  = "SECAM60";
      break;
  }
  myProperties.set(Display_Format, saveformat);

  setPalette(myOSystem->settings().getString("palette"));
  setTIAProperties();
  myTIA->frameReset();
  initializeVideo();  // takes care of refreshing the screen
}
#endif

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Console::toggleColorLoss()
{
#ifndef TARGET_GNW
  bool colorloss = !myOSystem->settings().getBool("colorloss");
  myOSystem->settings().setValue("colorloss", colorloss);
  myTIA->enableColorLoss(colorloss);
#endif
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Console::toggleColorLoss(bool state)
{
  myTIA->enableColorLoss(state);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Console::togglePalette()
{
#ifndef TARGET_GNW
  string palette;
  palette = myOSystem->settings().getString("palette");
 
  if(palette == "standard")       // switch to z26
  {
    palette = "z26";
  }
  else if(palette == "z26")       // switch to user or standard
  {
    // If we have a user-defined palette, it will come next in
    // the sequence; otherwise loop back to the standard one
    if(myUserPaletteDefined)
      palette = "user";
    else
      palette = "standard";
  }
  else if(palette == "user")  // switch to standard
    palette = "standard";
  else  // switch to standard mode if we get this far
    palette = "standard";

  myOSystem->settings().setValue("palette", palette);

  setPalette(palette);
#endif
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Console::setPalette(const string& type)
{
  // Look at all the palettes, since we don't know which one is
  // currently active
  uInt32* palettes[3][3] = {
    { &ourNTSCPalette[0],    &ourPALPalette[0],    &ourSECAMPalette[0]    },
    { &ourNTSCPaletteZ26[0], &ourPALPaletteZ26[0], &ourSECAMPaletteZ26[0] },
    { 0, 0, 0 }
  };
  if(myUserPaletteDefined)
  {
    palettes[2][0] = &ourUserNTSCPalette[0];
    palettes[2][1] = &ourUserPALPalette[0];
    palettes[2][2] = &ourUserSECAMPalette[0];
  }

  // See which format we should be using
  int paletteNum = 0;
  if(type == "standard")
    paletteNum = 0;
  else if(type == "z26")
    paletteNum = 1;
  else if(type == "user" && myUserPaletteDefined)
    paletteNum = 2;

  // Now consider the current display format
  currentPalette =
    (myDisplayFormat.compare(0, 3, "PAL") == 0)   ? palettes[paletteNum][1] :
    (myDisplayFormat.compare(0, 5, "SECAM") == 0) ? palettes[paletteNum][2] :
     palettes[paletteNum][0];

  //myOSystem->frameBuffer().setTIAPalette(currentPalette);
}

const uInt32* Console::getPalette(int direction) const
{
	return currentPalette;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef TARGET_GNW
void Console::togglePhosphor()
{
  const string& phosphor = myProperties.get(Display_Phosphor);
  int blend = atoi(myProperties.get(Display_PPBlend).c_str());
  bool enable;
  if(phosphor == "YES")
  {
    myProperties.set(Display_Phosphor, "No");
    enable = false;
  }
  else
  {
    myProperties.set(Display_Phosphor, "Yes");
    enable = true;
  }

  myOSystem->frameBuffer().enablePhosphor(enable, blend);
}
#endif

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef TARGET_GNW
void Console::setProperties(const Properties& props)
{
  myProperties = props;
}
#endif

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
FBInitStatus Console::initializeVideo(bool full)
{
  FBInitStatus fbstatus = kSuccess;

  if(full)
  {
#ifndef TARGET_GNW
    const string& title = string("Stella ") + STELLA_VERSION +
                   ": \"" + myProperties.get(Cartridge_Name) + "\"";
#else
    const string& title = "";
#endif
    fbstatus = myOSystem->frameBuffer().initialize(title,
                 myTIA->width() << 1, myTIA->height());
    if(fbstatus != kSuccess)
      return fbstatus;
    setColorLossPalette();
  }

#ifndef TARGET_GNW
  bool enable = myProperties.get(Display_Phosphor) == "YES";
  int blend = atoi(myProperties.get(Display_PPBlend).c_str());
  myOSystem->frameBuffer().enablePhosphor(enable, blend);
#endif

#ifndef TARGET_GNW
  setPalette(myOSystem->settings().getString("palette"));
#else
  setPalette("standard");
#endif

  // Set the correct framerate based on the format of the ROM
  // This can be overridden by changing the framerate on the
  // commandline, but it can't be saved
  // (ie, framerate is now determined based on number of scanlines).
  //float framerate = myOSystem->settings().getFloat("framerate");
  //if(framerate > 0) myFramerate = float(framerate);
  myOSystem->setFramerate(myFramerate);

  // Make sure auto-frame calculation is only enabled when necessary
  //myTIA->enableAutoFrame(framerate <= 0);

  return fbstatus;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Console::initializeAudio()
{
  // Initialize the sound interface.
  // The # of channels can be overridden in the AudioDialog box or on
  // the commandline, but it can't be saved.
  //float framerate = myOSystem->settings().getFloat("framerate");
  //if(framerate > 0) myFramerate = float(framerate);
#ifndef TARGET_GNW
  const string& sound = myProperties.get(Cartridge_Sound);
#endif

  myOSystem->sound().close();
#ifndef TARGET_GNW
  myOSystem->sound().setChannels(sound == "STEREO" ? 2 : 1);
#else
  myOSystem->sound().setChannels(1);
#endif
  myOSystem->sound().setFrameRate(myFramerate);
  myOSystem->sound().open();

  // Make sure auto-frame calculation is only enabled when necessary
  //myTIA->enableAutoFrame(framerate <= 0);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
/* Original frying research and code by Fred Quimby.
   I've tried the following variations on this code:
   - Both OR and Exclusive OR instead of AND. This generally crashes the game
     without ever giving us realistic "fried" effects.
   - Loop only over the RIOT RAM. This still gave us frying-like effects, but
     it seemed harder to duplicate most effects. I have no idea why, but
     munging the TIA regs seems to have some effect (I'd think it wouldn't).

   Fred says he also tried mangling the PC and registers, but usually it'd just
   crash the game (e.g. black screen, no way out of it).

   It's definitely easier to get some effects (e.g. 255 lives in Battlezone)
   with this code than it is on a real console. My guess is that most "good"
   frying effects come from a RIOT location getting cleared to 0. Fred's
   code is more likely to accomplish this than frying a real console is...

   Until someone comes up with a more accurate way to emulate frying, I'm
   leaving this as Fred posted it.   -- B.
*/
void Console::fry() const
{
  for (int ZPmem=0; ZPmem<0x100; ZPmem += rand() % 4)
    mySystem->poke(ZPmem, mySystem->peek(ZPmem) & (uInt8)rand() % 256);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Console::changeYStart(int direction)
{
  uInt32 ystart = myTIA->ystart();

  if(direction == +1)       // increase YStart
  {
    if(ystart >= 64)
      return;
    ystart++;
  }
  else if(direction == -1)  // decrease YStart
  {
    if(ystart == 0)
      return;
    ystart--;
  }
  else
    return;

  myTIA->setYStart(ystart);
  myTIA->frameReset();

#ifndef TARGET_GNW
  ostringstream val;
  val << ystart;
  myProperties.set(Display_YStart, val.str());
#endif
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Console::changeHeight(int direction)
{
  uInt32 height = myTIA->height();

  if(direction == +1)       // increase Height
  {
    height++;
    if(height > 256 || height > myOSystem->desktopHeight()) /* Height at maximum */
      return;
  }
  else if(direction == -1)  // decrease Height
  {
    height--;
    if(height < 210) /* Height at minimum */
      return;
  }
  else
    return;

  myTIA->setHeight(height);
  myTIA->frameReset();
  initializeVideo();  // takes care of refreshing the screen

#ifndef TARGET_GNW
  ostringstream val;
  val << height;
  myProperties.set(Display_Height, val.str());
#endif
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Console::setTIAProperties()
{
  // TODO - query these values directly from the TIA if value is 'AUTO'
#ifndef TARGET_GNW
  uInt32 ystart = atoi(myProperties.get(Display_YStart).c_str());
#else
  uInt8 ystart = a2600_y_offset;
#endif
  if(ystart > 64) ystart = 64;
#ifndef TARGET_GNW
  uInt32 height = atoi(myProperties.get(Display_Height).c_str());
#else
  uInt16 height = a2600_height;
#endif
  if(height < 210)      height = 210;
  else if(height > 256) height = 256;

  if(myDisplayFormat == "NTSC" || myDisplayFormat == "PAL60" ||
     myDisplayFormat == "SECAM60")
  {
    // Assume we've got ~262 scanlines (NTSC-like format)
    //myFramerate = 60.0;
    myFramerate = 59.92;
    myConsoleInfo.InitialFrameRate = "60";
  }
  else
  {
    // Assume we've got ~312 scanlines (PAL-like format)
    //myFramerate = 50.0;
    myFramerate = 49.92;
    myConsoleInfo.InitialFrameRate = "50";

    // PAL ROMs normally need at least 250 lines
    height = MAX(height, 250u);
  }

  // Make sure these values fit within the bounds of the desktop
  // If not, attempt to center vertically
  if(height > myOSystem->desktopHeight())
  {
    ystart += height - myOSystem->desktopHeight();
    ystart  = MIN(ystart, 64u);
    height  = myOSystem->desktopHeight();
  }
  myTIA->setYStart(ystart);
  myTIA->setHeight(height);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Console::setControllers(const string& rommd5)
{
  delete myControllers[0];
  delete myControllers[1];

  // Setup the controllers based on properties
#ifndef TARGET_GNW
  const string& left  = myProperties.get(Controller_Left);
  const string& right = myProperties.get(Controller_Right);
#else
  const string& left  = a2600_control;
  const string& right = "";
#endif

  // Check for CompuMate controllers; they are special in that a handler
  // creates them for us, and also that they must be used in both ports
#ifndef TARGET_GNW
  if(left == "COMPUMATE" || right == "COMPUMATE")
  {
    delete myCMHandler;
    myCMHandler = new CompuMate(*((CartridgeCM*)myCart), myEvent, *mySystem);
    myControllers[0] = myCMHandler->leftController();
    myControllers[1] = myCMHandler->rightController();
    return;
  }
#endif

  // Swap the ports if necessary
  int leftPort, rightPort;
#ifndef TARGET_GNW
  if(myProperties.get(Console_SwapPorts) == "NO")
#else
  if(!a2600_control_swap)
#endif
  {
    leftPort = 0; rightPort = 1;
  }
  else
  {
    leftPort = 1; rightPort = 0;
  }

  // Also check if we should swap the paddles plugged into a jack
#ifndef TARGET_GNW
  bool swapPaddles = myProperties.get(Controller_SwapPaddles) == "YES";
#else
  bool swapPaddles = a2600_swap_paddle;
#endif

  // Construct left controller
  if(left == "BOOSTERGRIP")
  {
    myControllers[leftPort] = new BoosterGrip(Controller::Left, myEvent, *mySystem);
  }
  else if(left == "DRIVING")
  {
    myControllers[leftPort] = new Driving(Controller::Left, myEvent, *mySystem);
  }
  else if((left == "KEYBOARD") || (left == "KEYPAD"))
  {
    myControllers[leftPort] = new Keyboard(Controller::Left, myEvent, *mySystem);
  }
  else if(BSPF_startsWithIgnoreCase(left, "PADDLES"))
  {
    bool swapAxis = false, swapDir = false;
    if(left == "PADDLES_IAXIS")
      swapAxis = true;
    else if(left == "PADDLES_IDIR")
      swapDir = true;
    else if(left == "PADDLES_IAXDR")
      swapAxis = swapDir = true;
    myControllers[leftPort] =
      new Paddles(Controller::Left, myEvent, *mySystem,
                  swapPaddles, swapAxis, swapDir);
  }
  else if(left == "TRACKBALL22")
  {
    myControllers[leftPort] = new TrackBall(Controller::Left, myEvent, *mySystem,
                                            Controller::TrackBall22);
  }
  else if(left == "TRACKBALL80")
  {
    myControllers[leftPort] = new TrackBall(Controller::Left, myEvent, *mySystem,
                                            Controller::TrackBall80);
  }
  else if(left == "AMIGAMOUSE")
  {
    myControllers[leftPort] = new TrackBall(Controller::Left, myEvent, *mySystem,
                                            Controller::AmigaMouse);
  }
  else if(left == "GENESIS")
  {
    myControllers[leftPort] = new Genesis(Controller::Left, myEvent, *mySystem);
  }
  else if(left == "MINDLINK")
  {
    myControllers[leftPort] = new MindLink(Controller::Left, myEvent, *mySystem);
  }
  else
  {
    myControllers[leftPort] = new Joystick(Controller::Left, myEvent, *mySystem);
  }

  // Construct right controller
 #ifndef TARGET_GNW
  if(right == "BOOSTERGRIP")
  {
    myControllers[rightPort] = new BoosterGrip(Controller::Right, myEvent, *mySystem);
  }
  else if(right == "DRIVING")
  {
    myControllers[rightPort] = new Driving(Controller::Right, myEvent, *mySystem);
  }
  else if((right == "KEYBOARD") || (right == "KEYPAD"))
  {
    myControllers[rightPort] = new Keyboard(Controller::Right, myEvent, *mySystem);
  }
  else if(BSPF_startsWithIgnoreCase(right, "PADDLES"))
  {
    bool swapAxis = false, swapDir = false;
    if(right == "PADDLES_IAXIS")
      swapAxis = true;
    else if(right == "PADDLES_IDIR")
      swapDir = true;
    else if(right == "PADDLES_IAXDR")
      swapAxis = swapDir = true;
    myControllers[rightPort] =
      new Paddles(Controller::Right, myEvent, *mySystem,
                  swapPaddles, swapAxis, swapDir);
  }
  else if(right == "TRACKBALL22")
  {
    myControllers[rightPort] = new TrackBall(Controller::Left, myEvent, *mySystem,
                                             Controller::TrackBall22);
  }
  else if(right == "TRACKBALL80")
  {
    myControllers[rightPort] = new TrackBall(Controller::Left, myEvent, *mySystem,
                                             Controller::TrackBall80);
  }
  else if(right == "AMIGAMOUSE")
  {
    myControllers[rightPort] = new TrackBall(Controller::Left, myEvent, *mySystem,
                                             Controller::AmigaMouse);
  }
  else if(right == "ATARIVOX")
  {
    const string& nvramfile = myOSystem->nvramDir() + "atarivox_eeprom.dat";
    myControllers[rightPort] = new AtariVox(Controller::Right, myEvent,
                   *mySystem, myOSystem->serialPort(),
                   myOSystem->settings().getString("avoxport"), nvramfile);
  }
  else if(right == "SAVEKEY")
  {
    const string& nvramfile = myOSystem->nvramDir() + "savekey_eeprom.dat";
    myControllers[rightPort] = new SaveKey(Controller::Right, myEvent, *mySystem,
                                           nvramfile);
  }
  else if(right == "GENESIS")
  {
    myControllers[rightPort] = new Genesis(Controller::Right, myEvent, *mySystem);
  }
  else if(right == "KIDVID")
  {
    myControllers[rightPort] = new KidVid(Controller::Right, myEvent, *mySystem, rommd5);
  }
  else if(right == "MINDLINK")
  {
    myControllers[rightPort] = new MindLink(Controller::Right, myEvent, *mySystem);
  }
  else
#endif
  {
    myControllers[rightPort] = new Joystick(Controller::Right, myEvent, *mySystem);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef TARGET_GNW
void Console::loadUserPalette()
{
  const string& palette = myOSystem->paletteFile();
  ifstream in(palette.c_str(), ios::binary);
  if(!in)
    return;

  // Make sure the contains enough data for the NTSC, PAL and SECAM palettes
  // This means 128 colours each for NTSC and PAL, at 3 bytes per pixel
  // and 8 colours for SECAM at 3 bytes per pixel
  in.seekg(0, ios::end);
  streampos length = in.tellg();
  in.seekg(0, ios::beg);
  if(length < 128 * 3 * 2 + 8 * 3)
  {
    in.close();
    return;
  }

  // Now that we have valid data, create the user-defined palettes
  uInt8 pixbuf[3];  // Temporary buffer for one 24-bit pixel

  for(int i = 0; i < 128; i++)  // NTSC palette
  {
    in.read((char*)pixbuf, 3);
    uInt32 pixel = ((int)pixbuf[0] << R_SHIFT) + ((int)pixbuf[1] << G_SHIFT) + (int)pixbuf[2] << B_SHIFT;
    ourUserNTSCPalette[(i<<1)] = pixel;
  }
  for(int i = 0; i < 128; i++)  // PAL palette
  {
    in.read((char*)pixbuf, 3);
    uInt32 pixel = ((int)pixbuf[0] << R_SHIFT) + ((int)pixbuf[1] << G_SHIFT) + (int)pixbuf[2] << B_SHIFT;
    ourUserPALPalette[(i<<1)] = pixel;
  }

  uInt32 secam[16];  // All 8 24-bit pixels, plus 8 colorloss pixels
  for(int i = 0; i < 8; i++)    // SECAM palette
  {
    in.read((char*)pixbuf, 3);
    uInt32 pixel = ((int)pixbuf[0] << R_SHIFT) + ((int)pixbuf[1] << G_SHIFT) + (int)pixbuf[2] << B_SHIFT;
    secam[(i<<1)]   = pixel;
    secam[(i<<1)+1] = 0;
  }
  uInt32* ptr = ourUserSECAMPalette;
  for(int i = 0; i < 16; ++i)
  {
    uInt32* s = secam;
    for(int j = 0; j < 16; ++j)
      *ptr++ = *s++;
  }

  in.close();
  myUserPaletteDefined = true;
}
#endif

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Console::setColorLossPalette()
{
  // Look at all the palettes, since we don't know which one is
  // currently active
  uInt32* palette[9] = {
    &ourNTSCPalette[0],    &ourPALPalette[0],    &ourSECAMPalette[0],
    &ourNTSCPaletteZ26[0], &ourPALPaletteZ26[0], &ourSECAMPaletteZ26[0],
    0, 0, 0
  };
  if(myUserPaletteDefined)
  {
    palette[6] = &ourUserNTSCPalette[0];
    palette[7] = &ourUserPALPalette[0];
    palette[8] = &ourUserSECAMPalette[0];
  }

  for(int i = 0; i < 9; ++i)
  {
    if(palette[i] == 0)
      continue;

    // Fill the odd numbered palette entries with gray values (calculated
    // using the standard RGB -> grayscale conversion formula)
    for(int j = 0; j < 128; ++j)
    {
      uInt32 pixel = palette[i][(j<<1)];
      uInt8 r = (pixel >> R_SHIFT) & 0xff;
      uInt8 g = (pixel >> G_SHIFT)  & 0xff;
      uInt8 b = (pixel >> B_SHIFT)  & 0xff;
      uInt8 sum = (uInt8) (((float)r * 0.2989) +
                           ((float)g * 0.5870) +
                           ((float)b * 0.1140));
      palette[i][(j<<1)+1] = (sum << 16) + (sum << 8) + sum;
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Console::setFramerate(float framerate)
{
  myFramerate = framerate;
  myOSystem->setFramerate(framerate);
#ifndef TARGET_GNW
  myOSystem->sound().setFrameRate(framerate);
#endif
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Console::toggleTIABit(TIABit bit, const string& bitname, bool show) const
{
  myTIA->toggleBit(bit);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Console::toggleBits() const
{
  myTIA->toggleBits();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Console::toggleTIACollision(TIABit bit, const string& bitname, bool show) const
{
  myTIA->toggleCollision(bit);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Console::toggleCollisions() const
{
  myTIA->toggleCollisions();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Console::toggleHMOVE() const
{
  myTIA->toggleHMOVEBlank();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Console::toggleFixedColors() const
{
  myTIA->toggleFixedColors();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Console::addDebugger()
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Console::stateChanged(EventHandler::State state)
{
#ifndef TARGET_GNW
  // For now, only the CompuMate cares about state changes
  if(myCMHandler)
    myCMHandler->enableKeyHandling(state == EventHandler::S_EMULATE);
#endif
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt32 Console::ourNTSCPalette[256] = {
#if defined(XBGR8888)
  0x000000, 0, 0x4a4a4a, 0, 0x6f6f6f, 0, 0x8e8e8e, 0,
  0xaaaaaa, 0, 0xc0c0c0, 0, 0xd6d6d6, 0, 0xececec, 0,
  0x004848, 0, 0x0f6969, 0, 0x1d8686, 0, 0x2aa2a2, 0,
  0x35bbbb, 0, 0x40d2d2, 0, 0x4ae8e8, 0, 0x54fcfc, 0,
  0x002c7c, 0, 0x114890, 0, 0x2162a2, 0, 0x307ab4, 0,
  0x3d90c3, 0, 0x4aa4d2, 0, 0x55b7df, 0, 0x60c8ec, 0,
  0x001c90, 0, 0x1539a3, 0, 0x2853b5, 0, 0x3a6cc6, 0,
  0x4a82d5, 0, 0x5997e3, 0, 0x67aaf0, 0, 0x74bcfc, 0,
  0x000094, 0, 0x1a1aa7, 0, 0x3232b8, 0, 0x4848c8, 0,
  0x5c5cd6, 0, 0x6f6fe4, 0, 0x8080f0, 0, 0x9090fc, 0,
  0x640084, 0, 0x7a1997, 0, 0x8f30a8, 0, 0xa246b8, 0,
  0xb359c6, 0, 0xc36cd4, 0, 0xd27ce0, 0, 0xe08cec, 0,
  0x840050, 0, 0x9a1968, 0, 0xad307d, 0, 0xc04692, 0,
  0xd059a4, 0, 0xe06cb5, 0, 0xee7cc5, 0, 0xfc8cd4, 0,
  0x900014, 0, 0xa31a33, 0, 0xb5324e, 0, 0xc64868, 0,
  0xd55c7f, 0, 0xe36f95, 0, 0xf080a9, 0, 0xfc90bc, 0,
  0x940000, 0, 0xa71a18, 0, 0xb8322d, 0, 0xc84842, 0,
  0xd65c54, 0, 0xe46f65, 0, 0xf08075, 0, 0xfc9084, 0,
  0x881c00, 0, 0x9d3b18, 0, 0xb0572d, 0, 0xc27242, 0,
  0xd28a54, 0, 0xe1a065, 0, 0xefb575, 0, 0xfcc884, 0,
  0x643000, 0, 0x805018, 0, 0x986d2d, 0, 0xb08842, 0,
  0xc5a054, 0, 0xd9b765, 0, 0xebcc75, 0, 0xfce084, 0,
  0x304000, 0, 0x4e6218, 0, 0x69812d, 0, 0x829e42, 0,
  0x99b854, 0, 0xaed165, 0, 0xc2e775, 0, 0xd4fc84, 0,
  0x004400, 0, 0x1a661a, 0, 0x328432, 0, 0x48a048, 0,
  0x5cba5c, 0, 0x6fd26f, 0, 0x80e880, 0, 0x90fc90, 0,
  0x003c14, 0, 0x185f35, 0, 0x2d7e52, 0, 0x429c6e, 0,
  0x54b787, 0, 0x65d09e, 0, 0x75e7b4, 0, 0x84fcc8, 0,
  0x003830, 0, 0x165950, 0, 0x2b766d, 0, 0x3e9288, 0,
  0x4faba0, 0, 0x5fc2b7, 0, 0x6ed8cc, 0, 0x7cece0, 0,
  0x002c48, 0, 0x144d69, 0, 0x266a86, 0, 0x3886a2, 0,
  0x479fbb, 0, 0x56b6d2, 0, 0x63cce8, 0, 0x70e0fc, 0
#else
  0x000000, 0, 0x4a4a4a, 0, 0x6f6f6f, 0, 0x8e8e8e, 0,
  0xaaaaaa, 0, 0xc0c0c0, 0, 0xd6d6d6, 0, 0xececec, 0,
  0x484800, 0, 0x69690f, 0, 0x86861d, 0, 0xa2a22a, 0,
  0xbbbb35, 0, 0xd2d240, 0, 0xe8e84a, 0, 0xfcfc54, 0,
  0x7c2c00, 0, 0x904811, 0, 0xa26221, 0, 0xb47a30, 0,
  0xc3903d, 0, 0xd2a44a, 0, 0xdfb755, 0, 0xecc860, 0,
  0x901c00, 0, 0xa33915, 0, 0xb55328, 0, 0xc66c3a, 0,
  0xd5824a, 0, 0xe39759, 0, 0xf0aa67, 0, 0xfcbc74, 0,
  0x940000, 0, 0xa71a1a, 0, 0xb83232, 0, 0xc84848, 0,
  0xd65c5c, 0, 0xe46f6f, 0, 0xf08080, 0, 0xfc9090, 0,
  0x840064, 0, 0x97197a, 0, 0xa8308f, 0, 0xb846a2, 0,
  0xc659b3, 0, 0xd46cc3, 0, 0xe07cd2, 0, 0xec8ce0, 0,
  0x500084, 0, 0x68199a, 0, 0x7d30ad, 0, 0x9246c0, 0,
  0xa459d0, 0, 0xb56ce0, 0, 0xc57cee, 0, 0xd48cfc, 0,
  0x140090, 0, 0x331aa3, 0, 0x4e32b5, 0, 0x6848c6, 0,
  0x7f5cd5, 0, 0x956fe3, 0, 0xa980f0, 0, 0xbc90fc, 0,
  0x000094, 0, 0x181aa7, 0, 0x2d32b8, 0, 0x4248c8, 0,
  0x545cd6, 0, 0x656fe4, 0, 0x7580f0, 0, 0x8490fc, 0,
  0x001c88, 0, 0x183b9d, 0, 0x2d57b0, 0, 0x4272c2, 0,
  0x548ad2, 0, 0x65a0e1, 0, 0x75b5ef, 0, 0x84c8fc, 0,
  0x003064, 0, 0x185080, 0, 0x2d6d98, 0, 0x4288b0, 0,
  0x54a0c5, 0, 0x65b7d9, 0, 0x75cceb, 0, 0x84e0fc, 0,
  0x004030, 0, 0x18624e, 0, 0x2d8169, 0, 0x429e82, 0,
  0x54b899, 0, 0x65d1ae, 0, 0x75e7c2, 0, 0x84fcd4, 0,
  0x004400, 0, 0x1a661a, 0, 0x328432, 0, 0x48a048, 0,
  0x5cba5c, 0, 0x6fd26f, 0, 0x80e880, 0, 0x90fc90, 0,
  0x143c00, 0, 0x355f18, 0, 0x527e2d, 0, 0x6e9c42, 0,
  0x87b754, 0, 0x9ed065, 0, 0xb4e775, 0, 0xc8fc84, 0,
  0x303800, 0, 0x505916, 0, 0x6d762b, 0, 0x88923e, 0,
  0xa0ab4f, 0, 0xb7c25f, 0, 0xccd86e, 0, 0xe0ec7c, 0,
  0x482c00, 0, 0x694d14, 0, 0x866a26, 0, 0xa28638, 0,
  0xbb9f47, 0, 0xd2b656, 0, 0xe8cc63, 0, 0xfce070, 0
#endif
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt32 Console::ourPALPalette[256] = {
#if defined(XBGR8888)
  0x000000, 0, 0x2b2b2b, 0, 0x525252, 0, 0x767676, 0,
  0x979797, 0, 0xb6b6b6, 0, 0xd2d2d2, 0, 0xececec, 0,
  0x000000, 0, 0x2b2b2b, 0, 0x525252, 0, 0x767676, 0,
  0x979797, 0, 0xb6b6b6, 0, 0xd2d2d2, 0, 0xececec, 0,
  0x005880, 0, 0x1a7196, 0, 0x3287ab, 0, 0x489cbe, 0,
  0x5cafcf, 0, 0x6fc0df, 0, 0x80d1ee, 0, 0x90e0fc, 0,
  0x005c44, 0, 0x1a795e, 0, 0x329376, 0, 0x48ac8c, 0,
  0x5cc2a0, 0, 0x6fd7b3, 0, 0x80eac4, 0, 0x90fcd4, 0,
  0x003470, 0, 0x1a5189, 0, 0x326ba0, 0, 0x4884b6, 0,
  0x5c9ac9, 0, 0x6fafdc, 0, 0x80c2ec, 0, 0x90d4fc, 0,
  0x146400, 0, 0x35801a, 0, 0x529832, 0, 0x6eb048, 0,
  0x87c55c, 0, 0x9ed96f, 0, 0xb4eb80, 0, 0xc8fc90, 0,
  0x140070, 0, 0x351a89, 0, 0x5232a0, 0, 0x6e48b6, 0,
  0x875cc9, 0, 0x9e6fdc, 0, 0xb480ec, 0, 0xc890fc, 0,
  0x5c5c00, 0, 0x76761a, 0, 0x8e8e32, 0, 0xa4a448, 0,
  0xb8b85c, 0, 0xcbcb6f, 0, 0xdcdc80, 0, 0xecec90, 0,
  0x5c0070, 0, 0x741a84, 0, 0x893296, 0, 0x9e48a8, 0,
  0xb05cb7, 0, 0xc16fc6, 0, 0xd180d3, 0, 0xe090e0, 0,
  0x703c00, 0, 0x895a19, 0, 0xa0752f, 0, 0xb68e44, 0,
  0xc9a557, 0, 0xdcba68, 0, 0xecce79, 0, 0xfce088, 0,
  0x700058, 0, 0x891a6e, 0, 0xa03283, 0, 0xb64896, 0,
  0xc95ca7, 0, 0xdc6fb7, 0, 0xec80c6, 0, 0xfc90d4, 0,
  0x702000, 0, 0x893f19, 0, 0xa05a2f, 0, 0xb67444, 0,
  0xc98b57, 0, 0xdca168, 0, 0xecb579, 0, 0xfcc888, 0,
  0x800034, 0, 0x961a4a, 0, 0xab325f, 0, 0xbe4872, 0,
  0xcf5c83, 0, 0xdf6f93, 0, 0xee80a2, 0, 0xfc90b0, 0,
  0x880000, 0, 0x9d1a1a, 0, 0xb03232, 0, 0xc24848, 0,
  0xd25c5c, 0, 0xe16f6f, 0, 0xef8080, 0, 0xfc9090, 0,
  0x000000, 0, 0x2b2b2b, 0, 0x525252, 0, 0x767676, 0,
  0x979797, 0, 0xb6b6b6, 0, 0xd2d2d2, 0, 0xececec, 0,
  0x000000, 0, 0x2b2b2b, 0, 0x525252, 0, 0x767676, 0,
  0x979797, 0, 0xb6b6b6, 0, 0xd2d2d2, 0, 0xececec, 0
#else
  0x000000, 0, 0x2b2b2b, 0, 0x525252, 0, 0x767676, 0,
  0x979797, 0, 0xb6b6b6, 0, 0xd2d2d2, 0, 0xececec, 0,
  0x000000, 0, 0x2b2b2b, 0, 0x525252, 0, 0x767676, 0,
  0x979797, 0, 0xb6b6b6, 0, 0xd2d2d2, 0, 0xececec, 0,
  0x805800, 0, 0x96711a, 0, 0xab8732, 0, 0xbe9c48, 0,
  0xcfaf5c, 0, 0xdfc06f, 0, 0xeed180, 0, 0xfce090, 0,
  0x445c00, 0, 0x5e791a, 0, 0x769332, 0, 0x8cac48, 0,
  0xa0c25c, 0, 0xb3d76f, 0, 0xc4ea80, 0, 0xd4fc90, 0,
  0x703400, 0, 0x89511a, 0, 0xa06b32, 0, 0xb68448, 0,
  0xc99a5c, 0, 0xdcaf6f, 0, 0xecc280, 0, 0xfcd490, 0,
  0x006414, 0, 0x1a8035, 0, 0x329852, 0, 0x48b06e, 0,
  0x5cc587, 0, 0x6fd99e, 0, 0x80ebb4, 0, 0x90fcc8, 0,
  0x700014, 0, 0x891a35, 0, 0xa03252, 0, 0xb6486e, 0,
  0xc95c87, 0, 0xdc6f9e, 0, 0xec80b4, 0, 0xfc90c8, 0,
  0x005c5c, 0, 0x1a7676, 0, 0x328e8e, 0, 0x48a4a4, 0,
  0x5cb8b8, 0, 0x6fcbcb, 0, 0x80dcdc, 0, 0x90ecec, 0,
  0x70005c, 0, 0x841a74, 0, 0x963289, 0, 0xa8489e, 0,
  0xb75cb0, 0, 0xc66fc1, 0, 0xd380d1, 0, 0xe090e0, 0,
  0x003c70, 0, 0x195a89, 0, 0x2f75a0, 0, 0x448eb6, 0,
  0x57a5c9, 0, 0x68badc, 0, 0x79ceec, 0, 0x88e0fc, 0,
  0x580070, 0, 0x6e1a89, 0, 0x8332a0, 0, 0x9648b6, 0,
  0xa75cc9, 0, 0xb76fdc, 0, 0xc680ec, 0, 0xd490fc, 0,
  0x002070, 0, 0x193f89, 0, 0x2f5aa0, 0, 0x4474b6, 0,
  0x578bc9, 0, 0x68a1dc, 0, 0x79b5ec, 0, 0x88c8fc, 0,
  0x340080, 0, 0x4a1a96, 0, 0x5f32ab, 0, 0x7248be, 0,
  0x835ccf, 0, 0x936fdf, 0, 0xa280ee, 0, 0xb090fc, 0,
  0x000088, 0, 0x1a1a9d, 0, 0x3232b0, 0, 0x4848c2, 0,
  0x5c5cd2, 0, 0x6f6fe1, 0, 0x8080ef, 0, 0x9090fc, 0,
  0x000000, 0, 0x2b2b2b, 0, 0x525252, 0, 0x767676, 0,
  0x979797, 0, 0xb6b6b6, 0, 0xd2d2d2, 0, 0xececec, 0,
  0x000000, 0, 0x2b2b2b, 0, 0x525252, 0, 0x767676, 0,
  0x979797, 0, 0xb6b6b6, 0, 0xd2d2d2, 0, 0xececec, 0
#endif
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt32 Console::ourSECAMPalette[256] = {
#if defined(XBGR8888)
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff50ff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff50ff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff50ff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff50ff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff50ff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff50ff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff50ff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff50ff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff50ff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff50ff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff50ff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff50ff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff50ff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff50ff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff50ff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff50ff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0
#else
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff50ff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff50ff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff50ff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff50ff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff50ff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff50ff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff50ff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff50ff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff50ff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff50ff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff50ff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff50ff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff50ff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff50ff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff50ff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff50ff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0
#endif
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt32 Console::ourNTSCPaletteZ26[256] = {
#if defined(XBGR8888)
  0x000000, 0, 0x505050, 0, 0x646464, 0, 0x787878, 0,
  0x8c8c8c, 0, 0xa0a0a0, 0, 0xb4b4b4, 0, 0xc8c8c8, 0,
  0x005444, 0, 0x006858, 0, 0x007c6c, 0, 0x009080, 0,
  0x14a494, 0, 0x28b8a8, 0, 0x3cccbc, 0, 0x50e0d0, 0,
  0x003967, 0, 0x004d7b, 0, 0x00618f, 0, 0x1375a3, 0,
  0x2789b7, 0, 0x3b9dcb, 0, 0x4fb1df, 0, 0x63c5f3, 0,
  0x04257b, 0, 0x18398f, 0, 0x2c4da3, 0, 0x4061b7, 0,
  0x5475cb, 0, 0x6889df, 0, 0x7c9df3, 0, 0x90b1ff, 0,
  0x2c127d, 0, 0x402691, 0, 0x543aa5, 0, 0x684eb9, 0,
  0x7c62cd, 0, 0x9076e1, 0, 0xa48af5, 0, 0xb89eff, 0,
  0x710873, 0, 0x851c87, 0, 0x99309b, 0, 0xad44af, 0,
  0xc158c3, 0, 0xd56cd7, 0, 0xe980eb, 0, 0xfd94ff, 0,
  0x920b5d, 0, 0xa61f71, 0, 0xba3385, 0, 0xce4799, 0,
  0xe25bad, 0, 0xf66fc1, 0, 0xff83d5, 0, 0xff97e9, 0,
  0x991540, 0, 0xad2954, 0, 0xc13d68, 0, 0xd5517c, 0,
  0xe96590, 0, 0xfd79a4, 0, 0xff8db8, 0, 0xffa1cc, 0,
  0x932525, 0, 0xa73939, 0, 0xbb4d4d, 0, 0xcf6161, 0,
  0xe37575, 0, 0xf78989, 0, 0xff9d9d, 0, 0xffb1b1, 0,
  0x80340f, 0, 0x944823, 0, 0xa85c37, 0, 0xbc704b, 0,
  0xd0845f, 0, 0xe49873, 0, 0xf8ac87, 0, 0xffc09b, 0,
  0x5a4204, 0, 0x6e5618, 0, 0x826a2c, 0, 0x967e40, 0,
  0xaa9254, 0, 0xbea668, 0, 0xd2ba7c, 0, 0xe6ce90, 0,
  0x304f04, 0, 0x446318, 0, 0x58772c, 0, 0x6c8b40, 0,
  0x809f54, 0, 0x94b368, 0, 0xa8c77c, 0, 0xbcdb90, 0,
  0x0a550f, 0, 0x1e6923, 0, 0x327d37, 0, 0x46914b, 0,
  0x5aa55f, 0, 0x6eb973, 0, 0x82cd87, 0, 0x96e19b, 0,
  0x00511f, 0, 0x056533, 0, 0x197947, 0, 0x2d8d5b, 0,
  0x41a16f, 0, 0x55b583, 0, 0x69c997, 0, 0x7dddab, 0,
  0x004634, 0, 0x005a48, 0, 0x146e5c, 0, 0x288270, 0,
  0x3c9684, 0, 0x50aa98, 0, 0x64beac, 0, 0x78d2c0, 0,
  0x003e46, 0, 0x05525a, 0, 0x19666e, 0, 0x2d7a82, 0,
  0x418e96, 0, 0x55a2aa, 0, 0x69b6be, 0, 0x7dcad2, 0
#else
  0x000000, 0, 0x505050, 0, 0x646464, 0, 0x787878, 0,
  0x8c8c8c, 0, 0xa0a0a0, 0, 0xb4b4b4, 0, 0xc8c8c8, 0,
  0x445400, 0, 0x586800, 0, 0x6c7c00, 0, 0x809000, 0,
  0x94a414, 0, 0xa8b828, 0, 0xbccc3c, 0, 0xd0e050, 0,
  0x673900, 0, 0x7b4d00, 0, 0x8f6100, 0, 0xa37513, 0,
  0xb78927, 0, 0xcb9d3b, 0, 0xdfb14f, 0, 0xf3c563, 0,
  0x7b2504, 0, 0x8f3918, 0, 0xa34d2c, 0, 0xb76140, 0,
  0xcb7554, 0, 0xdf8968, 0, 0xf39d7c, 0, 0xffb190, 0,
  0x7d122c, 0, 0x912640, 0, 0xa53a54, 0, 0xb94e68, 0,
  0xcd627c, 0, 0xe17690, 0, 0xf58aa4, 0, 0xff9eb8, 0,
  0x730871, 0, 0x871c85, 0, 0x9b3099, 0, 0xaf44ad, 0,
  0xc358c1, 0, 0xd76cd5, 0, 0xeb80e9, 0, 0xff94fd, 0,
  0x5d0b92, 0, 0x711fa6, 0, 0x8533ba, 0, 0x9947ce, 0,
  0xad5be2, 0, 0xc16ff6, 0, 0xd583ff, 0, 0xe997ff, 0,
  0x401599, 0, 0x5429ad, 0, 0x683dc1, 0, 0x7c51d5, 0,
  0x9065e9, 0, 0xa479fd, 0, 0xb88dff, 0, 0xcca1ff, 0,
  0x252593, 0, 0x3939a7, 0, 0x4d4dbb, 0, 0x6161cf, 0,
  0x7575e3, 0, 0x8989f7, 0, 0x9d9dff, 0, 0xb1b1ff, 0,
  0x0f3480, 0, 0x234894, 0, 0x375ca8, 0, 0x4b70bc, 0,
  0x5f84d0, 0, 0x7398e4, 0, 0x87acf8, 0, 0x9bc0ff, 0,
  0x04425a, 0, 0x18566e, 0, 0x2c6a82, 0, 0x407e96, 0,
  0x5492aa, 0, 0x68a6be, 0, 0x7cbad2, 0, 0x90cee6, 0,
  0x044f30, 0, 0x186344, 0, 0x2c7758, 0, 0x408b6c, 0,
  0x549f80, 0, 0x68b394, 0, 0x7cc7a8, 0, 0x90dbbc, 0,
  0x0f550a, 0, 0x23691e, 0, 0x377d32, 0, 0x4b9146, 0,
  0x5fa55a, 0, 0x73b96e, 0, 0x87cd82, 0, 0x9be196, 0,
  0x1f5100, 0, 0x336505, 0, 0x477919, 0, 0x5b8d2d, 0,
  0x6fa141, 0, 0x83b555, 0, 0x97c969, 0, 0xabdd7d, 0,
  0x344600, 0, 0x485a00, 0, 0x5c6e14, 0, 0x708228, 0,
  0x84963c, 0, 0x98aa50, 0, 0xacbe64, 0, 0xc0d278, 0,
  0x463e00, 0, 0x5a5205, 0, 0x6e6619, 0, 0x827a2d, 0,
  0x968e41, 0, 0xaaa255, 0, 0xbeb669, 0, 0xd2ca7d, 0
#endif
}; 


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt32 Console::ourPALPaletteZ26[256] = {
#if defined(XBGR8888)
  0x000000, 0, 0x4c4c4c, 0, 0x606060, 0, 0x747474, 0,
  0x888888, 0, 0x9c9c9c, 0, 0xb0b0b0, 0, 0xc4c4c4, 0,
  0x000000, 0, 0x4c4c4c, 0, 0x606060, 0, 0x747474, 0,
  0x888888, 0, 0x9c9c9c, 0, 0xb0b0b0, 0, 0xc4c4c4, 0,
  0x003a53, 0, 0x004e67, 0, 0x03627b, 0, 0x17768f, 0,
  0x2b8aa3, 0, 0x3f9eb7, 0, 0x53b2cb, 0, 0x67c6df, 0,
  0x00581b, 0, 0x006c2f, 0, 0x018043, 0, 0x159457, 0,
  0x29a86b, 0, 0x3dbc7f, 0, 0x51d093, 0, 0x65e4a7, 0,
  0x00296a, 0, 0x123d7e, 0, 0x265192, 0, 0x3a65a6, 0,
  0x4e79ba, 0, 0x628dce, 0, 0x76a1e2, 0, 0x8ab5f6, 0,
  0x005b07, 0, 0x116f1b, 0, 0x25832f, 0, 0x399743, 0,
  0x4dab57, 0, 0x61bf6b, 0, 0x75d37f, 0, 0x89e793, 0,
  0x2f1b74, 0, 0x432f88, 0, 0x57439c, 0, 0x6b57b0, 0,
  0x7f6bc4, 0, 0x937fd8, 0, 0xa793ec, 0, 0xbba7ff, 0,
  0x2e5700, 0, 0x426b10, 0, 0x567f24, 0, 0x6a9338, 0,
  0x7ea74c, 0, 0x92bb60, 0, 0xa6cf74, 0, 0xbae388, 0,
  0x5f166d, 0, 0x732a81, 0, 0x873e95, 0, 0x9b52a9, 0,
  0xaf66bd, 0, 0xc37ad1, 0, 0xd78ee5, 0, 0xeba2f9, 0,
  0x5e4c01, 0, 0x726015, 0, 0x867429, 0, 0x9a883d, 0,
  0xae9c51, 0, 0xc2b065, 0, 0xd6c479, 0, 0xead88d, 0,
  0x88155f, 0, 0x9c2973, 0, 0xb03d87, 0, 0xc4519b, 0,
  0xd865af, 0, 0xec79c3, 0, 0xff8dd7, 0, 0xffa1eb, 0,
  0x873b12, 0, 0x9b4f26, 0, 0xaf633a, 0, 0xc3774e, 0,
  0xd78b62, 0, 0xeb9f76, 0, 0xffb38a, 0, 0xffc79e, 0,
  0x9d1e45, 0, 0xb13259, 0, 0xc5466d, 0, 0xd95a81, 0,
  0xed6e95, 0, 0xff82a9, 0, 0xff96bd, 0, 0xffaad1, 0,
  0x9e2b2a, 0, 0xb23f3e, 0, 0xc65352, 0, 0xda6766, 0,
  0xee7b7a, 0, 0xff8f8e, 0, 0xffa3a2, 0, 0xffb7b6, 0,
  0x000000, 0, 0x4c4c4c, 0, 0x606060, 0, 0x747474, 0,
  0x888888, 0, 0x9c9c9c, 0, 0xb0b0b0, 0, 0xc4c4c4, 0,
  0x000000, 0, 0x4c4c4c, 0, 0x606060, 0, 0x747474, 0,
  0x888888, 0, 0x9c9c9c, 0, 0xb0b0b0, 0, 0xc4c4c4, 0
#else
  0x000000, 0, 0x4c4c4c, 0, 0x606060, 0, 0x747474, 0,
  0x888888, 0, 0x9c9c9c, 0, 0xb0b0b0, 0, 0xc4c4c4, 0,
  0x000000, 0, 0x4c4c4c, 0, 0x606060, 0, 0x747474, 0,
  0x888888, 0, 0x9c9c9c, 0, 0xb0b0b0, 0, 0xc4c4c4, 0,
  0x533a00, 0, 0x674e00, 0, 0x7b6203, 0, 0x8f7617, 0,
  0xa38a2b, 0, 0xb79e3f, 0, 0xcbb253, 0, 0xdfc667, 0,
  0x1b5800, 0, 0x2f6c00, 0, 0x438001, 0, 0x579415, 0,
  0x6ba829, 0, 0x7fbc3d, 0, 0x93d051, 0, 0xa7e465, 0,
  0x6a2900, 0, 0x7e3d12, 0, 0x925126, 0, 0xa6653a, 0,
  0xba794e, 0, 0xce8d62, 0, 0xe2a176, 0, 0xf6b58a, 0,
  0x075b00, 0, 0x1b6f11, 0, 0x2f8325, 0, 0x439739, 0,
  0x57ab4d, 0, 0x6bbf61, 0, 0x7fd375, 0, 0x93e789, 0,
  0x741b2f, 0, 0x882f43, 0, 0x9c4357, 0, 0xb0576b, 0,
  0xc46b7f, 0, 0xd87f93, 0, 0xec93a7, 0, 0xffa7bb, 0,
  0x00572e, 0, 0x106b42, 0, 0x247f56, 0, 0x38936a, 0,
  0x4ca77e, 0, 0x60bb92, 0, 0x74cfa6, 0, 0x88e3ba, 0,
  0x6d165f, 0, 0x812a73, 0, 0x953e87, 0, 0xa9529b, 0,
  0xbd66af, 0, 0xd17ac3, 0, 0xe58ed7, 0, 0xf9a2eb, 0,
  0x014c5e, 0, 0x156072, 0, 0x297486, 0, 0x3d889a, 0,
  0x519cae, 0, 0x65b0c2, 0, 0x79c4d6, 0, 0x8dd8ea, 0,
  0x5f1588, 0, 0x73299c, 0, 0x873db0, 0, 0x9b51c4, 0,
  0xaf65d8, 0, 0xc379ec, 0, 0xd78dff, 0, 0xeba1ff, 0,
  0x123b87, 0, 0x264f9b, 0, 0x3a63af, 0, 0x4e77c3, 0,
  0x628bd7, 0, 0x769feb, 0, 0x8ab3ff, 0, 0x9ec7ff, 0,
  0x451e9d, 0, 0x5932b1, 0, 0x6d46c5, 0, 0x815ad9, 0,
  0x956eed, 0, 0xa982ff, 0, 0xbd96ff, 0, 0xd1aaff, 0,
  0x2a2b9e, 0, 0x3e3fb2, 0, 0x5253c6, 0, 0x6667da, 0,
  0x7a7bee, 0, 0x8e8fff, 0, 0xa2a3ff, 0, 0xb6b7ff, 0,
  0x000000, 0, 0x4c4c4c, 0, 0x606060, 0, 0x747474, 0,
  0x888888, 0, 0x9c9c9c, 0, 0xb0b0b0, 0, 0xc4c4c4, 0,
  0x000000, 0, 0x4c4c4c, 0, 0x606060, 0, 0x747474, 0,
  0x888888, 0, 0x9c9c9c, 0, 0xb0b0b0, 0, 0xc4c4c4, 0
  #endif
}; 

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt32 Console::ourSECAMPaletteZ26[256] = {
#if defined(XBGR8888)
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff3cff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff3cff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff3cff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff3cff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff3cff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff3cff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff3cff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff3cff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff3cff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff3cff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff3cff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff3cff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff3cff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff3cff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff3cff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0, 
  0x000000, 0, 0xff2121, 0, 0x793cf0, 0, 0xff3cff, 0, 
  0x00ff7f, 0, 0xffff7f, 0, 0x3fffff, 0, 0xffffff, 0
#else
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff3cff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff3cff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff3cff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff3cff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff3cff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff3cff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff3cff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff3cff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff3cff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff3cff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff3cff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff3cff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff3cff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff3cff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff3cff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0, 
  0x000000, 0, 0x2121ff, 0, 0xf03c79, 0, 0xff3cff, 0, 
  0x7fff00, 0, 0x7fffff, 0, 0xffff3f, 0, 0xffffff, 0
#endif
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt32 Console::ourUserNTSCPalette[256]  = { 0 }; // filled from external file

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt32 Console::ourUserPALPalette[256]   = { 0 }; // filled from external file

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt32 Console::ourUserSECAMPalette[256] = { 0 }; // filled from external file

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Console::Console(const Console& console)
  : myOSystem(console.myOSystem),
    myEvent(console.myEvent)
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Console& Console::operator = (const Console&)
{
  return *this;
}
