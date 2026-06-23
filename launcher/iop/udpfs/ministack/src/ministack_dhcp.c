#include "ministack_dhcp.h"
#include "ministack_ip.h"
#include "ministack_udp.h"
#include "smap.h"

#include <sysclib.h>
#include <thbase.h>

#define DHCP_UDP_PAYLOAD_MAX 400

#define DHCP_MAGIC0 0x63
#define DHCP_MAGIC1 0x82
#define DHCP_MAGIC2 0x53
#define DHCP_MAGIC3 0x63

#define DHCPDISCOVER 1
#define DHCPOFFER    2
#define DHCPREQUEST  3
#define DHCPACK      5

#define DHCP_P_IDLE       0
#define DHCP_P_WAIT_OFFER 1
#define DHCP_P_WAIT_ACK   2

static volatile int dhcp_active;
static volatile int dhcp_phase;
static uint32_t dhcp_xid;
static uint32_t dhcp_server_ip;
static uint32_t dhcp_offered_ip;
static volatile int dhcp_result; /* 0 pending, 1 success */

static uint32_t rd_u32_be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void wr_u32_be(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint8_t dhcp_parse_msgtype(const uint8_t *opt, int optlen)
{
    int i = 0;

    while (i < optlen) {
        if (opt[i] == 255)
            return 0;
        if (opt[i] == 0) {
            i++;
            continue;
        }
        if (i + 2 > optlen)
            break;
        {
            uint8_t code = opt[i];
            uint8_t olen = opt[i + 1];

            if (i + 2 + olen > optlen)
                break;
            if (code == 53 && olen >= 1)
                return opt[i + 2];
            i += 2 + olen;
        }
    }
    return 0;
}

static void dhcp_drain(const uint8_t *hdr, uint16_t frame_len, uint16_t from_off)
{
    const ip_header_t *ip = (const ip_header_t *)(hdr + 14);
    uint16_t ip_total = ntohs(ip->len);
    uint16_t eth_end  = 14 + ip_total;
    uint8_t junk[128];

    if (eth_end > frame_len)
        eth_end = frame_len;
    while (from_off < eth_end) {
        uint16_t n = (uint16_t)(eth_end - from_off);

        if (n > sizeof(junk))
            n = (uint16_t)sizeof(junk);
        smap_fifo_read(from_off, junk, n);
        from_off = (uint16_t)(from_off + n);
    }
    /* Ethernet padding beyond IP: use full BD length */
    while (from_off < frame_len) {
        uint16_t n = (uint16_t)(frame_len - from_off);

        if (n > sizeof(junk))
            n = (uint16_t)sizeof(junk);
        smap_fifo_read(from_off, junk, n);
        from_off = (uint16_t)(from_off + n);
    }
}

static int dhcp_read_payload(const uint8_t *hdr, uint16_t frame_len, uint8_t *out, uint16_t *out_len)
{
    const udp_packet_t *u = (const udp_packet_t *)hdr;
    uint8_t ip_ihl       = (uint8_t)((u->ip.hlen & 0x0f) * 4);
    uint16_t udp_total  = ntohs(u->udp.len);
    uint16_t dhcp_start = (uint16_t)(14 + ip_ihl + 8);
    uint16_t dhcp_len;

    if (udp_total < 8 || udp_total > 8 + DHCP_UDP_PAYLOAD_MAX)
        return -1;
    dhcp_len = (uint16_t)(udp_total - 8);
    if ((uint32_t)dhcp_start + dhcp_len > frame_len)
        return -1;
    if (dhcp_len > DHCP_UDP_PAYLOAD_MAX)
        return -1;
    smap_fifo_read(dhcp_start, out, dhcp_len);
    *out_len = dhcp_len;
    return 0;
}

static int dhcp_handle_reply(const uint8_t *hdr, uint16_t frame_len)
{
    const udp_packet_t *pkt = (const udp_packet_t *)hdr;
    uint8_t buf[DHCP_UDP_PAYLOAD_MAX];
    uint16_t plen;
    uint8_t msg;
    uint32_t yiaddr;
    uint32_t xid_pkt;
    uint32_t srv;

    if (ntohs(pkt->udp.port_dst) != 68 || ntohs(pkt->udp.port_src) != 67)
        return -1;

    if (dhcp_read_payload(hdr, frame_len, buf, &plen) < 0) {
        dhcp_drain(hdr, frame_len, 44);
        return 0;
    }

    if (plen < 240 || buf[0] != 2 /* BOOTREPLY */)
        goto drain;

    xid_pkt = rd_u32_be(buf + 4);
    if (xid_pkt != dhcp_xid)
        goto drain;

    yiaddr = rd_u32_be(buf + 16);
    if (buf[236] != DHCP_MAGIC0 || buf[237] != DHCP_MAGIC1 || buf[238] != DHCP_MAGIC2 || buf[239] != DHCP_MAGIC3)
        goto drain;

    msg = dhcp_parse_msgtype(buf + 240, plen - 240);
    srv = IP_ADDR(pkt->ip.addr_src.addr[0], pkt->ip.addr_src.addr[1], pkt->ip.addr_src.addr[2], pkt->ip.addr_src.addr[3]);

    if (dhcp_phase == DHCP_P_WAIT_OFFER) {
        if (msg == DHCPOFFER && yiaddr != 0) {
            dhcp_server_ip  = srv;
            dhcp_offered_ip = yiaddr;
            dhcp_phase      = DHCP_P_WAIT_ACK;
        }
    } else if (dhcp_phase == DHCP_P_WAIT_ACK) {
        if (msg == DHCPACK && yiaddr == dhcp_offered_ip) {
            ms_ip_set_ip(yiaddr);
            dhcp_phase  = DHCP_P_IDLE;
            dhcp_active = 0;
            dhcp_result = 1;
        }
    }

drain:
    {
        uint8_t ip_ihl      = (uint8_t)((pkt->ip.hlen & 0x0f) * 4);
        uint16_t dhcp_start = (uint16_t)(14 + ip_ihl + 8);
        uint16_t end        = (uint16_t)(dhcp_start + plen);

        dhcp_drain(hdr, frame_len, end);
    }
    return 0;
}

int ms_dhcp_try_rx(const uint8_t *hdr, uint16_t hdr_len, uint16_t frame_len)
{
    const udp_packet_t *pkt = (const udp_packet_t *)hdr;

    if (!dhcp_active)
        return -1;
    if (hdr_len < 42)
        return -1;
    if (ntohs(pkt->udp.port_dst) != 68)
        return -1;
    return dhcp_handle_reply(hdr, frame_len);
}

static void dhcp_fill_base(uint8_t *b, uint32_t xid, const uint8_t mac[6])
{
    memset(b, 0, DHCP_UDP_PAYLOAD_MAX);
    b[0]  = 1; /* BOOTREQUEST */
    b[1]  = 1; /* Ethernet */
    b[2]  = 6;
    wr_u32_be(b + 4, xid);
    b[10] = 0x80; /* broadcast flags */
    b[11] = 0;
    memcpy(b + 28, mac, 6);
    b[236] = DHCP_MAGIC0;
    b[237] = DHCP_MAGIC1;
    b[238] = DHCP_MAGIC2;
    b[239] = DHCP_MAGIC3;
}

static uint16_t dhcp_send(uint8_t *payload, uint16_t paylen)
{
    udp_packet_t pkt;
    udp_socket_t sock = {68, NULL, NULL};

    udp_packet_init(&pkt, 0xFFFFFFFFu, 67);
    udp_packet_send_ll(&sock, &pkt, 0, payload, paylen);
    return paylen;
}

static void dhcp_send_discover(uint32_t xid, const uint8_t mac[6])
{
    uint8_t b[DHCP_UDP_PAYLOAD_MAX];
    int o;

    dhcp_fill_base(b, xid, mac);
    o = 240;
    b[o++] = 53;
    b[o++] = 1;
    b[o++] = DHCPDISCOVER;
    b[o++] = 255;
    dhcp_send(b, (uint16_t)o);
}

static void dhcp_send_request(uint32_t xid, const uint8_t mac[6], uint32_t yiaddr, uint32_t server_ip)
{
    uint8_t b[DHCP_UDP_PAYLOAD_MAX];
    int o = 240;

    dhcp_fill_base(b, xid, mac);
    b[o++] = 53;
    b[o++] = 1;
    b[o++] = DHCPREQUEST;
    /* requested IP */
    b[o++] = 50;
    b[o++] = 4;
    wr_u32_be(b + o, yiaddr);
    o += 4;
    /* server ID */
    b[o++] = 54;
    b[o++] = 4;
    wr_u32_be(b + o, server_ip);
    o += 4;
    b[o++] = 255;
    dhcp_send(b, (uint16_t)o);
}

int ms_dhcp_acquire(void)
{
    uint8_t mac[6];
    int attempt, spin;

    SMAPGetMACAddress(mac);
    dhcp_xid = rd_u32_be(mac) ^ 0x2D2E4F50u;
    if (dhcp_xid == 0)
        dhcp_xid = 1;

    for (attempt = 0; attempt < 8; attempt++) {
        dhcp_active = 1;
        dhcp_phase  = DHCP_P_WAIT_OFFER;
        dhcp_result = 0;
        dhcp_offered_ip = 0;
        dhcp_server_ip  = 0;

        dhcp_send_discover(dhcp_xid, mac);

        for (spin = 0; spin < 400; spin++) { /* ~4 s */
            if (dhcp_phase != DHCP_P_WAIT_OFFER)
                break;
            DelayThread(10000);
        }

        if (dhcp_phase != DHCP_P_WAIT_ACK || dhcp_offered_ip == 0) {
            dhcp_active = 0;
            dhcp_phase  = DHCP_P_IDLE;
            continue;
        }

        dhcp_result = 0;
        dhcp_send_request(dhcp_xid, mac, dhcp_offered_ip, dhcp_server_ip);

        for (spin = 0; spin < 400; spin++) {
            if (dhcp_result == 1)
                break;
            DelayThread(10000);
        }

        dhcp_active = 0;
        dhcp_phase  = DHCP_P_IDLE;
        if (dhcp_result == 1)
            return 0;
    }

    return -1;
}
