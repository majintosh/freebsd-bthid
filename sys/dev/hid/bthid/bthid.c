#include <sys/param.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/socketvar.h>
#include <sys/mutex.h>
#include <sys/uio.h>
#include <dev/hid/hid.h>
#include <sys/taskqueue.h>

#include <sys/mbuf.h>
#include <dev/evdev/input.h>

#include "bthid.h"
#include "hid_if.h"
#include <sys/file.h>

#define MAX_LOOPS 64

struct bthid_softc {
	struct socket			*ctrl;
	struct socket			*intr;
	struct task			intr_task;
	hid_intr_t			*intr_handler;
	void				*intr_ctx;
	hid_size_t			input_length;
	struct hid_device_info		dinfo;
	struct hid_rdesc_info		rdesc;

	struct file			*ctrl_file;
	struct file			*intr_file;
};


static int
bthid_probe(device_t dev)
{
	return (BUS_PROBE_DEFAULT);
}




static int
socket_close(struct socket *sock, struct file *f)
{
	if (sock == NULL)
		return (0);
	SOCK_RECVBUF_LOCK(sock);
	if (sock->so_rcv.sb_upcall != NULL)
		soupcall_clear(sock, SO_RCV);
	SOCK_RECVBUF_UNLOCK(sock);
	// Need to clear taskqueue threads before closing socket
	fdrop(f, curthread);
	return (0);
}

static int
bthid_detach(device_t dev)
{
	struct bthid_softc *sc = device_get_softc(dev);
	device_delete_children(dev);
	socket_close(sc->ctrl, sc->ctrl_file);
	socket_close(sc->intr, sc->intr_file);
	free(sc->rdesc.data, M_DEVBUF);
	return (0);
}

static int
bthid_attach(device_t dev)
{
	struct bthid_ivars *ivar = device_get_ivars(dev);
	device_t child = device_add_child(dev, "hidbus", DEVICE_UNIT_ANY);
	if (child == NULL) {
		device_printf(dev, "Couldn't add hidbus device\n");
		free(ivar->rdesc, M_DEVBUF);
		return (ENOMEM);
	}
	struct bthid_softc *sc = device_get_softc(dev);
	bzero(sc, sizeof(struct bthid_softc));
	sc->rdesc.data = ivar->rdesc;
	sc->ctrl = ivar->ctrl_sock;
	sc->intr = ivar->intr_sock;
	sc->ctrl_file = ivar->ctrl_file;
	sc->intr_file = ivar->intr_file;
	sc->dinfo.idBus = BUS_BLUETOOTH;
	sc->dinfo.idVendor = ivar->vendor_id;
	sc->dinfo.idProduct = ivar->product_id;
	sc->dinfo.idVersion = ivar->version_id;
	sc->dinfo.rdescsize = ivar->rdesc_len;

	device_set_ivars(child, &sc->dinfo);
	bus_attach_children(dev);
	return (0);
}

static void
intr_worker(void *context, int pending)
{
	struct bthid_softc *sc = context;

	struct uio uio;
	int flag = MSG_DONTWAIT;
	uio.uio_td = curthread;
	struct mbuf *m = NULL;
	int loops = 0;
	for (; loops < MAX_LOOPS; loops++) {
		uio.uio_resid = sc->rdesc.isize + 1;
		soreceive(sc->intr, NULL, &uio, &m, NULL, &flag);
		if (m == NULL)
			break;
		m_adj(m, 1); // Strip the bluetooth header from the packet
		uint8_t *payload = mtod(m, uint8_t *);
		sc->intr_handler(sc->intr_ctx, payload, m->m_len);
		m_freem(m);
	}
	// If we hit the loop cap, we probably have more packets to process
	if (loops == MAX_LOOPS)
		taskqueue_enqueue(taskqueue_swi, &sc->intr_task);
}

static int
intr_upcall(struct socket *s, void *arg, int which)
{
	struct task *intr_task = arg;
	taskqueue_enqueue(taskqueue_swi, intr_task);
	return (SU_OK);
}

static void
bthid_intr_setup(device_t dev, device_t child __unused, hid_intr_t intr,
		void *context, struct hid_rdesc_info *rdesc)
{
	struct bthid_softc *sc = device_get_softc(dev);
	sc->intr_handler = intr;
	sc->intr_ctx = context;
	sc->rdesc.isize = rdesc->isize;
	TASK_INIT(&sc->intr_task, 0, intr_worker, sc);
}

static int
bthid_intr_start(device_t dev, device_t child __unused)
{
	struct bthid_softc *sc = device_get_softc(dev);
	SOCK_RECVBUF_LOCK(sc->intr);
	soupcall_set(sc->intr, SO_RCV, intr_upcall, &sc->intr_task);
	SOCK_RECVBUF_UNLOCK(sc->intr);

	return (0);
}

static int
bthid_intr_stop(device_t dev, device_t child __unused)
{
	struct bthid_softc *sc = device_get_softc(dev);
	SOCK_RECVBUF_LOCK(sc->intr);
	if (sc->intr->so_rcv.sb_upcall != NULL)
		soupcall_clear(sc->intr, SO_RCV);
	SOCK_RECVBUF_UNLOCK(sc->intr);
	taskqueue_drain(taskqueue_swi, &sc->intr_task);
	return (0);
}

static int
bthid_get_rdesc(device_t dev, device_t child __unused, void *buf,
		hid_size_t len)
{
	struct bthid_softc *sc = device_get_softc(dev);
	memcpy(buf, sc->rdesc.data, len);
	return (0);
}



static device_method_t bthid_methods[] = {
	DEVMETHOD(device_probe,		bthid_probe),
	DEVMETHOD(device_attach,	bthid_attach),
	DEVMETHOD(device_detach,	bthid_detach),

	DEVMETHOD(hid_intr_setup,	bthid_intr_setup),
	DEVMETHOD(hid_intr_start,	bthid_intr_start),
	DEVMETHOD(hid_intr_stop,	bthid_intr_stop),
	DEVMETHOD(hid_get_rdesc,	bthid_get_rdesc),

	DEVMETHOD_END
};

static driver_t bthid_driver = {
	"bthid",
	bthid_methods,
	sizeof(struct bthid_softc)
};

DRIVER_MODULE(bthid, bthidbus, bthid_driver, NULL, NULL);
MODULE_VERSION(bthid, 1);
extern driver_t hidbus_driver;
DRIVER_MODULE(hidbus, bthid, hidbus_driver, 0, 0);
