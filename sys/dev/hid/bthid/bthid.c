#include <sys/param.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/kernel.h>

static int
bthid_probe(device_t dev)
{
	return (BUS_PROBE_DEFAULT);
}

static int
bthid_attach(device_t dev)
{
	return 0; // This is where we store the ivars in the softc
}

static int
bthid_detach(device_t dev)
{
	return 0;
}

static device_method_t bthid_methods[] = {
	DEVMETHOD(device_probe,		bthid_probe),
	DEVMETHOD(device_attach,	bthid_attach),
	DEVMETHOD(device_detach,	bthid_detach),

	DEVMETHOD_END
};

static driver_t bthid_driver = {
	"bthid",
	bthid_methods,
	0
};

DRIVER_MODULE(bthid, bthidbus, bthid_driver, NULL, NULL);
MODULE_VERSION(bthid, 1);
