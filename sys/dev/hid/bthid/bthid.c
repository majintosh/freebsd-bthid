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

#include "bthid.h"
#include "hid_if.h"

struct bthid_softc {
	struct socket*	ctrl;
	struct socket*	intr;
	struct task*	intr_task;
	hid_intr_t*	intr_handler;
	void*		intr_ctx;
	hid_size_t	input_length;
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


static void
intr_worker(void* context, int pending)
{
	struct bthid_softc *sc = context;
	
	struct uio uio;
	int flag = MSG_DONTWAIT;
	// We set resid to 1m to drain everything in the buffer. There is a worry that the intr worker would then retrieve
	// multiple input packets at a time rather than just 1, but this shouldn't be an issue, since the worker thread runs
	// as soon as a packet is received. Though we could receive another packet before the worker thread runs...
	// The alternative approach is to use the isize of hid_rdsec_info and add the size of the packet header to it. Gonna implement this later
	uio.uio_resid = 1000000; 
	uio.uio_td = curthread;
	struct mbuf *m = NULL;
	soreceive(sc->intr, NULL, &uio, &m, NULL, &flag);
	if (m!=NULL) {
		printf("Packet received\n"); 
		// Need to shift the mbuf to get rid of the header bytes
		uint8_t *payload = mtod(m, uint8_t *);
		sc->intr_handler(sc->intr_ctx, payload, m->m_len);
	}
	m_freem(m);
}

static int
intr_upcall(struct socket *s, void *arg, int which)
{
	struct task *intr_task = arg;
	taskqueue_enqueue(taskqueue_thread, intr_task);
	return SU_OK;
}

static void 
bthid_intr_setup(device_t dev, device_t child __unused, hid_intr_t intr,
		void *context, struct hid_rdesc_info *rdesc)
{
	struct bthid_softc *sc = device_get_softc(dev);
	sc->intr_handler = intr;
	sc->intr_ctx = context;
	sc->input_length = rdesc->isize;
	TASK_INIT(sc->intr_task, 0, intr_worker, sc);
}

static int
bthid_intr_start(device_t dev, device_t child __unused)
{
	struct bthid_softc *sc = device_get_softc(dev);

	SOCK_RECVBUF_LOCK(sc->intr);
	soupcall_set(sc->intr, SO_RCV, intr_upcall, sc->intr_task);
	SOCK_RECVBUF_UNLOCK(sc->intr);

	return (0);
}


static device_method_t bthid_methods[] = {
	DEVMETHOD(device_probe,		bthid_probe),
	DEVMETHOD(device_attach,	bthid_attach),
	DEVMETHOD(device_detach,	bthid_detach),

	DEVMETHOD(hid_intr_setup,	bthid_intr_setup),
	DEVMETHOD(hid_intr_start,	bthid_intr_start),

	DEVMETHOD_END
};

static driver_t bthid_driver = {
	"bthid",
	bthid_methods,
	sizeof(struct bthid_softc)
};

DRIVER_MODULE(bthid, bthidbus, bthid_driver, NULL, NULL);
MODULE_VERSION(bthid, 1);
