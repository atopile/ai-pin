// Core / logging
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

// Filesystem
#include <zephyr/fs/fs.h>

// Networking
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <ctype.h>

#include "net.h"

LOG_MODULE_REGISTER(ai_net, LOG_LEVEL_INF);

static struct k_thread wifi_thread_data;
static K_THREAD_STACK_DEFINE(wifi_stack, 2048);
static k_tid_t wifi_tid;

static struct net_mgmt_event_callback wifi_cb;
static struct net_if *wifi_iface;

static volatile bool wifi_connected = false;
bool net_is_wifi_connected(void)
{
    return wifi_connected;
}

static void wifi_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event, struct net_if *iface)
{
    switch (mgmt_event)
    {
    case NET_EVENT_WIFI_CONNECT_RESULT:
        LOG_INF("Wi‑Fi connect result: %d", cb->info ? *(int *)cb->info : 0);
        break;
    case NET_EVENT_WIFI_DISCONNECT_RESULT:
        LOG_WRN("Wi‑Fi disconnected");
        wifi_connected = false;
        break;
    case NET_EVENT_IPV4_ADDR_ADD:
        LOG_INF("Got IPv4 address");
        wifi_connected = true;
        break;
    default:
        break;
    }
}

static int read_wifi_txt(char *ssid, size_t ssid_sz, char *psk, size_t psk_sz)
{
    struct fs_file_t file;
    fs_file_t_init(&file);
    int rc = fs_open(&file, "/SD:/wifi.txt", FS_O_READ);
    if (rc < 0)
    {
        return rc;
    }
    char buf[256];
    ssize_t n = fs_read(&file, buf, sizeof(buf) - 1);
    fs_close(&file);
    if (n <= 0)
    {
        return -EIO;
    }
    buf[n] = '\0';
    // Simple parser: lines like "ssid=..." and "psk=..." (case‑insensitive)
    char *line = buf;
    while (line && *line)
    {
        char *next = strchr(line, '\n');
        if (next)
            *next = '\0';
        // trim leading spaces
        while (*line == ' ' || *line == '\t' || *line == '\r')
            line++;
        // find '=' or ':'
        char *sep = strchr(line, '=');
        if (!sep)
            sep = strchr(line, ':');
        if (sep)
        {
            *sep = '\0';
            char *key = line;
            char *val = sep + 1;
            // trim key end
            for (char *p = key; *p; ++p)
                *p = (char)tolower((unsigned char)*p);
            while (*val == ' ' || *val == '\t')
                val++;
            if (strcmp(key, "ssid") == 0)
            {
                strncpy(ssid, val, ssid_sz - 1);
                ssid[ssid_sz - 1] = '\0';
            }
            else if (strcmp(key, "psk") == 0 || strcmp(key, "password") == 0 || strcmp(key, "pass") == 0)
            {
                strncpy(psk, val, psk_sz - 1);
                psk[psk_sz - 1] = '\0';
            }
        }
        line = next ? (next + 1) : NULL;
    }
    return 0;
}

static int wifi_connect_with_credentials(const char *ssid, const char *psk)
{
    struct wifi_connect_req_params params = {0};
    params.ssid = ssid;
    params.ssid_length = strlen(ssid);
    params.psk = psk;
    params.psk_length = strlen(psk);
    params.security = WIFI_SECURITY_TYPE_PSK;
    params.channel = WIFI_CHANNEL_ANY;
    params.mfp = WIFI_MFP_OPTIONAL;

    int rc = net_mgmt(NET_REQUEST_WIFI_CONNECT, wifi_iface, &params, sizeof(params));
    if (rc)
    {
        LOG_ERR("Wi‑Fi connect request failed: %d", rc);
        return rc;
    }
    return 0;
}

static void wifi_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    // Subscribe to Wi‑Fi and IP events
    net_mgmt_init_event_callback(&wifi_cb, (net_mgmt_event_handler_t)wifi_event_handler,
                                 NET_EVENT_WIFI_CONNECT_RESULT |
                                     NET_EVENT_WIFI_DISCONNECT_RESULT |
                                     NET_EVENT_IPV4_ADDR_ADD);
    net_mgmt_add_event_callback(&wifi_cb);

    wifi_iface = net_if_get_default();
    if (!wifi_iface)
    {
        LOG_ERR("No default net_if");
        return;
    }

    // Poll for credentials file on SD card
    char ssid[64] = {0};
    char psk[64] = {0};
    while (read_wifi_txt(ssid, sizeof(ssid), psk, sizeof(psk)) < 0)
    {
        k_sleep(K_SECONDS(1));
    }
    LOG_INF("Read Wi‑Fi credentials for SSID='%s'", ssid);

    // Attempt connection
    int rc = wifi_connect_with_credentials(ssid, psk);
    if (rc)
    {
        return;
    }

    // Wait until we are connected (got IPv4) or timeout
    int64_t deadline = k_uptime_get() + 2000; // 2s
    while (!wifi_connected && k_uptime_get() < deadline)
    {
        k_sleep(K_MSEC(200));
    }
    if (wifi_connected)
    {
        LOG_INF("Wi‑Fi connected and IPv4 acquired");
    }
    else
    {
        LOG_WRN("Wi‑Fi connect timeout");
    }
}

int net_init(void)
{
    if (wifi_tid)
        return 0;
    wifi_tid = k_thread_create(&wifi_thread_data, wifi_stack, K_THREAD_STACK_SIZEOF(wifi_stack),
                               wifi_thread, NULL, NULL, NULL, K_PRIO_PREEMPT(8), 0, K_NO_WAIT);
    return 0;
}

int net_connect_async(void)
{
    // Thread starts in net_init
    return 0;
}
