#include "gdb_stub.h"

#include "led.h"
#include "lib.h"
#include "serial.h"

void gdb_command_status(char *in, int inLen, char *out, int *outLen) {
	(void)in;
	(void)inLen;

	/* Ignore command, we don't use status. */
	out[0] = 'S';
	out[1] = '0';
	out[2] = '5';
	*outLen = 3;
}

void gdb_command_thread(char *in, int inLen, char *out, int *outLen) {
	(void)in;
	(void)inLen;

	/* Ignore command, we don't use threads. */
	out[0] = 'O';
	out[1] = 'K';
	*outLen = 2;
}

void gdb_command_unknown(char *in, int inLen, char *out, int *outLen) {
	(void)in;
	(void)inLen;
	(void)out;

	/* Send an empty packet if we don't know what to do. */
	*outLen = 0;
}

void gdb_command_read_memory(char *in, int inLen, char *out, int *outLen) {
	(void)inLen;

	uint32_t addr, len;
	char *next;

	/* Parse parameters. */
	addr = hex2word(&in[1], &next);
	len = hex2word(next+1, &next);

	if (len == 4) {
		uint32_t data = *(volatile uint32_t*)addr;
		byte2hex(data & 0xFF, out);
		byte2hex((data >> 8) & 0xFF, out+2);
		byte2hex((data >> 16) & 0xFF, out+4);
		byte2hex((data >> 24) & 0xFF, out+6);
		*outLen = 8;
	}
	else if (len == 2) {
		uint16_t data = *(volatile uint16_t*)addr;
		byte2hex(data & 0xFF, out);
		byte2hex((data >> 8) & 0xFF, out+2);
		*outLen = 4;
	}
	else {
		unsigned i;

		for (i = 0; i < len; i++) {
			uint8_t data = *(volatile uint8_t*)addr;
			byte2hex(data, out + 2*i);
		}

		*outLen = 2*i;
	}
}

void gdb_command_write_memory(char *in, int inLen, char *out, int *outLen) {
	(void)inLen;

	uint32_t addr, len;
	char *next;

	/* Parse parameters. */
	addr = hex2word(&in[1], &next);
	len = hex2word(next+1, &next);
	next++;

	if (len == 4) {
		uint32_t data = 0;
		for (int i = 0; i < 8; i++)
			data |= hex2byte(*next++) << ((i % 2 ? i-1:i+1) * 4);

		*(volatile uint32_t*)addr = data;
	}
	else if (len == 2) {
		uint16_t data = 0;
		for (int i = 0; i < 4; i++)
			data |= hex2byte(*next++) << ((i % 2 ? i-1:i+1) * 4);

		*(volatile uint16_t*)addr = data;
	}
	else {
		for (unsigned i = 0; i < len; i++) {
			uint8_t data = 0;
			for (int j = 0; j < 2; j++)
				data |= hex2byte(*next++) << ((1-j)*4);

			*(volatile uint8_t*)addr = data;
			addr++;
		}
	}

	out[0] = 'O';
	out[1] = 'K';
	*outLen = 2;
}

void gdb_command_read_registers(char *in, int inLen, char *out, int *outLen) {
	(void)in;
	(void)inLen;
	(void)out;

	const int EXTRA_REGS = 26;

	/*
	 * XXX: can't read from registers yet, we return all zeroes.
	 */

	for (int i = 0; i < 16 + EXTRA_REGS; i++)
		memset(&out[i*8], '0', 8);

	*outLen = (16 + EXTRA_REGS) * 8;
}

void gdb_command_write_registers(char *in, int inLen, char *out, int *outLen) {
	(void)in;
	(void)inLen;

	/*
	 * XXX: can't write to registers yet, ignore command.
	 */

	out[0] = 'O';
	out[1] = 'K';
	*outLen = 2;
}

int gdb_read_packet(char *in) {
	int cpt = 0;
	uint8_t chksum = 0;

	while (1) {
		char c = serial_pollc();

		if (c == '$') {
			/* Start of packet marker : start to record contents. */
			cpt = 0;
			chksum = 0;
		}
		else if (c == '#') {
			/* End of packet marker : verify checksum. */
			uint8_t msg_chksum;
			msg_chksum = hex2byte(serial_pollc());
			msg_chksum = msg_chksum << 4 | hex2byte(serial_pollc());

			if (chksum != msg_chksum)
				return -1;

			/* Put a null byte at the end, for string handling */
			in[cpt] = 0;

			return cpt;
		}
		else {
			/* Store into reception buffer */
			in[cpt++] = c;
			chksum += c;
		}
	}
}

void gdb_write_packet(char *out, int len) {
	uint8_t chksum = 0;

	/* Start of packet marker. */
	serial_putc('$');

	/* Output payload. */
	for (int i = 0; i < len; i++) {
		serial_putc(out[i]);
		chksum += out[i];
	}

	/* Output checksum. */
	char hexsum[2];
	byte2hex(chksum, hexsum);

	serial_putc('#');
	serial_putc(hexsum[0]);
	serial_putc(hexsum[1]);
}

void gdb_mainloop(void) {
	char in[GDB_PACKET_BUFFER_LEN], out[GDB_PACKET_BUFFER_LEN];
	int len, outLen;


	while (1) {
		led_set(LED_BLUE);

		len = gdb_read_packet(in);

		led_set(LED_BLUE | LED_RED);

		if (len == -1) {
			/* Bad checksum, NACK. */
			serial_putc('-');
			continue;
		}
		else
			/* ACK packet. */
			serial_putc('+');

		switch (in[0]) {
		case 'g':
			gdb_command_read_registers(in, len, out, &outLen);
			break;
		case 'G':
			gdb_command_write_registers(in, len, out, &outLen);
			break;
		case 'm':
			gdb_command_read_memory(in, len, out, &outLen);
			break;
		case 'M':
			gdb_command_write_memory(in, len, out, &outLen);
			break;
		case 'H':
			gdb_command_thread(in, len, out, &outLen);
			break;
		case '?':
			gdb_command_status(in, len, out, &outLen);
			break;
		default:
			gdb_command_unknown(in, len, out, &outLen);
			break;
		}

		/* Write out reply. */
		gdb_write_packet(out, outLen);
	}
}
