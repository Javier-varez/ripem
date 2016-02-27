#ifndef _CPIO_H
#define _CPIO_H

typedef struct {
	char name[36];
	void *location;
} payload_item;

int parse_payloads(payload_item *items);
void menu_payloads(payload_item *items, int nb, unsigned r0, void *initial_stack);

#endif
