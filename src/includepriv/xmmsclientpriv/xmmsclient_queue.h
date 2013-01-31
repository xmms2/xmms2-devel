
#ifndef __X_QUEUE_H_
#define __X_QUEUE_H_

#include "xmmsc/xmmsc_stdbool.h"
#include "xmmscpriv/xmms_list.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	x_list_t *head;
	x_list_t *tail;
	unsigned int length;
} x_queue_t;

x_queue_t *x_queue_new (void);
void x_queue_free (x_queue_t *queue);
void x_queue_push_head (x_queue_t *queue, void *data);
void x_queue_push_tail (x_queue_t *queue, void *data);
void* x_queue_peek_head (x_queue_t *queue);
void* x_queue_peek_tail (x_queue_t *queue);
void* x_queue_pop_head (x_queue_t *queue);
void* x_queue_pop_tail (x_queue_t *queue);
bool x_queue_is_empty (x_queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif /* __X_QUEUE_H_ */
