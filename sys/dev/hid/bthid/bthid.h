#include <sys/socket.h>
#include <sys/types.h>

struct bthid_ivars {
	int ctrl_sock;
	int intr_sock;
	uint16_t vendorId;
	uint16_t productId;
	uint16_t versionId;
	void *rdesc;
	size_t rdesc_len;
};
