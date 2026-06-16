#include <sys/bitstring.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <netgraph/bluetooth/include/ng_bluetooth.h>
#include <netgraph/bluetooth/include/ng_hci.h>
#include <netgraph/bluetooth/include/ng_l2cap.h>
#include <netgraph/bluetooth/include/ng_btsocket.h>

static struct socket *bthid_socket = NULL;

static int
socket_setup(void)
{
	struct sockaddr_l2cap l2addr;
	int error = 0;
	error = socreate(PF_BLUETOOTH, &bthid_socket, SOCK_SEQPACKET,
	    BLUETOOTH_PROTO_L2CAP, curthread->td_ucred, curthread);
	if (error != 0) {
		printf("We couldn't create the socket!\n");
		return error;
	}
	printf("We created the socket!\n");

	l2addr.l2cap_len = sizeof(l2addr);
	l2addr.l2cap_family = AF_BLUETOOTH;
	l2addr.l2cap_bdaddr = *NG_HCI_BDADDR_ANY;
	l2addr.l2cap_psm = htole16(0x11); // The control channel... we should do intr first
	l2addr.l2cap_bdaddr_type = BDADDR_BREDR;
	l2addr.l2cap_cid = 0;

	error = sobind(bthid_socket, (struct sockaddr *)&l2addr, curthread);

	if (error != 0) {
		printf("We couldn't bind the socket!\n");
		soclose(bthid_socket);
		bthid_socket = NULL;
		return error;
	}
	printf("We bound the socket!\n"); // Or is it "binded"?
	return error;
}

static void
socket_close(void)
{
	if (bthid_socket != NULL) {
		soclose(bthid_socket);
		bthid_socket = NULL;
		printf("We closed the socket!\n");
	}
}

static int
bthidbus_server_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	switch (type) {
		
		case MOD_LOAD:
			socket_setup();
			break;
		case MOD_UNLOAD:
			socket_close();
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
