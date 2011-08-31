#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "imq.h"
#include "imqmessage.h"

static int imq_msg_write_uint16(imq_fifo_t *fifo, uint16_t val) {
	uint16_t val_netw;

	val_netw = htons(val);

	return imq_write_fifo(fifo, &val_netw, sizeof (val_netw));
}

static int imq_msg_read_uint16(imq_fifo_t *fifo, uint16_t *pval) {
	int rc;
	uint16_t val_netw;

	rc = imq_read_fifo(fifo, &val_netw, sizeof (val_netw));

	if (rc < sizeof (val_netw))
		return -1;

	assert(pval != NULL);
	*pval = ntohs(val_netw);

	return 0;
}

static int imq_msg_write_string(imq_fifo_t *fifo, const char *val) {
	int rc;
	uint16_t len;

	if (val != NULL)
		len = strlen(val);
	else
		len = 0;

	rc = imq_msg_write_uint16(fifo, len);

	if (rc < 0)
		return -1;

	if (val == NULL)
		return 0;

	rc = imq_write_fifo(fifo, val, len);

	if (rc < len)
		return -1;

	return 0;
}

static int imq_msg_read_string(imq_fifo_t *fifo, char **pval) {
	int rc;
	uint16_t len;

	rc = imq_msg_read_uint16(fifo, &len);

	if (rc < 0)
		return rc;

	if (len == 0) {
		*pval = NULL;
		return 0;
	}

	assert(pval != NULL);

	*pval = malloc(len + 1);
	(*pval)[len] = '\0';

	rc = imq_read_fifo(fifo, *pval, len);

	if (rc < len) {
		free(*pval);
		*pval = NULL;
		return -1;
	}

	return 0;
}

static int imq_msg_send_open_circuit(imq_fifo_t *fifo,
    const imq_msg_open_circuit_t *msg) {
	int rc;

	rc = imq_msg_write_string(fifo, msg->channel);

	if (rc < 0)
		return -1;

	rc = imq_msg_write_string(fifo, msg->instance);

	if (rc < 0)
		return -1;

	rc = imq_msg_write_uint16(fifo, msg->circuitid);

	if (rc < 0)
		return -1;

	return 0;
}

static int imq_msg_receive_open_circuit(imq_fifo_t *fifo,
    imq_msg_open_circuit_t *msg) {
	int rc;

	rc = imq_msg_read_string(fifo, &(msg->channel));

	if (rc < 0)
		return -1;

	rc = imq_msg_read_string(fifo, &(msg->instance));

	if (rc < 0)
		return -1;

	rc = imq_msg_read_uint16(fifo, &(msg->circuitid));

	if (rc < 0)
		return -1;

	return 0;
}

static int imq_msg_send_auth(imq_fifo_t *fifo,
    const imq_msg_auth_t *msg) {
	int rc;

	rc = imq_msg_write_string(fifo, msg->username);

	if (rc < 0)
		return -1;

	rc = imq_msg_write_string(fifo, msg->password);

	if (rc < 0)
		return -1;

	return 0;
}

static int imq_msg_receive_auth(imq_fifo_t *fifo,
    imq_msg_auth_t *msg) {
	int rc;

	rc = imq_msg_read_string(fifo, &(msg->username));

	if (rc < 0)
		return -1;

	rc = imq_msg_read_string(fifo, &(msg->password));

	if (rc < 0)
		return -1;

	return 0;
}

static int imq_msg_send_close_circuit(imq_fifo_t *fifo,
    const imq_msg_close_circuit_t *msg) {
	int rc;

	rc = imq_msg_write_uint16(fifo, msg->circuitid);

	if (rc < 0)
		return -1;

	return 0;
}

static int imq_msg_receive_close_circuit(imq_fifo_t *fifo,
    imq_msg_close_circuit_t *msg) {
	return imq_msg_read_uint16(fifo, &(msg->circuitid));
}

static int imq_msg_send_data_circuit(imq_fifo_t *fifo,
    const imq_msg_data_circuit_t *msg) {
	int rc;

	rc = imq_msg_write_uint16(fifo, msg->circuitid);

	if (rc < 0)
		return rc;

	rc = imq_msg_write_uint16(fifo, msg->len);

	if (rc < 0)
		return rc;

	rc = imq_write_fifo(fifo, msg->data, msg->len);

	if (rc < msg->len)
		return -1;

	return 0;
}

static int imq_msg_receive_data_circuit(imq_fifo_t *fifo,
    imq_msg_data_circuit_t *msg) {
	int rc;
	uint16_t len;

	rc = imq_msg_read_uint16(fifo, &(msg->circuitid));

	if (rc < 0)
		return -1;

	rc = imq_msg_read_uint16(fifo, &(msg->len));

	if (rc < 0)
		return -1;

	msg->data = malloc(msg->len);

	if (msg->data == NULL)
		return -1;

	rc = imq_read_fifo(fifo, msg->data, msg->len);

	if (rc < msg->len)
		return -1;

	return 0;
}

static int imq_msg_send_adv_endpoint(imq_fifo_t *fifo,
    const imq_msg_adv_endpoint_t *msg) {
	int rc;

	rc = imq_msg_write_string(fifo, msg->channel);

	if (rc < 0)
		return -1;

	rc = imq_msg_write_string(fifo, msg->instance);

	if (rc < 0)
		return -1;

	rc = imq_msg_write_uint16(fifo, msg->zmqtype);

	if (rc < 0)
		return -1;

	return 0;
}

static int imq_msg_receive_adv_endpoint(imq_fifo_t *fifo,
    imq_msg_adv_endpoint_t *msg) {
	int rc;

	rc = imq_msg_read_string(fifo, &(msg->channel));

	if (rc < 0)
		return -1;

	rc = imq_msg_read_string(fifo, &(msg->instance));

	if (rc < 0)
		return -1;

	rc = imq_msg_read_uint16(fifo, &(msg->zmqtype));

	if (rc < 0)
		return -1;

	return 0;
}

int imq_send_message(imq_fifo_t *fifo, const imq_msg_t *message) {
	int rc;

	rc = imq_msg_write_uint16(fifo, message->type);

	if (rc < 0)
		return -1;

	switch (message->type) {
	case IMQ_MSG_AUTH:
		return imq_msg_send_auth(fifo, &(message->content.auth));
	case IMQ_MSG_OPEN_CIRCUIT:
		return imq_msg_send_open_circuit(fifo,
		    &(message->content.open_circuit));
	case IMQ_MSG_CLOSE_CIRCUIT:
		return imq_msg_send_close_circuit(fifo,
		    &(message->content.close_circuit));
	case IMQ_MSG_DATA_CIRCUIT:
		return imq_msg_send_data_circuit(fifo,
		    &(message->content.data_circuit));
	case IMQ_MSG_ADV_ENDPOINT:
		return imq_msg_send_adv_endpoint(fifo,
		    &(message->content.adv_endpoint));
	default:
		return -1;
	}
}

imq_msg_t *imq_receive_message(imq_fifo_t *fifo) {
	int rc;
	imq_fifo_t *clone_fifo;
	imq_msg_t *message;
	uint16_t type;

	clone_fifo = imq_clone_fifo(fifo);

	if (clone_fifo == NULL)
		return NULL;

	message = (imq_msg_t *)malloc(sizeof (*message));

	memset(message, 0, sizeof (*message));

	message->type = IMQ_MSG_INVALID_TYPE;

	if (message == NULL) {
		imq_free_fifo(clone_fifo);
		return NULL;
	}

	rc = imq_msg_read_uint16(clone_fifo, &(type));

	if (rc < 0) {
		imq_free_fifo(clone_fifo);
		imq_free_message(message);

		return NULL;
	}

	message->type = type;

	switch (message->type) {
	case IMQ_MSG_AUTH:
		rc = imq_msg_receive_auth(clone_fifo, &(message->content.auth));
		break;
	case IMQ_MSG_OPEN_CIRCUIT:
		rc = imq_msg_receive_open_circuit(clone_fifo,
		    &(message->content.open_circuit));
		break;
	case IMQ_MSG_CLOSE_CIRCUIT:
		rc = imq_msg_receive_close_circuit(clone_fifo,
		    &(message->content.close_circuit));
		break;
	case IMQ_MSG_DATA_CIRCUIT:
		rc = imq_msg_receive_data_circuit(clone_fifo,
		    &(message->content.data_circuit));
		break;
	case IMQ_MSG_ADV_ENDPOINT:
		rc = imq_msg_receive_adv_endpoint(clone_fifo,
		    &(message->content.adv_endpoint));
		break;
	default:
		rc = -1;
	}

	if (rc < 0) {
		imq_free_fifo(clone_fifo);
		imq_free_message(message);
		return NULL;
	}

	imq_read_fifo(fifo, NULL,
	    imq_fifo_size(fifo) - imq_fifo_size(clone_fifo));

	imq_free_fifo(clone_fifo);

	return message;
}

void imq_free_message(imq_msg_t *message) {
	if (message == NULL)
		return;

	switch (message->type) {
	case IMQ_MSG_AUTH:
		free(message->content.auth.username);
		free(message->content.auth.password);
		break;
	case IMQ_MSG_OPEN_CIRCUIT:
		free(message->content.open_circuit.channel);
		free(message->content.open_circuit.instance);
		break;
	case IMQ_MSG_CLOSE_CIRCUIT:
		break;
	case IMQ_MSG_DATA_CIRCUIT:
		free(message->content.data_circuit.data);
		break;
	case IMQ_MSG_ADV_USER:
	case IMQ_MSG_ADV_USER_COMMIT:
	case IMQ_MSG_ADV_ENDPOINT:
		free(message->content.adv_endpoint.channel);
		free(message->content.adv_endpoint.instance);
		break;
	default:
		break;
	}

	free(message);
}
