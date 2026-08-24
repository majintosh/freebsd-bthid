#ifndef _BTHID_H_
#define _BTHID_H_

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/file.h>

struct bthid_ivars {
	struct socket	*ctrl_sock;
	struct socket	*intr_sock;
	struct file	*ctrl_file;
	struct file	*intr_file;
	uint16_t	vendor_id;
	uint16_t	product_id;
	uint16_t	version_id;
	void		*rdesc;
	size_t		rdesc_len;
};

#endif /* !_BTHID_H_ */
