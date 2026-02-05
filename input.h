#ifndef INPUT_H
#define INPUT_H

#include "window.h"
#include <stddef.h>

void pane_input(struct window_pane *p, const char *data, size_t len);
void sync_vterm_from_grid(struct window_pane *p);

#endif /* INPUT_H */
