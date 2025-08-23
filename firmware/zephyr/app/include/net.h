#pragma once

// Networking stubs (Wi‑Fi manager + uploader)

int net_init(void);
int net_connect_async(void);

// Wi‑Fi provisioning expects /SD:/wifi.txt with lines:
// ssid=YourNetwork
// psk=YourPassword

// Status helpers
bool net_is_wifi_connected(void);
