#include "easytier_wg.h"
#include "settings.h"

#include <cstring>
#include <esp_log.h>
#include <esp_wireguard.h>

#define TAG "EasytierWg"

// ── anonymous helpers ──────────────────────────────────────────────────────

static char* dup_str(const std::string& s) {
    if (s.empty()) {
        return nullptr;
    }
    char* p = new char[s.size() + 1];
    std::memcpy(p, s.c_str(), s.size() + 1);
    return p;
}

// ── singleton ─────────────────────────────────────────────────────────────

static wireguard_config_t s_wg_config = ESP_WIREGUARD_CONFIG_DEFAULT();
static wireguard_ctx_t    s_wg_ctx    = {};

EasytierWg& EasytierWg::GetInstance() {
    static EasytierWg instance;
    return instance;
}

EasytierWg::~EasytierWg() {
    Stop();
}

// ── private helpers ────────────────────────────────────────────────────────

void EasytierWg::FreeBuffers() {
    delete[] private_key_buf_;    private_key_buf_     = nullptr;
    delete[] peer_pubkey_buf_;    peer_pubkey_buf_     = nullptr;
    delete[] preshared_key_buf_;  preshared_key_buf_   = nullptr;
    delete[] endpoint_buf_;       endpoint_buf_        = nullptr;
    delete[] allowed_ip_buf_;     allowed_ip_buf_      = nullptr;
    delete[] allowed_ip_mask_buf_;allowed_ip_mask_buf_ = nullptr;
}

// ── public API ─────────────────────────────────────────────────────────────

bool EasytierWg::IsEnabled() const {
    Settings settings("easytier", false);
    // NVS value overrides sdkconfig; if not stored, fall back to sdkconfig.
    bool nvs_val = settings.GetBool("enabled",
#ifdef CONFIG_EASYTIER_WG_ENABLED
        true
#else
        false
#endif
    );
    return nvs_val;
}

bool EasytierWg::IsConnected() const {
    if (!started_) {
        return false;
    }
    return esp_wireguardif_peer_is_up(const_cast<wireguard_ctx_t*>(&s_wg_ctx)) == ESP_OK;
}

std::string EasytierWg::GetVpnIpAddress() const {
    Settings settings("easytier", false);
    std::string ip = settings.GetString("local_ip",
#ifdef CONFIG_EASYTIER_WG_LOCAL_IP
        CONFIG_EASYTIER_WG_LOCAL_IP
#else
        "10.0.0.2"
#endif
    );
    return ip;
}

void EasytierWg::Start() {
    if (started_) {
        ESP_LOGW(TAG, "WireGuard tunnel already started");
        return;
    }

    if (!IsEnabled()) {
        ESP_LOGI(TAG, "EasyTier WireGuard is disabled — skipping");
        return;
    }

    // ── Read configuration from NVS, with sdkconfig defaults ────────────

    Settings settings("easytier", false);

    std::string private_key = settings.GetString("private_key",
#ifdef CONFIG_EASYTIER_WG_PRIVATE_KEY
        CONFIG_EASYTIER_WG_PRIVATE_KEY
#else
        ""
#endif
    );

    std::string peer_pubkey = settings.GetString("peer_pub_key",
#ifdef CONFIG_EASYTIER_WG_PEER_PUBLIC_KEY
        CONFIG_EASYTIER_WG_PEER_PUBLIC_KEY
#else
        ""
#endif
    );

    std::string preshared_key = settings.GetString("preshared_key",
#ifdef CONFIG_EASYTIER_WG_PRESHARED_KEY
        CONFIG_EASYTIER_WG_PRESHARED_KEY
#else
        ""
#endif
    );

    std::string endpoint = settings.GetString("endpoint",
#ifdef CONFIG_EASYTIER_WG_ENDPOINT
        CONFIG_EASYTIER_WG_ENDPOINT
#else
        ""
#endif
    );

    int port = settings.GetInt("port",
#ifdef CONFIG_EASYTIER_WG_PORT
        CONFIG_EASYTIER_WG_PORT
#else
        51820
#endif
    );

    std::string local_ip = settings.GetString("local_ip",
#ifdef CONFIG_EASYTIER_WG_LOCAL_IP
        CONFIG_EASYTIER_WG_LOCAL_IP
#else
        "10.0.0.2"
#endif
    );

    std::string local_ip_mask = settings.GetString("local_ip_mask",
#ifdef CONFIG_EASYTIER_WG_LOCAL_IP_MASK
        CONFIG_EASYTIER_WG_LOCAL_IP_MASK
#else
        "255.255.255.0"
#endif
    );

    int keepalive = settings.GetInt("keepalive",
#ifdef CONFIG_EASYTIER_WG_KEEPALIVE
        CONFIG_EASYTIER_WG_KEEPALIVE
#else
        25
#endif
    );

    // ── Validate required fields ────────────────────────────────────────

    if (private_key.empty()) {
        ESP_LOGE(TAG, "private_key is not configured — aborting");
        return;
    }
    if (peer_pubkey.empty()) {
        ESP_LOGE(TAG, "peer_pub_key is not configured — aborting");
        return;
    }
    if (endpoint.empty()) {
        ESP_LOGE(TAG, "endpoint is not configured — aborting");
        return;
    }

    // ── Duplicate strings into heap buffers ─────────────────────────────
    // The WireGuard library keeps raw char* pointers; we must keep them alive.

    FreeBuffers();
    private_key_buf_     = dup_str(private_key);
    peer_pubkey_buf_     = dup_str(peer_pubkey);
    preshared_key_buf_   = preshared_key.empty() ? nullptr : dup_str(preshared_key);
    endpoint_buf_        = dup_str(endpoint);
    allowed_ip_buf_      = dup_str(local_ip);
    allowed_ip_mask_buf_ = dup_str(local_ip_mask);

    // ── Build wireguard_config_t ─────────────────────────────────────────
    // Note: wireguard_config_t.public_key holds the *peer's* public key
    // (the field name reflects the WireGuard protocol naming convention where
    // the remote side's key is referenced as "public_key" in the peer config).
    s_wg_config.private_key      = private_key_buf_;
    s_wg_config.public_key       = peer_pubkey_buf_;
    s_wg_config.preshared_key    = preshared_key_buf_;
    s_wg_config.endpoint         = endpoint_buf_;
    s_wg_config.port             = port;
    s_wg_config.allowed_ip       = allowed_ip_buf_;
    s_wg_config.allowed_ip_mask  = allowed_ip_mask_buf_;
    s_wg_config.persistent_keepalive = keepalive;

    // ── Initialize and connect ───────────────────────────────────────────

    ESP_LOGI(TAG, "Initializing WireGuard tunnel to %s:%d (local IP %s)",
             endpoint.c_str(), port, local_ip.c_str());

    esp_err_t err = esp_wireguard_init(&s_wg_config, &s_wg_ctx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wireguard_init failed: %s", esp_err_to_name(err));
        FreeBuffers();
        return;
    }

    err = esp_wireguard_connect(&s_wg_ctx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wireguard_connect failed: %s", esp_err_to_name(err));
        // Tear down the interface that was set up by esp_wireguard_init.
        esp_wireguard_disconnect(&s_wg_ctx);
        FreeBuffers();
        return;
    }

    started_ = true;
    ESP_LOGI(TAG, "WireGuard tunnel started (peer handshake in progress)");
}

void EasytierWg::Stop() {
    if (!started_) {
        return;
    }
    ESP_LOGI(TAG, "Stopping WireGuard tunnel");
    esp_wireguard_disconnect(&s_wg_ctx);
    started_ = false;
    FreeBuffers();
}
