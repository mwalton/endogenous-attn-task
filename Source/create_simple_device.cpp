#include "EPICLib/Output_tee_globals.h"
#include "simple_device.h"


// for use in non-dynamically loaded models
Device_base * create_simple_device()
{
	return new simple_device("Simple Device", Normal_out);
}

// the class factory functions to be accessed with dlsym
extern "C" Device_base * create_device() 
{
    return create_simple_device();
}

extern "C" void destroy_device(Device_base * p) 
{
    delete p;
}
