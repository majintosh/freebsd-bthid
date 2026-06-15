#include <sys/param.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/kernel.h>

static device_t bthidbus = NULL;

static void
bthidbus_identify(driver_t *driver, device_t parent)
{
	printf("BTHIDBUS IDENTIFIED\n");
	if (bthidbus == NULL)
		bthidbus = BUS_ADD_CHILD(parent, 0, "bthidbus", DEVICE_UNIT_ANY);
}

static int
bthidbus_probe(device_t dev)
{
	printf("BTHIDBUS PROBED\n");
	return (0);
}

static int
bthidbus_attach(device_t dev)
{
	printf("BTHIDBUS ATTACHED\n");
	return (0);
}

static int
bthidbus_detach(device_t dev)
{
	printf("BTHIDBUS DETACHED\n");
	return (0);
}

static device_method_t bthidbus_methods[] = {
	DEVMETHOD(device_identify,	bthidbus_identify),
	DEVMETHOD(device_probe,		bthidbus_probe),
	DEVMETHOD(device_attach,	bthidbus_attach),
	DEVMETHOD(device_detach,	bthidbus_detach),
	DEVMETHOD_END
};

static int
bthidbus_modevent(module_t mod, int type, void *data)
{
	int error = 0;
	switch (type) {
		case MOD_LOAD:
			break;
		case MOD_UNLOAD:
			if (bthidbus!=NULL)
				device_delete_child(device_get_parent(bthidbus), bthidbus);
			break;
		default:
			break;
	}
	return error;
}

static driver_t bthidbus_driver = {
	"bthidbus",
	bthidbus_methods,
	0
};

DRIVER_MODULE(bthidbus, nexus, bthidbus_driver, bthidbus_modevent, NULL);
