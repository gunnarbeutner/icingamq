#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "imq.h"

imq_user_t *imq_alloc_user(const char *username, const char *password,
    int super) {
	imq_user_t *user;

	user = (imq_user_t *)malloc(sizeof (*user));

	if (user == NULL)
		return NULL;

	memset(user, 0, sizeof (*user));

	user->username = strdup(username);

	if (user->username == NULL) {
		free(user);

		return NULL;
	}

	if (password != NULL) {
		user->password = strdup(password);

		if (user->password == NULL) {
			free(user->username);
			free(user);

			return NULL;
		}
	}

	user->super = super;

	return user;
}

void imq_free_user(imq_user_t *user) {
	int i;

	if (user == NULL)
		return;

	for (i = 0; i < user->channelcount; i++) {
		free(user->channels[i]);
	}

	free(user->channels);

	free(user->password);
	free(user->username);
	free(user);
}

int imq_user_allow_channel(imq_user_t *user, const char *channel) {
	char **new_channels;
	char *dup_channel;

	assert(user != NULL);
	assert(channel != NULL);

	dup_channel = strdup(channel);

	if (dup_channel == NULL)
		return -1;

	new_channels = (char **)realloc(user->channels,
	    (user->channelcount + 1) * sizeof (char *));

	if (new_channels == NULL)
		return -1;

	user->channels = new_channels;
	user->channelcount++;
	user->channels[user->channelcount - 1] = dup_channel;

	return 0;
}

int imq_user_authz_endpoint(imq_user_t *user, imq_endpoint_t *endpoint) {
	int i;

	if (user->super)
		return 0;

	for (i = 0; i < user->channelcount; i++) {
		if (strcmp(user->channels[i], endpoint->channel) == 0)
			return 0;
	}

	return -1;
}
