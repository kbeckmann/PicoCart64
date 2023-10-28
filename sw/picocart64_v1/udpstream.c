#include "udpstream.h"

#include "FreeRTOS.h"
#include "task.h"

#include "pico/cyw43_arch.h"

#include "lwip/ip4_addr.h"
#include "lwip/dns.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

#include "sram.h"

#define REMOTE_ADDR "192.168.5.18"
#define REMOTE_PORT 1111
#define LOCAL_PORT  1112

uint8_t recv_buf[1024];
uint32_t button_state;
uint32_t prev_button_state;

typedef struct {
	ip_addr_t server_address;
	struct udp_pcb *out_udp_pcb;
} state_t;

// struct udp_pcb *button_udp_pcb;

///////////////////////////
// Packets to server
typedef enum {
	CMD_HELLO = 0x41414141,
	CMD_BUTTONS = 0x43434343,
} cmd_type_t;

typedef struct __attribute__((packed)) {
	uint32_t type;
} pkt_cmd_hello_t;

typedef struct __attribute__((packed)) {
	uint32_t type;
	uint32_t buttons;
} pkt_cmd_buttons_t;

///////////////////////////

///////////////////////////
// Packets from server
typedef enum {
	RSP_FB = 0x42424242,
} rsp_type_t;

typedef struct __attribute__((packed)) {
	uint32_t type;
} pkt_rsp_t;

typedef struct __attribute__((packed)) {
	uint32_t type;
	uint32_t offset;
	uint8_t data[];				// 4-byte aligned
} pkt_rsp_fb_t;

///////////////////////////

static void dump_bytes(const uint8_t * bptr, uint32_t len)
{
	unsigned int i = 0;

	printf("dump_bytes %d", len);
	for (i = 0; i < len;) {
		if ((i & 0x0f) == 0) {
			printf("\n");
		} else if ((i & 0x07) == 0) {
			printf(" ");
		}
		printf("%02x ", bptr[i++]);
	}
	printf("\n");
}

static void cmd_buttons(state_t * state, uint32_t buttons)
{
	cyw43_arch_lwip_begin();

	struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, sizeof(pkt_cmd_buttons_t), PBUF_RAM);

	pkt_cmd_buttons_t *cmd = (pkt_cmd_buttons_t *) p->payload;
	cmd->type = CMD_BUTTONS;
	cmd->buttons = buttons;

	udp_sendto(state->out_udp_pcb, p, &state->server_address, REMOTE_PORT);

	pbuf_free(p);

	cyw43_arch_lwip_end();
}

// err_t pbuf_copy(struct pbuf *p_to, const struct pbuf *p_from);
// u16_t pbuf_copy_partial(const struct pbuf *p, void *dataptr, u16_t len, u16_t offset);

// NTP data received
static void udp_recv_cb(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t * addr, u16_t port)
{
	state_t *state = (state_t *) arg;

	// Check the result
	// if (ip_addr_cmp(addr, &state->server_address) && port == REMOTE_PORT && p->tot_len >= 2) {
	// printf("Packet %d:%d len=%d\n", addr, port, p->tot_len);

	pkt_rsp_fb_t pkt_rsp_fb;
	u16_t ret = pbuf_copy_partial(p, &pkt_rsp_fb, 4, 0);

	if (pkt_rsp_fb.type == RSP_FB) {
		ret = pbuf_copy_partial(p, &pkt_rsp_fb.offset, 4, 4);
		uint32_t copylen = p->tot_len - 8;
		uint32_t copyend = pkt_rsp_fb.offset + copylen;
		if (pkt_rsp_fb.offset < sizeof(sram) && copyend <= sizeof(sram)) {
			// printf("Good, offset=%d, copylen=%d\n", pkt_rsp_fb.offset, copylen);
			u16_t ret = pbuf_copy_partial(p, &sram[pkt_rsp_fb.offset / 2], copylen, 8);
		} else {
			printf("Bad, offset=%d, copylen=%d\n", pkt_rsp_fb.offset, copylen);
		}
	} else {
		printf("Unexpected pkt_rsp_fb.type=%08X\n", pkt_rsp_fb.type);
	}
	// } else {
	// printf("Unexpected packet (ip=%d, port=%d), expected cmd=%d, addr=%d\n", addr, port, ip_addr_cmp(addr, &state->server_address), state->server_address);
	// uint32_t copylen = p->tot_len < sizeof(recv_buf) ? p->tot_len : sizeof(recv_buf);
	// u16_t ret = pbuf_copy_partial(p, recv_buf, copylen, 0);
	// dump_bytes(recv_buf, ret);
	// }

	pbuf_free(p);

	if (button_state != prev_button_state) {
		// Button state changed between frames
		printf("Buttons: %04X\n", button_state);
		cmd_buttons(state, button_state);

		prev_button_state = button_state;
	}
}

static state_t *state_init(void)
{
	state_t *state = calloc(1, sizeof(state_t));
	if (!state) {
		printf("Failed to allocate state\n");
		return NULL;
	}

	state->out_udp_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
	if (!state->out_udp_pcb) {
		printf("failed to create pcb\n");
		free(state);
		return NULL;
	}

	ip4_addr_set_u32(&state->server_address, ipaddr_addr(REMOTE_ADDR));

	udp_recv(state->out_udp_pcb, udp_recv_cb, state);

	return state;
}

static void cmd_hello(state_t * state)
{
	cyw43_arch_lwip_begin();

	struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, sizeof(pkt_cmd_hello_t), PBUF_RAM);
	pkt_cmd_hello_t *cmd = (pkt_cmd_hello_t *) p->payload;
	cmd->type = CMD_HELLO;

	udp_sendto(state->out_udp_pcb, p, &state->server_address, REMOTE_PORT);

	pbuf_free(p);

	cyw43_arch_lwip_end();
}

void udpstream_task_entry(void *params)
{
	int ret = 0;

	printf("Calling cyw43_arch_init()\n");
	if (cyw43_arch_init()) {
		printf("cyw43_arch_init() failed\n");
		goto error;
	}

	cyw43_arch_enable_sta_mode();

	// Try to connect to WiFi forever
	do {
		printf("Connecting to WiFi (%s)...\n", WIFI_SSID);

		ret = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000);
		if (ret != 0) {
			printf("Failed to connect.\n");
			vTaskDelay(1000);
		}
	} while (ret != 0);

	printf("Connected to wifi.\n");

	state_t *state = state_init();
	printf("state_init() = %p\n", state);

	cmd_hello(state);
	printf("cmd_hello() OK\n");

	// cyw43_arch_lwip_begin();

	// struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, NTP_MSG_LEN, PBUF_RAM);
	// uint8_t *req = (uint8_t *) p->payload;
	// memset(req, 0, NTP_MSG_LEN);
	// req[0] = 0x1b;
	// udp_sendto(state->ntp_pcb, p, &state->ntp_server_address, NTP_PORT);
	// pbuf_free(p);
	// cyw43_arch_lwip_end();

 error:
	while (true) {
		// This is not good... Should use pico_cyw43_arch_lwip_sys_freertos but it needs some extra work.
		cyw43_arch_poll();
		// vTaskDelay(10000);
	}
}

void pc64_button_state(uint32_t state)
{
	button_state = state;
}
