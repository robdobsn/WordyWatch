/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Main entry point
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "RaftCoreApp.h"
#include "RegisterSysMods.h"

#include "BusI2C.h"
#include "DeviceFactory.h"
#include "DeviceLEDCharlie.h"
#include "WordyWatch.h"

// Create the app
RaftCoreApp raftCoreApp;

// Entry point
extern "C" void app_main(void)
{
    
    // Register SysMods from RaftSysMods library
    RegisterSysMods::registerSysMods(raftCoreApp.getSysManager());

    // Register BusI2C
    raftBusSystem.registerBus("I2C", BusI2C::createFn);

    // Register charlie LED device
    deviceFactory.registerDevice("LEDCharlie", DeviceLEDCharlie::create);

    // WordyWatch
     raftCoreApp.registerSysMod("WordyWatch", WordyWatch::create, true);

    // Loop forever
    while (1)
    {
        // Loop the app
        raftCoreApp.loop();
    }
}
