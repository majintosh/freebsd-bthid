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

static struct socket *ctrl, *intr;

static int
socket_setup(struct socket *sock, uint16_t psm)
{
	struct sockaddr_l2cap l2addr;
	int error = 0;
	error = socreate(PF_BLUETOOTH, &sock, SOCK_SEQPACKET,
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

	error = sobind(sock, (struct sockaddr *)&l2addr, curthread);

	if (error != 0) {
		printf("We couldn't bind the socket!\n");
		goto cleanup;
	}

	printf("We bound the socket!\n"); // Or is it "binded"?
	
	l2addr.l2cap_psm = htole16(psm); 

	bdaddr_t controller_addr = {{0xMA, 0xJE, 0xDW, 0xAS, 0xHE, 0xRE}}; // Replace this with the actual bdaddr. Needs to be in little endian format.

	memcpy(&l2addr.l2cap_bdaddr, &controller_addr, sizeof(l2addr.l2cap_bdaddr));

	error = soconnect(sock, (struct sockaddr *) &l2addr, curthread);

	if (error != 0) {
		printf("We couldn't connect the socket!\n");
		goto cleanup;
	}

#if 0
	error = solisten(sock, 10, curthread);

	if (error != 0) {
		printf("We couldn't listen on the socket!\n");
		soclose(sock);
		sock = NULL;
		return error;
	}

	printf("We're listening on the socket!\n");

	error = soaccept(sock, (struct sockaddr*) &l2addr);

	if (error != 0) {
		printf("We couldn't accept the socket!\n");
		soclose(sock);
		sock = NULL;
		return error;
	}


	sock->so_linger = 1;
	printf("We accepted the socket!\n");
	printf("Linger is: %d\n", sock->so_linger);
	printf("Addr is: %p\n", sock);
#endif
	return error;

cleanup:
	soclose(sock);
	sock = NULL;
	return error;
}

static void
socket_close(struct socket *sock)
{
	if (sock != NULL) {
		soclose(sock);
		sock = NULL;
		printf("We closed the socket!\n");
	}
}

static int
bthidbus_server_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	switch (type) {
		
		case MOD_LOAD:
			socket_setup(ctrl, 0x11);
			socket_setup(intr, 0x13);
			break;
		case MOD_UNLOAD:
			socket_close(ctrl);
			socket_close(intr);
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
