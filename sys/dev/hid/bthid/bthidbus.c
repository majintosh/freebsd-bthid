#include <sys/param.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/kernel.h>

#include <sys/bitstring.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/mutex.h>

#include <netgraph/bluetooth/include/ng_bluetooth.h>
#include <netgraph/bluetooth/include/ng_hci.h>
#include <netgraph/bluetooth/include/ng_l2cap.h>
#include <netgraph/bluetooth/include/ng_btsocket.h>

#include <sys/conf.h>

#include "bthid.h"
#include "bthidbus.h"

struct bthidbus_softc {
	struct cdev	*cdev;
	device_t dev;
};

static int
bthidbus_probe(device_t dev)
{
	printf("BTHIDBUS PROBED\n");
	return (0);
}


static int
bthidbus_detach(device_t dev)
{
	printf("BTHIDBUS DETACHED\n");
	struct bthidbus_softc *sc = device_get_softc(dev);
	sc->cdev->si_drv1 = NULL;
	destroy_dev(sc->cdev);
	device_delete_children(dev);
	return (0);
}

static device_t
bthidbus_add_child(device_t dev, u_int order, const char *name, int unit)
{
	struct bthid_ivars *ivars;
	device_t child;
	child = device_add_child_ordered(dev, order, name, unit);

	if (child == NULL)
		return child;
	ivars = malloc(sizeof(struct bthid_ivars), M_DEVBUF, M_WAITOK | M_ZERO);
	device_set_ivars(child, ivars);

	return (child);
}

static void
new_connection(device_t bus, struct bthidbus_new_connection *con)
{
	device_t child;
	child = BUS_ADD_CHILD(bus, 0, "bthid", DEVICE_UNIT_ANY);

	if (child == NULL) {
		free(con->rdesc, M_DEVBUF);
		return;
	}
	struct bthid_ivars *ivars = device_get_ivars(child);
	ivars->vendorId = con->vendorId;
	ivars->productId = con->productId;
	ivars->versionId = con->versionId;
	ivars->rdesc = con->rdesc;
	ivars->rdesc_len = con->rdesc_len; // Should probably just make bthid_ivars and bthidbus_new_connection the same struct

	device_probe_and_attach(child);
}

static d_ioctl_t bthidbus_ioctl;

static struct cdevsw bthidbus_cdevsw = {
	.d_version =	D_VERSION,
	.d_ioctl =	bthidbus_ioctl,
	.d_name =	"bthidbus",
};

static int
bthidbus_attach(device_t dev)
{
	struct bthidbus_softc *sc = device_get_softc(dev);
	struct make_dev_args mda;
	make_dev_args_init(&mda);
	mda.mda_devsw = &bthidbus_cdevsw;
	mda.mda_uid = UID_ROOT;
	mda.mda_si_drv1 = sc;


	make_dev_s(&mda, &sc->cdev, "bthidbus%d", device_get_unit(dev));

	return (0);
}

static device_t bthidbus;

static void
bthidbus_identify(driver_t *driver, device_t parent)
{
	printf("BTHIDBUS IDENTIFIED\n");
	if (bthidbus == NULL)
		bthidbus = BUS_ADD_CHILD(parent, 0, "bthidbus", DEVICE_UNIT_ANY);
}


static int
bthidbus_ioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag, struct thread *td) 
{
	struct bthidbus_new_connection *con;
	struct bthidbus_softc *sc = dev->si_drv1;
	switch (cmd) {
		case BTHIDBUS_NEW_CONNECTION:
			con = (struct bthidbus_new_connection *) addr;
			uint8_t *kern_rdesc = malloc(con->rdesc_len, M_DEVBUF, M_WAITOK | M_ZERO);
			int err = copyin(con->rdesc, kern_rdesc, con->rdesc_len);
			if (err != 0) {
				free(kern_rdesc, M_DEVBUF);
			}
			con->rdesc = kern_rdesc;
			new_connection(sc->dev, con);
			return 0;
	}
	return 1;
}
static device_method_t bthidbus_methods[] = {
	DEVMETHOD(device_identify,	bthidbus_identify),
	DEVMETHOD(device_probe,		bthidbus_probe),
	DEVMETHOD(device_attach,	bthidbus_attach),
	DEVMETHOD(device_detach,	bthidbus_detach),

	/* BUS METHODS */
	DEVMETHOD(bus_add_child,	bthidbus_add_child),
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
			device_delete_child(device_get_parent(bthidbus), bthidbus);
			bthidbus = NULL;
			break;
		default:
			break;
	}
	return error;
}

static driver_t bthidbus_driver = {
	"bthidbus",
	bthidbus_methods,
	sizeof(struct bthidbus_softc)
};

DRIVER_MODULE(bthidbus, nexus, bthidbus_driver, bthidbus_modevent, NULL);
MODULE_VERSION(bthidbus, 1);
