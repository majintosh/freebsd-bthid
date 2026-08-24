#ifndef _BTHIDBUS_H_
#define _BTHIDBUS_H_

#include <sys/ioccom.h>
#include <sys/types.h>

struct bthidbus_new_connection {
	int		ctrl_sock;
	int		intr_sock;
	uint16_t	vendor_id;
	uint16_t	product_id;
	uint16_t	version_id;
	void		*rdesc;
	size_t		rdesc_len;

};

#define BTHIDBUS_NEW_CONNECTION _IOW('B', 1, struct bthidbus_new_connection)

#endif /* !_BTHIDBUS_H_ */
