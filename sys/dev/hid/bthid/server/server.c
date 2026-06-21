#include <sys/bitstring.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/kernel.h>
#include <sys/module.h>
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

static struct socket *ctrl, *intr;
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
	l2addr.l2cap_psm = 0;
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

static int
bthidbus_server_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	switch (type) {
		
		case MOD_LOAD:
			socket_setup(&ctrl, 0x11);
			socket_setup(&intr, 0x13);
			TASK_INIT(&printer_task, 0, printer, intr); // Worried this might run after our intr socket already receives a packet
			break;
		case MOD_UNLOAD:
			socket_close(ctrl);
			ctrl = NULL;
			socket_close(intr);
			intr = NULL;
			break;
		default:
			break;
	}
	return error;
}

static moduledata_t bthidbus_server = {
	"bthidbus_server",
	bthidbus_server_modevent,
	0
};

DECLARE_MODULE(bthidbus_server, bthidbus_server, SI_SUB_DRIVERS, SI_ORDER_ANY);
