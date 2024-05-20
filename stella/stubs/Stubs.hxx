#include <ctime>
#include "OSystem.hxx"

OSystem::OSystem()
{
    myNVRamDir     = ".";
    mySettings     = 0;
    mySound        = new Sound(this);
#ifndef TARGET_GNW
    mySerialPort   = new SerialPort();
#endif
    myEventHandler = new EventHandler(this);
#ifndef TARGET_GNW
    myPropSet      = new PropertiesSet(this);
    Paddles::setDigitalSensitivity(50);
    Paddles::setMouseSensitivity(5);
#endif
}

OSystem::~OSystem()
{
    delete mySound;
#ifndef TARGET_GNW
    delete mySerialPort;
    delete myPropSet;
#endif
    delete myEventHandler;
}

bool OSystem::create() { return 1; }
void OSystem::stateChanged(EventHandler::State state) { }

uInt64 OSystem::getTicks() const
{
    return myConsole->tia().getMilliSeconds();
}

EventHandler::EventHandler(OSystem*) { }
EventHandler::~EventHandler() { }
