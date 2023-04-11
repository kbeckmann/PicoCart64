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

typedef struct {
	ip_addr_t server_address;
	struct udp_pcb *out_udp_pcb;
} state_t;

// Packets
typedef enum {
	CMD_HELLO = 0x4142,
} cmd_type_t;

typedef struct __attribute__((packed)) {
	uint16_t type;
} pkt_cmd_hello_t;

typedef struct __attribute__((packed)) {
	uint16_t type;
} pkt_hello_t;

///////////////////

static void dump_bytes(const uint8_t *bptr, uint32_t len)
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

// err_t pbuf_copy(struct pbuf *p_to, const struct pbuf *p_from);
// u16_t pbuf_copy_partial(const struct pbuf *p, void *dataptr, u16_t len, u16_t offset);

// NTP data received
static void udp_recv_cb(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
	state_t *state = (state_t *) arg;

	// Check the result
	if (ip_addr_cmp(addr, &state->server_address) && port == REMOTE_PORT) {
		printf("Expected packet\n", addr, port);
		u16_t ret = pbuf_copy_partial(p, sram, p->tot_len, 0);
		dump_bytes(sram, ret);
	} else {
		printf("Unexpected packet (ip=%08x, port=%d)\n", addr, port);
		u16_t ret = pbuf_copy_partial(p, sram, p->tot_len, 0);
		dump_bytes(sram, ret);
	}

	pbuf_free(p);
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

static void cmd_hello(state_t *state)
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
		vTaskDelay(10000);
	}
}
