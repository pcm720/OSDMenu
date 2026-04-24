#ifndef MINISTACK_DHCP_H
#define MINISTACK_DHCP_H

#include <stdint.h>

/**
 * Blocking DHCP client (DISCOVER → OFFER → REQUEST → ACK).
 * Sets the ministack IPv4 address on success. Requires ip_addr 0 and link up.
 * @return 0 on success, -1 on timeout or error
 */
int ms_dhcp_acquire(void);

/**
 * RX hook from handle_rx_udp; returns -1 if this packet is not for DHCP.
 */
int ms_dhcp_try_rx(const uint8_t *hdr, uint16_t hdr_len, uint16_t frame_len);

#endif
