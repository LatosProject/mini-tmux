#ifndef INPUT_H
#define INPUT_H

#include "window.h"
#include <stddef.h>

void pane_input(struct window_pane *p, const char *data, size_t len);

#endif /* INPUT_H */
