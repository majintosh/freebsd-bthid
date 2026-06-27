#include <sys/socket.h>

struct bthid_ivars {
	struct socket *ctrl;
	struct socket *intr;
};
