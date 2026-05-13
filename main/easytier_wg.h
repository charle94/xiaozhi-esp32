#ifndef EASYTIER_WG_H
#define EASYTIER_WG_H

#include <string>
#include <esp_err.h>

/**
 * EasytierWg — WireGuard client that connects this device to an EasyTier
 * virtual network.
 *
 * Usage:
 *   1. Call EasytierWg::GetInstance().Start() when the underlying network
 *      (Wi-Fi / LTE) becomes available.
 *   2. Call EasytierWg::GetInstance().Stop() when the network goes down.
 *
 * Configuration is read from NVS (namespace "easytier") at Start() time.
 * Compile-time defaults can be baked in via sdkconfig (EASYTIER_WG_* options).
 * The MCP tools registered in mcp_server.cc allow runtime reconfiguration.
 */
class EasytierWg {
public:
    static EasytierWg& GetInstance();

    EasytierWg(const EasytierWg&) = delete;
    EasytierWg& operator=(const EasytierWg&) = delete;

    /**
     * Start the WireGuard tunnel.
     *
     * Reads configuration from NVS.  Falls back to compile-time defaults for
     * any key that is not stored in NVS.  Does nothing (and logs a warning)
     * when the VPN is disabled or when required fields (private_key,
     * peer_public_key, endpoint) are empty.
     *
     * Safe to call from any task.
     */
    void Start();

    /**
     * Disconnect and tear down the WireGuard tunnel.
     * Safe to call even when the tunnel was never started.
     */
    void Stop();

    /** Returns true when the WireGuard tunnel is currently active. */
    bool IsConnected() const;

    /** Returns true when the VPN is enabled in NVS / sdkconfig. */
    bool IsEnabled() const;

    /** Returns the configured virtual IP, or an empty string if not set. */
    std::string GetVpnIpAddress() const;

private:
    EasytierWg() = default;
    ~EasytierWg();

    bool started_ = false;

    // Heap-allocated strings kept alive for the lifetime of the WireGuard
    // context (the underlying C library holds raw char* pointers).
    char* private_key_buf_    = nullptr;
    char* peer_pubkey_buf_    = nullptr;
    char* preshared_key_buf_  = nullptr;
    char* endpoint_buf_       = nullptr;
    char* allowed_ip_buf_     = nullptr;
    char* allowed_ip_mask_buf_= nullptr;

    void FreeBuffers();
};

#endif // EASYTIER_WG_H
