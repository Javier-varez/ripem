#include "defs.h"
#include "elf_loader.h"
#include "keypad.h"
#include "led.h"
#include "serial.h"
#include "syscon.h"
#include "rtc.h"

#include "gdb_stub.h"

const char *ripem_version = "0.0.1";

#define PAYLOAD_STAGING_AREA 0x31000000

extern uint32_t _binary_payload_start;
extern uint32_t _binary_payload_size;

void launch_payload(unsigned r0, void *initial_stack) {
	void *payload_ptr = &_binary_payload_start;
	uint32_t payload_size = (uint32_t)&_binary_payload_size;

	uint32_t entry;
	char buffer[32];

	/* Dump info about payload. */
	serial_puts("Payload address : 0x");
	serial_puts(itoa((uint32_t)payload_ptr, buffer, 16));
	serial_putc('\n');

	serial_puts("Payload size : ");
	serial_puts(itoa((uint32_t)payload_size, buffer, 10));
	serial_puts(" bytes\n");

	serial_puts("Payload stack : 0x");
	serial_puts(itoa((uint32_t)initial_stack, buffer, 16));
	serial_putc('\n');

	serial_puts("Payload staging area : 0x");
	serial_puts(itoa(PAYLOAD_STAGING_AREA, buffer, 16));
	serial_putc('\n');

	/* Move payload out of the way. */
	memcpy((char*)PAYLOAD_STAGING_AREA, payload_ptr, payload_size);

	/* Load payload. */
	switch (load_elf((char*)PAYLOAD_STAGING_AREA, &entry)) {
	case ELF_OK:
		serial_puts("Payload entry : 0x");
		serial_puts(itoa(entry, buffer, 16));
		serial_puts("\nLoading successful, jumping into payload...\n");

		run_elf(r0, initial_stack, entry);

	case ELF_ERR_INVALID_HEADER:
		serial_puts("Invalid ELF header! Aborting.\n");
		led_set(LED_RED);

		/* Reboot when ON is pressed. */
		while (!keypad_get(KEY_ON));
		syscon_reset();

	case ELF_ERR_INVALID_PROGRAM_HEADER:
		serial_puts("Invalid ELF program header! Aborting.\n");
		led_set(LED_RED);

		/* Reboot when ON is pressed. */
		while (!keypad_get(KEY_ON));
		syscon_reset();
	}

}

void main(unsigned r0, void *initial_stack) {
	int sec, prev_sec;

	led_init();
	keypad_init();
	serial_init(115200);
	rtc_init();

	/*
	 * Print propaganda early on.
	 */
	serial_puts("\nRip'Em version ");
	serial_puts(ripem_version);
	serial_puts("\n\n");

	/*
	 * Align ourselves on the next second, then wait for one second.
	 * If the ON key is kept pressed during all that time, run the GDB stub.
	 * Otherwise, run the payload.
	 */
	for (int delay = 0; delay < 2; delay++) {
		rtc_get_time(NULL, NULL, NULL, NULL, NULL, &sec);
		prev_sec = sec;

		/*
		 * XXX : abuse the RTC for sleeping for about a second.
		 */
		do {
			rtc_get_time(NULL, NULL, NULL, NULL, NULL, &sec);
			if (keypad_get(KEY_ON) == 0)
				goto launch_payload;
		} while (prev_sec == sec);
	}

	serial_puts("Launching GDB stub...\n");
	gdb_mainloop(r0, initial_stack);

launch_payload:
	serial_puts("Loading payload...\n");
	launch_payload(r0, initial_stack);

	__builtin_unreachable();
}
