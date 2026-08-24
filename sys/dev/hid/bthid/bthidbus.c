#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bitstring.h>
#include <sys/bus.h>
#include <sys/caprights.h>
#include <sys/capsicum.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <netgraph/bluetooth/include/ng_bluetooth.h>
#include <netgraph/bluetooth/include/ng_hci.h>
#include <netgraph/bluetooth/include/ng_l2cap.h>
#include <netgraph/bluetooth/include/ng_btsocket.h>

#include "bthid.h"
#include "bthidbus.h"

#define RDESC_MAX_LEN 4096

struct bthidbus_softc {
	struct cdev	*cdev;
	device_t	dev;
};

static int
bthidbus_probe(device_t dev)
{
	return (0);
}

static int
bthidbus_detach(device_t dev)
{
	struct bthidbus_softc *sc;

	sc = device_get_softc(dev);
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
		return (child);

	ivars = malloc(sizeof(struct bthid_ivars), M_DEVBUF, M_WAITOK | M_ZERO);
	device_set_ivars(child, ivars);

	return (child);
}

static int
new_connection(device_t bus, struct bthidbus_new_connection *con, struct socket *ctrl_sock,
		struct socket *intr_sock, struct file *ctrl_file, struct file *intr_file)
{
	struct bthid_ivars *ivars;
	device_t child;
	int error;

	child = BUS_ADD_CHILD(bus, 0, "bthid", DEVICE_UNIT_ANY);

	if (child == NULL)
		return (ENOMEM);

	ivars = device_get_ivars(child);
	ivars->vendor_id = con->vendor_id;
	ivars->product_id = con->product_id;
	ivars->version_id = con->version_id;
	ivars->rdesc = con->rdesc;
	ivars->rdesc_len = con->rdesc_len;
	ivars->ctrl_sock = ctrl_sock;
	ivars->intr_sock = intr_sock;
	ivars->ctrl_file = ctrl_file;
	ivars->intr_file = intr_file;

	mtx_lock(&Giant);
	error = device_probe_and_attach(child);
	mtx_unlock(&Giant);
	if (error != 0)
		device_delete_child(bus, child);

	return (error);
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
	struct bthidbus_softc *sc;
	struct make_dev_args mda;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;
	make_dev_args_init(&mda);
	mda.mda_devsw = &bthidbus_cdevsw;
	mda.mda_uid = UID_ROOT;
	mda.mda_si_drv1 = sc;
	mda.mda_gid = GID_WHEEL;
	mda.mda_mode = 0600;

	error = make_dev_s(&mda, &sc->cdev, "bthidbus%d", device_get_unit(dev));
	if (error != 0)
		return (error);

	return (0);
}

static device_t bthidbus;

static void
bthidbus_identify(driver_t *driver, device_t parent)
{
	if (bthidbus == NULL)
		bthidbus = BUS_ADD_CHILD(parent, 0, "bthidbus", DEVICE_UNIT_ANY);
}

static int
bthidbus_ioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag, struct thread *td)
{
	struct bthidbus_new_connection *con;
	struct bthidbus_softc *sc;
	struct socket *ctrl_sock;
	struct socket *intr_sock;
	struct file *ctrl_file;
	struct file *intr_file;
	uint8_t *kern_rdesc;
	cap_rights_t rights;
	int err;

	sc = dev->si_drv1;

	switch (cmd) {
	case BTHIDBUS_NEW_CONNECTION: {
		con = (struct bthidbus_new_connection *) addr;
		if (con->rdesc_len == 0 || con->rdesc_len > RDESC_MAX_LEN)
			return (EINVAL);

		kern_rdesc = malloc(con->rdesc_len, M_DEVBUF, M_WAITOK | M_ZERO);

		err = copyin(con->rdesc, kern_rdesc, con->rdesc_len);
		if (err != 0) {
			free(kern_rdesc, M_DEVBUF);
			return (err);
		}

		/* Shouldn't overwrite the ioctl struct's rdsec field */
		con->rdesc = kern_rdesc;
		cap_rights_init_one(&rights, CAP_SOCK_CLIENT);

		err = fget(td, con->ctrl_sock, &rights, &ctrl_file);
		if (err != 0) {
			free(kern_rdesc, M_DEVBUF);
			return (err);
		}

		err = fget(td, con->intr_sock, &rights, &intr_file);
		if (err != 0) {
			free(kern_rdesc, M_DEVBUF);
			fdrop(ctrl_file, td);
			return (err);
		}

		if (ctrl_file->f_type != DTYPE_SOCKET || intr_file->f_type != DTYPE_SOCKET) {
			free(kern_rdesc, M_DEVBUF);
			fdrop(ctrl_file, td);
			fdrop(intr_file, td);
			return (ENOTSOCK);
		}

		ctrl_sock = ctrl_file->f_data;
		intr_sock = intr_file->f_data;

		err = new_connection(sc->dev, con, ctrl_sock, intr_sock, ctrl_file, intr_file);
		if (err != 0) {
			free(kern_rdesc, M_DEVBUF);
			fdrop(ctrl_file, td);
			fdrop(intr_file, td);
		}
		return (err);
	}
	}
	return (ENOTTY);
}

static void 
bthidbus_child_deleted(device_t bus, device_t child)
{
	struct bthid_ivars *ivars;

	ivars = device_get_ivars(child);
	free(ivars->rdesc, M_DEVBUF);
	free(ivars, M_DEVBUF);
	device_set_ivars(child, NULL);
}

static device_method_t bthidbus_methods[] = {
	DEVMETHOD(device_identify,	bthidbus_identify),
	DEVMETHOD(device_probe,		bthidbus_probe),
	DEVMETHOD(device_attach,	bthidbus_attach),
	DEVMETHOD(device_detach,	bthidbus_detach),

	/* BUS METHODS */
	DEVMETHOD(bus_add_child,	bthidbus_add_child),
	DEVMETHOD(bus_child_deleted,	bthidbus_child_deleted),
	DEVMETHOD_END
};

static int
bthidbus_modevent(module_t mod, int type, void *data)
{
	int error;
	error = 0;
	switch (type) {
	case MOD_LOAD: {
		break;
	}
	case MOD_UNLOAD: {
		error = device_delete_child(device_get_parent(bthidbus), bthidbus);
		if (error == 0)
			bthidbus = NULL;
		break;
	}
	default: {
		break;
	}
	}
	return (error);
}

static driver_t bthidbus_driver = {
	"bthidbus",
	bthidbus_methods,
	sizeof(struct bthidbus_softc)
};

DRIVER_MODULE(bthidbus, nexus, bthidbus_driver, bthidbus_modevent, NULL);
MODULE_VERSION(bthidbus, 1);
