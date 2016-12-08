/*
 * rparam_slash.c
 *
 *  Created on: Oct 8, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <slash/slash.h>

#include <csp/csp.h>
#include <csp/csp_endian.h>
#include <csp/arch/csp_malloc.h>

#include <param/param.h>
#include <param/rparam.h>
#include <param/rparam_list.h>
#include <param/rparam_listset.h>

#include "param_serializer.h"
#include "param_string.h"

slash_command_group(rparam, "Remote parameters");

#define RPARAM_SLASH_MAX_QUEUESIZE 500

static rparam_t * rparams[RPARAM_SLASH_MAX_QUEUESIZE];
static int rparams_count = 0;
static int rparam_autosend = 1;

int rparam_default_node = 0;
int rparam_default_timeout = 2000;

static void rparam_print_queue(void) {
	printf("Queue\n");
	for(int i = 0; i < rparams_count; i++)
		rparam_print(rparams[i]);
}

static void rparam_completer(struct slash *slash, char * token) {

	int matches = 0;
	size_t prefixlen = -1;
	rparam_t *prefix = NULL;
	size_t tokenlen = strlen(token);

	void rparam_iterator(rparam_t * rparam) {

		if (tokenlen > strlen(rparam->name))
			return;

		if (rparam_default_node != rparam->node)
			return;

		if (rparam->readonly == PARAM_HIDDEN)
			return;

		if (strncmp(token, rparam->name, slash_min(strlen(rparam->name), tokenlen)) == 0) {

			/* Count matches */
			matches++;

			/* Find common prefix */
			if (prefixlen == (size_t) -1) {
				prefix = rparam;
				prefixlen = strlen(prefix->name);
			} else {
				prefixlen = slash_prefix_length(prefix->name, rparam->name);
			}

			/* Print newline on first match */
			if (matches == 1)
				slash_printf(slash, "\n");

			/* Print param */
			rparam_print(rparam);

		}

	}

	rparam_list_foreach(rparam_iterator);

	if (!matches) {
		slash_bell(slash);
	} else {
		strncpy(token, prefix->name, prefixlen);
		slash->cursor = slash->length = (token - slash->buffer) + prefixlen;
	}

}

static int rparam_slash_node(struct slash *slash)
{
	if (slash->argc < 2) {
		slash_printf(slash, "node: %u, timeout: %u\n", rparam_default_node, rparam_default_timeout);
		return SLASH_EUSAGE;
	}

	rparam_default_node = atoi(slash->argv[1]);

	if (slash->argc >= 3)
		rparam_default_timeout = atoi(slash->argv[2]);

	slash_printf(slash, "Set node to: %u, timeout: %u\n", rparam_default_node, rparam_default_timeout);
	return SLASH_SUCCESS;
}
slash_command_sub(rparam, node, rparam_slash_node, "<node> [timeout]", NULL);


static int rparam_slash_getall(struct slash *slash)
{
	unsigned int node = rparam_default_node;
	unsigned int timeout = rparam_default_timeout;

	if (slash->argc >= 2)
		node = atoi(slash->argv[1]);
	if (slash->argc >= 3)
		timeout = atoi(slash->argv[2]);

	/* Clear queue first */
	rparams_count = 0;

	void add_to_queue(rparam_t * rparam) {
		if (rparam->node == node) {
			if (rparams_count < RPARAM_SLASH_MAX_QUEUESIZE)
				rparams[rparams_count++] = rparam;
		}
	}
	rparam_list_foreach(add_to_queue);

	if (rparams_count == 0)
		return SLASH_SUCCESS;

	rparams[0]->timeout = timeout;
	rparam_get(rparams, rparams_count, 1);

	return SLASH_SUCCESS;
}
slash_command_sub(rparam, getall, rparam_slash_getall, "[node] [timeout]", NULL);

static int rparam_slash_setall(struct slash *slash)
{
	unsigned int node = rparam_default_node;
	unsigned int timeout = rparam_default_timeout;

	if (slash->argc >= 2)
		node = atoi(slash->argv[1]);
	if (slash->argc >= 3)
		timeout = atoi(slash->argv[2]);

	/* Clear queue first */
	rparams_count = 0;

	void add_to_queue(rparam_t * rparam) {
		if (rparam->node == node && rparam->setvalue_pending == 1) {
			if (rparams_count < RPARAM_SLASH_MAX_QUEUESIZE)
				rparams[rparams_count++] = rparam;
		}
	}
	rparam_list_foreach(add_to_queue);

	if (rparams_count == 0)
		return SLASH_SUCCESS;

	rparams[0]->timeout = timeout;
	if (rparam_set(rparams, rparams_count, 1) < 0) {
		printf("No response\n");
		return SLASH_EINVAL;
	}

	return SLASH_SUCCESS;
}
slash_command_sub(rparam, setall, rparam_slash_setall, "[node] [timeout]", NULL);

static int rparam_slash_get(struct slash *slash)
{

	/**
	 * If called without arguments, and there are stuff in queue, get now
	 */
	if ((slash->argc <= 1) && (rparams_count > 0)) {

		if (rparam_get(rparams, rparams_count, 1) < 0) {
			printf("No response\n");
			return SLASH_EINVAL;
		}

		return SLASH_SUCCESS;
	}

	if (slash->argc < 2)
		return SLASH_EUSAGE;

	rparam_t * rparam = rparam_list_find_name(rparam_default_node, slash->argv[1]);

	if (rparam == NULL) {
		slash_printf(slash, "Could not find parameter\n");
		return SLASH_EINVAL;
	}

	if ((rparams_count > 0) && (rparams[0]->node != rparam->node)) {
		printf("You can only queue parameters from the same node\n");
		return SLASH_EINVAL;
	}

	int already_in_queue = 0;
	for(int i = 0; i < rparams_count; i++) {
		if (rparams[i]->id == rparam->id)
			already_in_queue = 1;
	}

	if (!already_in_queue) {
		if (rparams_count < RPARAM_SLASH_MAX_QUEUESIZE)
			rparams[rparams_count++] = rparam;
		if (rparam_autosend == 0)
			printf("Added %s to queue\n", rparam->name);
	}

	if (rparam_autosend == 0) {
		rparam_print_queue();
		return SLASH_SUCCESS;
	}

	if (rparam_get(rparams, rparams_count, 1) < 0) {
		printf("No response\n");
		return SLASH_EINVAL;
	}

	rparams_count = 0;
	return SLASH_SUCCESS;
}
slash_command_sub_completer(rparam, get, rparam_slash_get, rparam_completer, "<param>", NULL);

static int rparam_slash_set(struct slash *slash)
{

	/**
	 * If called without arguments, and there are stuff in queue, set now
	 */
	if ((slash->argc <= 1) && (rparams_count > 0)) {

		if (rparam_set(rparams, rparams_count, 1) < 0) {
			printf("No response\n");
			return SLASH_EINVAL;
		}

		return SLASH_SUCCESS;
	}

	if (slash->argc < 3)
		return SLASH_EUSAGE;

	rparam_t * rparam = rparam_list_find_name(rparam_default_node, slash->argv[1]);

	if (rparam == NULL) {
		slash_printf(slash, "Could not find parameter\n");
		return SLASH_EINVAL;
	}

	if ((rparams_count > 0) && (rparams[0]->node != rparam->node)) {
		printf("You can only queue parameters from the same node\n");
		return SLASH_EINVAL;
	}

	if (rparam->setvalue == NULL) {
		rparam->setvalue = calloc(rparam_size(rparam), 1);
	}

	param_str_to_value(rparam->type, slash->argv[2], rparam->setvalue);
	rparam->setvalue_pending = 1;

	int already_in_queue = 0;
	for(int i = 0; i < rparams_count; i++) {
		if (rparams[i]->id == rparam->id)
			already_in_queue = 1;
	}

	if (!already_in_queue) {
		if (rparams_count < RPARAM_SLASH_MAX_QUEUESIZE)
			rparams[rparams_count++] = rparam;
		if (rparam_autosend == 0)
			printf("Added %s to queue\n", rparam->name);
	}

	if (rparam_autosend == 0) {
		rparam_print_queue();
		return SLASH_SUCCESS;
	}

	if (rparam_set(rparams, rparams_count, 1) < 0) {
		printf("No response\n");
		return SLASH_EINVAL;
	}

	rparams_count = 0;
	return SLASH_SUCCESS;

}
slash_command_sub_completer(rparam, set, rparam_slash_set, rparam_completer, "<param> <value>", NULL);

static int rparam_slash_download(struct slash *slash)
{
	if (slash->argc < 2)
		return SLASH_EUSAGE;

	unsigned int node = atoi(slash->argv[1]);
	unsigned int timeout = 1000;
	if (slash->argc >= 3)
		timeout = atoi(slash->argv[2]);

	rparam_list_download(node, timeout);

	return SLASH_SUCCESS;
}
slash_command_sub(rparam, download, rparam_slash_download, "<node> [timeout]", NULL);

static int rparam_slash_all(struct slash *slash)
{
	int node = -1;
	int pending = 0;
	if (slash->argc >= 2)
		node = atoi(slash->argv[1]);
	if (slash->argc >= 3)
		pending = atoi(slash->argv[2]);
	rparam_list_print(node, pending);
	return SLASH_SUCCESS;
}
slash_command_sub(rparam, all, rparam_slash_all, "<node> <pending>", "show all remote parameters");

static int rparam_slash_clear(struct slash *slash)
{
	rparams_count = 0;
	return SLASH_SUCCESS;
}
slash_command_sub(rparam, clear, rparam_slash_clear, "", NULL);

static int rparam_slash_autosend(struct slash *slash)
{
	if (slash->argc != 2)
		return SLASH_EUSAGE;
	rparam_autosend = atoi(slash->argv[1]);
	return SLASH_SUCCESS;
}
slash_command_sub(rparam, autosend, rparam_slash_autosend, "<autosend>", NULL);

static int rparam_slash_queue(struct slash *slash)
{
	rparam_print_queue();
	return SLASH_SUCCESS;
}
slash_command_sub(rparam, queue, rparam_slash_queue, "<autosend>", NULL);


static int rparam_slash_list(struct slash *slash)
{
	int node = csp_get_address();
	int timeout = 2000;
	char * endptr;

	if (slash->argc < 2) {
		printf("Select list:\n");
		rparam_listset_print();
		return SLASH_EUSAGE;
	}

	rparam_listset_t * list = rparam_listset_find(slash->argv[1]);

	if (list == NULL) {
		printf("Could not find list with name: %s\n", slash->argv[1]);
		return SLASH_EINVAL;
	}

	if (slash->argc >= 3) {
		node = strtoul(slash->argv[2], &endptr, 10);
		if (*endptr != '\0')
			return SLASH_EUSAGE;
	}

	if (slash->argc >= 4) {
		timeout = strtoul(slash->argv[3], &endptr, 10);
		if (*endptr != '\0')
			return SLASH_EUSAGE;
	}

	printf("Requesting list %s from %u, timeout %u\n", list->listname, node, timeout);

	rparam_t *rparams[50];
	int rparams_count = 0;
	for (char ** name = list->names; *name != NULL; name++) {
		rparams[rparams_count] = rparam_list_find_name(node, *name);
		if ((rparams[rparams_count] != NULL) && (rparams_count < 50)) {
			rparams_count++;
		}
	}

	if (rparams_count == 0) {
		return SLASH_EINVAL;
	}

	rparams[0]->node = node;
	rparams[0]->timeout = timeout;

	rparam_get(rparams, rparams_count, 1);

	return SLASH_SUCCESS;
}

slash_command_sub(rparam, list, rparam_slash_list, "<list> [node] [timeout]", NULL);

