#include <sys/param.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/socketvar.h>
#include <sys/mutex.h>

#include "bthid.h"

struct bthid_softc {
	struct socket *ctrl;
	struct socket *intr;
};

static int
bthid_probe(device_t dev)
{
	return (BUS_PROBE_DEFAULT);
}

static int
bthid_attach(device_t dev)
{
	struct bthid_softc *sc = device_get_softc(dev);
	struct bthid_ivars *ivar = device_get_ivars(dev);
	sc->ctrl = ivar->ctrl;
	sc->intr = ivar->intr;
	return 0;
}

static int
socket_close(struct socket* sock)
{
	if (sock == NULL)
		return 0;
	SOCK_RECVBUF_LOCK(sock);
	soupcall_clear(sock, SO_RCV);
	SOCK_RECVBUF_UNLOCK(sock);
	soclose(sock);
	return 0;
}

static int
bthid_detach(device_t dev)
{
	struct bthid_softc *sc = device_get_softc(dev);
	socket_close(sc->ctrl);
	socket_close(sc->intr);
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
	sizeof(struct bthid_softc)
};

DRIVER_MODULE(bthid, bthidbus, bthid_driver, NULL, NULL);
MODULE_VERSION(bthid, 1);
