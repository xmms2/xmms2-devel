
#include <stdlib.h>
#include <stdio.h>

#include <xmmscpriv/xmms_list.h>
#include <xmmsclientpriv/xmmsclient_util.h>
#include <xmmsclientpriv/xmmsclient_queue.h>
#include <xmmsc/xmmsc_util.h>
#include <xmmsc/xmmsc_stdbool.h>

x_queue_t *
x_queue_new (void)
{
	x_queue_t *queue;

	queue = x_new0 (x_queue_t, 1);

	return queue;
}

void
x_queue_free (x_queue_t *queue)
{
	x_return_if_fail (queue);

	x_list_free (queue->head);

	free (queue);
}

void
x_queue_push_head (x_queue_t *queue, void *data)
{
	x_return_if_fail (queue);

	queue->head = x_list_prepend (queue->head, data);

	if (!queue->tail)
		queue->tail = queue->head;

	queue->length++;
}

void
x_queue_push_tail (x_queue_t *queue, void *data)
{
	x_return_if_fail (queue);

	queue->tail = x_list_append (queue->tail, data);

	if (queue->tail->next)
		queue->tail = queue->tail->next;
	else
		queue->head = queue->tail;

	queue->length++;
}

void *
x_queue_pop_head (x_queue_t *queue)
{
	x_return_val_if_fail (queue, NULL);

	if (queue->head) {
		x_list_t *node = queue->head;
		void *data = node->data;

		queue->head = node->next;
		if (queue->head)
			queue->head->prev = NULL;
		else
			queue->tail = NULL;

		queue->length--;

		x_list_free_1 (node);
		return data;
	}

	return NULL;
}

void *
x_queue_pop_tail (x_queue_t *queue)
{
	x_return_val_if_fail (queue, NULL);

	if (queue->tail) {
		x_list_t *node = queue->tail;
		void *data = node->data;

		queue->tail = node->prev;
		if (queue->tail)
			queue->tail->next = NULL;
		else
			queue->head = NULL;

		queue->length--;

		x_list_free_1 (node);
		return data;
	}

	return NULL;
}

void *
x_queue_peek_head (x_queue_t *queue)
{
	x_return_val_if_fail (queue, NULL);

	return queue->head ? queue->head->data : NULL;
}

void *
x_queue_peek_tail (x_queue_t *queue)
{
	x_return_val_if_fail (queue, NULL);

	return queue->tail ? queue->tail->data : NULL;
}

bool
x_queue_is_empty (x_queue_t *queue)
{
	x_return_val_if_fail (queue, true);

	return queue->head == NULL;
}

