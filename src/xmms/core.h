#ifndef __XMMS_CORE_H__
#define __XMMS_CORE_H__

#include "xmms/object.h"
#include "xmms/output.h"

extern xmms_object_t *core;


void xmms_core_output_set (xmms_output_t *output);

void xmms_core_playtime_set (guint time);
void xmms_core_play_next ();

void xmms_core_init ();
void xmms_core_start ();

#endif
