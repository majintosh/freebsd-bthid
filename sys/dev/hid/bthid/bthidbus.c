#include <sys/param.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/kernel.h>

#include <sys/bitstring.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/mutex.h>
#include <sys/taskqueue.h>
#include <sys/mbuf.h>

#include <netgraph/bluetooth/include/ng_bluetooth.h>
#include <netgraph/bluetooth/include/ng_hci.h>
#include <netgraph/bluetooth/include/ng_l2cap.h>
#include <netgraph/bluetooth/include/ng_btsocket.h>

#include "bthid.h"

static struct socket *bthid_ctrl, *bthid_intr;
static struct task printer_task;

static void
printer(void *context, int pending)
{
	struct socket *s = (struct socket*) context;
	int flag = MSG_DONTWAIT;
	struct uio thing;
	thing.uio_resid = 1000000;
	thing.uio_td = curthread;
	struct mbuf *m = NULL;
	soreceive(s, NULL, &thing, &m, NULL, &flag);
	if (m != NULL) {
		uint8_t* payload = mtod(m, uint8_t *);
		for (int i = 0; i<min(7, m->m_len/sizeof(uint8_t)); i++)
		{	
			printf("%02X ", payload[i]);
		}
		printf("\n");
	}
}

static int
soup(struct socket *s, void *arg, int which)
{
	taskqueue_enqueue(taskqueue_thread, &printer_task);
	return SU_OK;
}

static int
socket_setup(struct socket **sock, uint16_t psm)
{
	struct sockaddr_l2cap l2addr;
	int error = 0;
	error = socreate(PF_BLUETOOTH, sock, SOCK_SEQPACKET,
	    BLUETOOTH_PROTO_L2CAP, curthread->td_ucred, curthread);
	if (error != 0) {
		printf("We couldn't create the socket!\n");
		return error;
	}
	printf("We created the socket!\n");

	l2addr.l2cap_len = sizeof(l2addr);
	l2addr.l2cap_family = AF_BLUETOOTH;
	l2addr.l2cap_bdaddr = *NG_HCI_BDADDR_ANY;
	l2addr.l2cap_psm = htole16(psm);
	l2addr.l2cap_bdaddr_type = BDADDR_BREDR;
	l2addr.l2cap_cid = 0;

	error = sobind(*sock, (struct sockaddr *)&l2addr, curthread);

	if (error != 0) {
		printf("We couldn't bind the socket!\n");
		goto cleanup;
	}

	printf("We bound the socket!\n"); // Or is it "binded"?
	
	l2addr.l2cap_psm = htole16(psm); 

	bdaddr_t controller_addr = {{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};

	memcpy(&l2addr.l2cap_bdaddr, &controller_addr, sizeof(l2addr.l2cap_bdaddr));

	error = soconnect(*sock, (struct sockaddr *) &l2addr, curthread);

	if (error != 0) {
		printf("We couldn't connect the socket!\n");
		goto cleanup;
	}

	printf("We connected the socket!\n");

	SOCK_RECVBUF_LOCK(*sock);
	soupcall_set(*sock, SO_RCV, soup, NULL); // soup, like the food.
	SOCK_RECVBUF_UNLOCK(*sock);

	return error;

cleanup: // Get rid of the label and goto stuff
	soclose(*sock);
	*sock = NULL;
	return error;
}

static void
socket_close(struct socket *sock)
{
	if (sock != NULL) {
		SOCK_RECVBUF_LOCK(sock);
		soupcall_clear(sock, SO_RCV);
		SOCK_RECVBUF_UNLOCK(sock);
		soclose(sock);
		printf("We closed the socket!\n");
	}
}

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
new_connection(device_t bus, struct socket *ctrl, struct socket *intr)
{
	device_t child;
	child = device_add_child(bus, "bthid", DEVICE_UNIT_ANY);

	if (child == NULL)
		return;
	struct bthid_ivars *ivars = device_get_ivars(child);
	ivars->ctrl = ctrl;
	ivars->intr = intr;
	bus_attach_children(bus);
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
			socket_setup(&bthid_ctrl, 0x11);
			socket_setup(&bthid_intr, 0x13);
			new_connection(bthidbus, bthid_ctrl, bthid_intr);
			TASK_INIT(&printer_task, 0, printer, bthid_intr); // Worried this might run after our bthid_intr socket already receives a packet
			break;
		case MOD_UNLOAD:
			if (bthidbus!=NULL)
				device_delete_child(device_get_parent(bthidbus), bthidbus);
			socket_close(bthid_ctrl);
			bthid_ctrl = NULL;
			socket_close(bthid_intr);
			bthid_intr = NULL;
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
MODULE_VERSION(bthidbus, 1);
