#include <ctime>
#include "OSystem.hxx"

OSystem::OSystem()
{
    myNVRamDir     = ".";
    mySettings     = 0;
    myFrameBuffer  = new FrameBuffer();
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
    delete myFrameBuffer;
    delete mySound;
#ifndef TARGET_GNW
    delete mySerialPort;
    delete myPropSet;
#endif
    delete myEventHandler;
}

bool OSystem::create() { return 1; }

void OSystem::mainLoop() { }

void OSystem::pollEvent() { }

bool OSystem::queryVideoHardware() { return 1; }

void OSystem::stateChanged(EventHandler::State state) { }

void OSystem::setDefaultJoymap(Event::Type event, EventMode mode) { }

void OSystem::setFramerate(float framerate) { }

uInt64 OSystem::getTicks() const
{
    return myConsole->tia().getMilliSeconds();
}

EventHandler::EventHandler(OSystem*)
{
    
}

EventHandler::~EventHandler()
{
    
}

FrameBuffer::FrameBuffer()
{

}

FrameBuffer::~FrameBuffer()
{

}

FBInitStatus FrameBuffer::initialize(const string& title, uInt32 width, uInt32 height)
{
    return kSuccess;
}
