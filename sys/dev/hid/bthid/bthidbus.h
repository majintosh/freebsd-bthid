#include <sys/ioccom.h>
#include <sys/types.h>

struct bthidbus_new_connection {
	int ctrl_sock;
	int intr_sock;
	uint16_t vendorId;
	uint16_t productId;
	uint16_t versionId;
	void *rdesc;
	size_t rdesc_len;

};

#define BTHIDBUS_NEW_CONNECTION _IOW('B', 1, struct bthidbus_new_connection)
