#include <sys/ioccom.h>

struct bthidbus_new_connection {
	int ctrl_sock;
	int intr_sock;
};

#define BTHIDBUS_NEW_CONNECTION _IOW('B', 1, struct bthidbus_new_connection)
