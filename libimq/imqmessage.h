#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdint.h>

typedef enum imq_message_type_e {
	IMQ_MSG_INVALID_TYPE,
	IMQ_MSG_ERROR,
	IMQ_MSG_AUTH,
	IMQ_MSG_OPEN_CIRCUIT,
	IMQ_MSG_CLOSE_CIRCUIT,
	IMQ_MSG_DATA_CIRCUIT,
	IMQ_MSG_ADV_USER,
	IMQ_MSG_ADV_USER_COMMIT,
	IMQ_MSG_ADV_ENDPOINT
} imq_message_type_t;

typedef struct imq_msg_auth_s {
	char *username;
	char *password;
} imq_msg_auth_t;

typedef struct imq_msg_open_circuit_s {
	char *channel;
	char *instance;
	uint16_t circuitid;
} imq_msg_open_circuit_t;

typedef struct imq_msg_close_circuit_s {
	uint16_t circuitid;
} imq_msg_close_circuit_t;

typedef struct imq_msg_data_circuit_s {
	uint16_t circuitid;
	uint16_t len;
	void *data;
} imq_msg_data_circuit_t;

typedef struct imq_msg_adv_endpoint_s {
	char *channel;
	char *instance;
	uint16_t zmqtype;
} imq_msg_adv_endpoint_t;

typedef struct imq_msg_s {
	imq_message_type_t type;

	union {
		imq_msg_auth_t auth;
		imq_msg_open_circuit_t open_circuit;
		imq_msg_close_circuit_t close_circuit;
		imq_msg_data_circuit_t data_circuit;
		imq_msg_adv_endpoint_t adv_endpoint;
	} content;
} imq_msg_t;

int imq_send_message(imq_fifo_t *fifo, const imq_msg_t *message);
imq_msg_t *imq_receive_message(imq_fifo_t *fifo);
void imq_free_message(imq_msg_t *message);

#endif /* MESSAGE_H */
