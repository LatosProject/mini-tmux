#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "client.h"

enum key_table { KEY_PREFIX };

void handle_key(struct client *c, enum key_table table, char key);
void keybind_init(void);

#endif /* KEYBOARD_H */
