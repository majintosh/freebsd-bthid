#include <sys/socket.h>
#include <sys/types.h>
#include <sys/file.h>

struct bthid_ivars {
	struct socket* ctrl_sock;
	struct socket* intr_sock;
	struct file* ctrl_file;
	struct file* intr_file;
	uint16_t vendorId;
	uint16_t productId;
	uint16_t versionId;
	void *rdesc;
	size_t rdesc_len;
};
