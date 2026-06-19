# HavenB480 — IoT Hub Connection Flow

This document describes the complete connection flow from device boot to a fully established Azure IoT Hub MQTT session, including network selection (Ethernet vs Wi-Fi), all intermediate steps, error states, retry logic, and timeout handling.

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Boot Sequence](#2-boot-sequence)
3. [Server Connect State Machine](#3-server-connect-state-machine)
4. [State-by-State Detail](#4-state-by-state-detail)
5. [IoT Hub (MQTT/TLS) Connection](#5-iot-hub-mqtttls-connection)
6. [Connectivity Supervisor](#6-connectivity-supervisor)
7. [Connection Monitor (MonSrvConn)](#7-connection-monitor-monsrvconn)
8. [Error States Reference](#8-error-states-reference)
9. [Timeout Constants Reference](#9-timeout-constants-reference)
10. [LED Indicator States](#10-led-indicator-states)
11. [Flow Diagram](#11-flow-diagram)

---

## 1. Architecture Overview

The HavenB480 is an ESP32-based IoT device using a **FreeRTOS actor-based architecture**. Each subsystem (Wi-Fi, Ethernet, IoT Hub, NTP, BLE, etc.) runs as an independent "actor" — a FreeRTOS task with its own message queue. Actors communicate by sending JSON-formatted messages through a central console dispatcher.

**Key actors involved in the connection flow:**

| Actor | File | Role |
|-------|------|------|
| **Console** | `console_actor.c` | Central message dispatcher, boot orchestrator |
| **SYSTEM** | `SYSTEM_Actor.c` | Connection state machine orchestrator |
| **ETH** | `ETH_Actor.c` | W5500 SPI Ethernet driver (DHCP/static IP) |
| **WIFI** | `wifi_actor.c` | ESP32 Wi-Fi STA mode (scan, connect, reconnect) |
| **NTP** | `NTP_Actor.c` | Network time synchronization (required for TLS) |
| **IHUB** | `iHub_Actor.c` | Azure IoT Hub client (MQTT over TLS) |
| **HTTP** | `HTTP_Actor.c` | HTTP client for Device Announce REST call |
| **BLE** | `BLE_Actor.c` | Bluetooth LE for credential provisioning |

**Cloud services involved:**

- **Device Announce Server** — A custom REST endpoint that the device calls via HTTP POST to register itself and receive IoT Hub credentials (hostname, primary key).
- **Azure IoT Hub** — The MQTT broker the device connects to for telemetry (D2C), commands (C2D), device twin, and direct methods.

---

## 2. Boot Sequence

### 2.1 Hardware Initialization (`app_main`)

```
app_main()
  ├── Register deep-sleep wakeup timer (20 s)
  ├── Check reset reason (panic/WDT → deep sleep → reboot cycle)
  ├── Set custom cJSON memory hooks (PSRAM allocation)
  ├── Initialize NVS flash
  │     └── On NVS error → erase NVS → reinitialize
  ├── Initialize RTC (PCF8563 I2C clock, sets system time)
  ├── Initialize filesystem (FATFS for command history, if configured)
  ├── Initialize console (UART/USB, linenoise)
  ├── Initialize watchdog (Task WDT, 15 s timeout, triggers panic)
  └── Console_Initialize()
```

### 2.2 Actor Initialization (`Console_Initialize` → `InitActors` task)

`Console_Initialize()` creates an `InitActors` FreeRTOS task that brings up all subsystem actors in sequence:

```
InitActors task
  ├── Clean up temp filesystem directories
  ├── SPIFFS.INIT()        — Mount SPIFFS partition for config storage
  ├── WIFI.INIT()          — Initialize Wi-Fi driver (if Enable_WIFI flag set)
  │     ├── Create Wi-Fi event group
  │     ├── esp_wifi_init() with default config
  │     ├── Register event handlers (DISCONNECT, CONNECTED, GOT_IP)
  │     └── Set mode to WIFI_MODE_NULL (no auto-connect)
  ├── ETH.INIT()           — Initialize Ethernet (if Enable_ETH flag set)
  │     ├── esp_netif_init() — TCP/IP stack
  │     ├── Create default event loop
  │     ├── Configure W5500 SPI Ethernet (CS, INT, PHY reset GPIOs)
  │     ├── Install Ethernet driver
  │     ├── Register event handlers (CONNECTED, DISCONNECTED, GOT_IP)
  │     ├── Attach to TCP/IP stack
  │     ├── Start Ethernet driver state machine
  │     └── Start DHCP client
  ├── SYSTEM.INIT()        — Initialize system actor
  │     ├── Create timers (periodic connectivity supervisor)
  │     ├── Set defaults (AUTO_EXEC=1, Device_Announce_F_delay=0)
  │     ├── Generate DeviceId from MAC address
  │     ├── Read SYSTEM.json from JFS filesystem (credentials, URLs)
  │     └── Read device info from SPIFFS (FW version, HW version, etc.)
  ├── LED.INIT()
  ├── PUSHBUTTON.INIT()
  ├── BLE.INIT()
  ├── NTP.INIT()
  ├── vTaskDelay(15 s)     — Wait for network to come up before lighting init
  ├── EVENT_ACTOR.INIT()
  └── LIGHTING.INIT()
```

After initialization, the `SYSTEM.SERVER_CONNECT()` command is issued (either via auto-execute commands stored on the filesystem, a stack reset, or manually). This triggers the **Server Connect state machine**.

---

## 3. Server Connect State Machine

The Server Connect flow runs in a dedicated FreeRTOS task (`Server_Connect`) within the SYSTEM Actor. It implements a sequential state machine that walks through credential verification, network establishment, device registration, and IoT Hub connection.

### State Enum

```c
typedef enum {
    STATE_CHECK_CREDENTIALS,
    STATE_CHECK_BLE_START_RESP,
    STATE_CHECK_WIFI_ENABLED,
    STATE_CHECK_WIFI_AVAILABLE,
    STATE_NETWORK_SCAN,
    STATE_WIFI_CONNECT_SSID,
    STATE_ETH_CONNECT,
    STATE_NTP_SYNC_CHECK,
    STATE_DEVICE_ANNOUNCE,
    STATE_PING_DNS,
    STATE_PING_GATEWAY,
    STATE_FAILED_PING_GATEWAY,
    STATE_WAITING_FOR_IHUB_RESPONSE,
    STATE_CONNECTED,
    STATE_FAILED,
    STATE_FAILED_1  ... STATE_FAILED_13
} State;
```

### High-Level Flow

```
SERVER_CONNECT invoked
        │
        ▼
┌─────────────────────────┐
│ CHECK_CREDENTIALS       │ ◄─── All FAILED states loop back here
│ (API key + DA URL set?) │      after 30 s delay + stack reset
└─────────┬───────────────┘
          │
     ┌────┴──── Missing? ──────────────────┐
     │ Yes                                  │ No
     ▼                                      ▼
 Start BLE advertising              CHECK_WIFI_ENABLED
 Wait 5 min for BLE provisioning    ┌───────┴───────┐
 (STATE_CHECK_BLE_START_RESP)  Wi-Fi ON         Wi-Fi OFF
     │                              │               │
     │                              ▼               ▼
     │                        NETWORK_SCAN     ETH_CONNECT
     │                         ┌────┴────┐         │
     │                    SSID found  ETH only      │
     │                         │         │          │
     │                         ▼         ▼          │
     │                  WIFI_CONNECT  ETH_CONNECT ◄─┘
     │                         │         │
     │                    Connected?  Connected?
     │                     Y │  N      Y │  N
     │                       │  │        │  │
     │                       ▼  │        ▼  │
     │                  NTP_SYNC │   NTP_SYNC │
     │                       │  │        │   │
     │                       ▼  │        ▼   │
     │               DEVICE_ANNOUNCE    ├────┘
     │                       │          │
     │                  HTTP 200?   FAILED_4/5
     │                   Y │  N
     │                     │  │
     │                     ▼  ▼
     │            IHUB creds  PING_DNS
     │            populated?     │
     │             Y │  N    PING_GATEWAY
     │               │  │        │
     │               ▼  ▼        ▼
     │    WAIT_IHUB_RESP FAILED_13 FAILED_6/10
     │               │
     │         CONNECTED / DISCONNECTED?
     │          │              │
     │          ▼              ▼
     │     CONNECTED      FAILED_11
     │
     └─── FAILED states → 30 s delay → Stack Reset → back to CHECK_CREDENTIALS
```

---

## 4. State-by-State Detail

### 4.1 STATE_CHECK_CREDENTIALS

**Purpose:** Verify that the Device Announce URL and API key are stored on the device.

**Logic:**
- If `DeviceAnnounce_URL` or `APIkey` is empty:
  - Set credential status to 0 (missing) via BLE actor
  - On first attempt: start BLE advertising for provisioning
  - Set LED to 1-blink pattern
  - Wait up to **5 minutes** (`SER_CONN_BLE_MAX_DELAY = 300,000 ms`) for credentials via BLE
  - Transition → `STATE_CHECK_BLE_START_RESP`
  - On timeout: reset back to `STATE_CHECK_CREDENTIALS`
- If credentials present:
  - Set credential status to 1
  - Query console for `ENABLE_WIFI` property
  - Transition → `STATE_CHECK_WIFI_ENABLED`

### 4.2 STATE_CHECK_BLE_START_RESP

**Purpose:** Confirm BLE server started and handle the BLE provisioning timeout.

**Logic:**
- Receives BLE start confirmation ("Server is started..")
- Reloads 5-minute timeout
- On timeout without credentials: transitions to appropriate FAILED state

### 4.3 STATE_CHECK_WIFI_ENABLED

**Purpose:** Determine whether to use Wi-Fi or Ethernet-only path.

**Logic:**
- Reads `ENABLE_WIFI` property from console
- If Wi-Fi enabled (`ENABLE_WIFI == 1`):
  - Send `WIFI.NETWORK_SCAN()` command
  - Transition → `STATE_NETWORK_SCAN`
- If Wi-Fi disabled (`ENABLE_WIFI == 0`):
  - Query Ethernet status
  - Transition → `STATE_ETH_CONNECT`
- **Timeout:** 3 seconds (`WIFI_ENABLED_TIMEOUT_SECONDS`). If no response, return to `STATE_CHECK_CREDENTIALS`.

### 4.4 STATE_NETWORK_SCAN

**Purpose:** Scan for available networks and decide Wi-Fi vs Ethernet path.

**Logic:**
- WIFI Actor performs a scan and returns results with SSID, PASSWORD, and ETHERNET availability
- If a matching **SSID is found** in scan results:
  - Send `WIFI.CONNECT_SSID()` with SSID + password
  - Transition → `STATE_WIFI_CONNECT_SSID`
- If **no SSID but Ethernet is AVAILABLE**:
  - Send `ETH.CONNECT()` command
  - Transition → `STATE_ETH_CONNECT`
- If **NETWORK NOT AVAILABLE** (no SSID, no Ethernet):
  - On first occurrence: start BLE advertising
  - Set LED to 2-blink pattern
  - Wait 5 minutes for network credentials via BLE
  - Transition → `STATE_CHECK_BLE_START_RESP`
- **Timeout:** 3 seconds for default case. On timeout with no networks:
  - Start BLE advertising for credential entry
  - Set connection state to "WAITING FOR WIFI"

### 4.5 STATE_WIFI_CONNECT_SSID

**Purpose:** Connect to the selected Wi-Fi SSID.

**Logic:**
- Waits for `WIFI_CONNECTION_STATUS` response from WIFI actor
- If `CONNECTED`:
  - Send `NTP.CONNECT()` for time sync
  - Set LED to "NET CONNECTED"
  - Transition → `STATE_NTP_SYNC_CHECK`
- If `DISCONNECTED`:
  - Start BLE advertising for new credentials
  - Set LED to 3-blink pattern
  - Set connection state to "BAD WIFI PASSWORD"
  - Wait 5 minutes for credentials via BLE
  - Transition → `STATE_CHECK_BLE_START_RESP`
- **Timeout:** 60 seconds (`WIFI_CONNECT_TIMEOUT_SECONDS`). On timeout: transition → `STATE_FAILED_3`.

### 4.6 STATE_ETH_CONNECT

**Purpose:** Verify Ethernet link is up and has an IP address.

**Logic:**
- Checks `ETH_STATUS` from the Ethernet actor
- If `ETH_STATUS == 1` (connected):
  - Send `NTP.CONNECT()`
  - Set LED to "NET CONNECTED"
  - Transition → `STATE_NTP_SYNC_CHECK`
- If `ETH_STATUS != 1` (not connected):
  - Set LED to 5-blink pattern
  - Set connection state to "WAITING FOR ETH"
  - If Wi-Fi is disabled: wait **10 minutes** before retrying
  - Transition → `STATE_FAILED_4`
- If no response from ETH actor:
  - Transition → `STATE_FAILED_5`
- **Timeout:** 3 seconds (`ETHERNET_TIMEOUT_SECONDS`)

### Ethernet Connection Details (ETH_Actor)

The Ethernet actor uses an SPI-connected **W5500** chip:

1. Initialize TCP/IP network interface (`esp_netif_init`)
2. Create default event loop
3. Configure SPI bus (MISO, MOSI, SCLK, CS, INT pins)
4. Initialize W5500 MAC and PHY drivers
5. Install Ethernet driver
6. Set MAC address from ESP32's fused MAC
7. Register event handlers for link status and IP acquisition
8. Attach to TCP/IP stack
9. Start Ethernet driver state machine
10. Start DHCP client

**Event-driven connection:**
- `ETHERNET_EVENT_CONNECTED` → updates status, reads MAC, logs audit
- `IP_EVENT_ETH_GOT_IP` → stores IP/SM/GW/DNS, notifies system, sets Wi-Fi scan to passive mode
- `ETHERNET_EVENT_DISCONNECTED` → updates status, sets Wi-Fi scan back to active mode

**DHCP vs Static IP:**
- DHCP enabled (default): `esp_netif_dhcpc_start()`
- DHCP disabled: configure static IP, subnet, gateway, DNS manually via `esp_netif_set_ip_info()` and `esp_netif_set_dns_info()`

### 4.7 STATE_NTP_SYNC_CHECK

**Purpose:** Ensure system clock is synchronized before making TLS connections (certificates require correct time).

**Logic:**
- Sends `NTP.CONNECT()` command to NTP actor
- Applies Device Announce failure delay (exponential backoff):
  - Initial delay: `Device_Announce_F_delay` (starts at 0, doubles each failure)
  - Capped at 100,000 seconds (~1.16 days); if exceeded, reset to 10 seconds
  - If delay > 300 seconds, a 5-minute periodic timer is started
- Transition → `STATE_DEVICE_ANNOUNCE`

### 4.8 STATE_DEVICE_ANNOUNCE

**Purpose:** Register the device with the custom Device Announce server via HTTP POST to obtain IoT Hub credentials.

**Logic:**
- SYSTEM actor sends a `DEVICE_ANN` command to itself, which triggers `httpDeviceAnnounceInit()`
- This sends an HTTP POST to the `DEVICE_ANNOUNCE_URL` with the API key and device information
- The server responds with a list of "methods" (commands) to execute, including IHUB credential SET commands

**On HTTP 200 (success):**
- Parse the response for IHUB credentials:
  - `IHUB.SET({"HOSTNAME": "..."})` — IoT Hub hostname
  - `IHUB.SET({"PRIMARY_KEY": "..."})` — Device symmetric key
- If **both** HOSTNAME and PRIMARY_KEY are present:
  - Set connection state to "WAITING FOR IHUB"
  - Transition → `STATE_WAITING_FOR_IHUB_RESPONSE`
- If credentials are **missing or partial** (0 or 1 of 2):
  - Set LED to 5-blink (iHUB category)
  - Transition → `STATE_FAILED_13`

**On HTTP error:**
- Specific error handling by status code:
  - `400` — Bad Request
  - `401` — Wrong API key (LED: 6-blink)
  - `404` — Server not found
  - `405` — Invalid URL (LED: 6-blink)
  - `408` — Server timeout (LED: 5-blink)
  - Other — Generic refusal (LED: 7-blink)
- Send a DNS ping to `www.google.com` to diagnose connectivity
- Transition → `STATE_PING_DNS`

**Timeout:** 51 seconds (`DEVICE_ANNOUNCE_TIMEOUT_SECONDS`). On timeout:
- Set connection state to "WAITING FOR SERVER"
- Set LED to 6-blink (B480) or 7-blink (other variants)
- Send DNS ping to diagnose
- Transition → `STATE_PING_DNS`

### 4.9 STATE_PING_DNS

**Purpose:** After a Device Announce failure, verify internet connectivity by pinging `www.google.com`.

**Logic:**
- Sends PING command to the active network actor (WIFI or ETH) with 1000 ms timeout
- If **ping successful**:
  - Internet is reachable but Device Announce server is not responding
  - Transition → `STATE_FAILED_6` (DNS ping OK but DA failed)
- If **ping failed**:
  - Internet is unreachable
  - Set LED to 4-blink
  - Increment `InternetDisconnectCount`
  - Log to audit
  - Set connection state to "WAITING FOR INTERNET"
  - Retry by sending another ping to gateway
  - Transition → `STATE_PING_GATEWAY`
- **Timeout:** 40 seconds (`PING_DNS_TIMEOUT_SECONDS`). On timeout: treat as ping failed, transition → `STATE_PING_GATEWAY`.

### 4.10 STATE_PING_GATEWAY

**Purpose:** Determine if the local network gateway is reachable (to differentiate LAN issues from WAN issues).

**Logic:**
- Sends PING command to `www.google.com` via the active network interface
- If **ping successful**:
  - LAN and WAN work, but Device Announce server failed
  - Transition → `STATE_FAILED_10` (ping OK but DA failed)
- If **ping failed**:
  - Network connectivity is broken
  - Set LED to 4-blink
  - Transition → `STATE_FAILED_PING_GATEWAY`
- **Timeout:** 40 seconds (`PING_GATEWAY_TIMEOUT_SECONDS`). On timeout: treat as ping failed.

### 4.11 STATE_WAITING_FOR_IHUB_RESPONSE

**Purpose:** Wait for the IoT Hub connection attempt to complete (IHUB actor runs its own connection task).

**Logic:**
- Listens for `IHUBRESP` message from the SYSTEM actor's response handler (which bridges IHUB events to the Server Connect queue)
- If `IHUBRESP == "CONNECTED"`:
  - Transition → `STATE_CONNECTED`
- If `IHUBRESP == "DISCONNECTED"`:
  - If MonSrvConn (connection monitor) is not running:
    - Transition → `STATE_FAILED_11`
    - LED indication varies by error number:
      - Error 1: LED 6-blink (iHUB)
      - Error 6 or 12: LED 5-blink (iHUB)
      - Other: LED 7-blink (iHUB)
  - If MonSrvConn is running: treat as `STATE_CONNECTED` (monitor will handle reconnection)
- **Timeout:** 60 seconds (`IHUB_RESP_TIMEOUT_SECONDS`). On timeout:
  - Transition → `STATE_FAILED_12` (no IHUB response)
  - LED: 7-blink (iHUB)

### 4.12 STATE_CONNECTED

**Purpose:** Terminal success state. The device is fully connected.

- Queue receive delay set to 1 minute
- Server Connect task exits
- Connection monitor (`MonSrvConn`) takes over for ongoing health monitoring

### 4.13 All FAILED States

All failed states are handled uniformly:

```
STATE_FAILED / STATE_FAILED_1 ... STATE_FAILED_13 / STATE_FAILED_PING_GATEWAY
    │
    ▼
handle_failed_state():
    ├── Log: "Server Connect flow failed. Stack will reset after 30 seconds"
    ├── vTaskDelay(30 seconds)
    ├── Reset_Stack():
    │     ├── Deinit HTTP actor
    │     ├── Deinit WIFI actor
    │     ├── Deinit IHUB actor
    │     ├── vTaskDelay(500 ms)
    │     ├── Reinit IHUB actor
    │     ├── vTaskDelay(2 s)
    │     └── Send SYSTEM.SERVER_CONNECT() — restart the entire flow
    └── state = STATE_CHECK_CREDENTIALS (queue delay = 1 minute)
```

**Failed state meanings:**

| State | Description |
|-------|-------------|
| `FAILED_1` | Network not available (no Wi-Fi SSID, no Ethernet) |
| `FAILED_2` | Invalid JSON input in Server Connect queue |
| `FAILED_3` | Wi-Fi connection failed (bad password or timeout) |
| `FAILED_4` | Ethernet connection failed |
| `FAILED_5` | No response from Ethernet actor |
| `FAILED_6` | DNS ping successful, but Device Announce failed |
| `FAILED_10` | Both DNS and gateway pings successful, but Device Announce failed |
| `FAILED_11` | IoT Hub connection returned DISCONNECTED |
| `FAILED_12` | No response from IoT Hub actor within 60 s |
| `FAILED_13` | IoT Hub credentials missing/partial after Device Announce |
| `FAILED_PING_GATEWAY` | Gateway ping failed (LAN connectivity issue) |

---

## 5. IoT Hub (MQTT/TLS) Connection

The IoT Hub connection is managed by the `prvAzureDemoTask` in `iHub_Actor.c`. It runs as a separate FreeRTOS task created when the IHUB actor receives a `CONNECT` command.

### 5.1 Pre-Connection Setup

1. **Get Device ID** from SYSTEM actor
2. **Initialize Azure IoT Middleware** (`AzureIoT_Init()`)
3. **Setup network credentials** (`prvSetupNetworkCredentials`):
   - Root CA certificate (`democonfigROOT_CA_PEM`)
   - Device symmetric key or client certificate
4. **Verify internet connectivity** (`xAzureSample_IsConnectedToInternet()`)

### 5.2 TLS Connection with Exponential Backoff

```c
prvConnectToServerWithBackoffRetries()
```

**Parameters:**
- **Max attempts:** 5 (`sampleazureiotRETRY_MAX_ATTEMPTS`)
- **Base backoff:** 500 ms (`sampleazureiotRETRY_BACKOFF_BASE_MS`)
- **Max backoff:** 5,000 ms (`sampleazureiotRETRY_MAX_BACKOFF_DELAY_MS`)
- **Transport timeout:** 5,000 ms send/receive (`sampleazureiotTRANSPORT_SEND_RECV_TIMEOUT_MS`)

**Flow:**

```
Attempt 1: TLS_Socket_Connect(hostname, 8883, credentials, 5s, 5s)
    ├── Success → return 0
    └── Failure → calculate backoff (500ms base, random jitter)
        ├── BackoffAlgorithmSuccess → vTaskDelay(backoff_ms) → retry
        └── BackoffAlgorithmRetriesExhausted → return 1 (all 5 attempts failed)

Attempt 2: TLS_Socket_Connect(...)
    ├── ...backoff ~500-1000ms...
Attempt 3: ...backoff ~1000-2500ms...
Attempt 4: ...backoff ~2000-5000ms...
Attempt 5: ...backoff ~2000-5000ms (capped)...
    └── All exhausted → return 1
```

### 5.3 MQTT Connection Sequence

After successful TLS connection:

```
1. AzureIoTHubClient_OptionsInit()
   └── Failure → log error, set DISCONNECTED, exit task

2. AzureIoTHubClient_Init(hostname, deviceId, options, buffer, transport)
   └── Failure → log error, exit task

3. AzureIoTHubClient_SetSymmetricKey(primaryKey, HMAC)
   └── Failure → log error, exit task

4. AzureIoTHubClient_Connect(cleanSession=true, CONNACK timeout=10s)
   ├── Session present bit set → log warning (stale session)
   ├── Success:
   │     ├── Set status → IHUB_CONNECTED
   │     ├── Update BLE online status
   │     ├── Restart connectivity supervisor timer
   │     └── Proceed to subscriptions
   └── Failure → set IHUB_NOT_CONNECTED, exit task

5. AzureIoTHubClient_SubscribeCommand(timeout=10s)
   └── Failure → log, exit task

6. AzureIoTHubClient_SubscribeProperties(timeout=10s)
   └── Failure → log, exit task

7. AzureIoTHubClient_RequestPropertiesAsync()
   └── Failure → log, exit task

8. AzureIoTHubClient_SubscribeCloudToDeviceMessage(timeout=10s)
   └── Failure → log, exit task
```

### 5.4 Connected Operation Loop

Once connected, the task enters a continuous loop:

```
for (; xAzureSample_IsConnectedToInternet(); ) {
    // D2C Heartbeat (every D2C_INTERVAL_SECOND, default 10 min)
    if (time_since_last_D2C >= D2C_interval) {
        ulCreateTelemetry() → AzureIoTHubClient_SendTelemetry()
    }

    // Process MQTT keep-alive and incoming messages
    AzureIoTHubClient_ProcessLoop(timeout = 2s or 60s)

    // Check for stale connection
    if (Azure_MQTTContextIsStale()) {
        → Force disconnect, set DISCONNECTED, restart
    }

    // Check D2C message queue for outbound messages
    // Check Twin update queue for property updates

    // Handle deinit request
    if (deinit_Flag) → graceful disconnect
}
```

**MQTT Process Loop Timeout:**
- B394 variant: 2,000 ms (faster response for digital input events)
- Other variants: 60,000 ms

**Stale Connection Detection (`Azure_MQTTContextIsStale`):**
- If `waitingForPingResp` and time since ping > `keepAliveIntervalSec * 1000ms` → stale
- If time since last packet > `keepAliveIntervalSec * 3000ms` → stale

### 5.5 IHUB Connection Status Events

The IHUB actor sends connection status events back to the SYSTEM actor via the console dispatcher:

```json
{"IHUB_CONNECTION_STATUS": "CONNECTED", "ERROR_NUM": 0}
{"IHUB_CONNECTION_STATUS": "DISCONNECTED", "ERROR_NUM": <error_code>}
```

The SYSTEM actor's `Analyse_Response` handler bridges these events to the `ServerConnect_Que` as `IHUBRESP` messages for the Server Connect state machine.

---

## 6. Connectivity Supervisor

A periodic ESP timer runs every **10 seconds** (`CONNECTIVITY_SUPERVISOR_INTERVAL_US`) to monitor connection health and trigger device reset if recovery fails.

### 6.1 Parameters

| Parameter | Value | Description |
|-----------|-------|-------------|
| `CONNECTIVITY_BOOT_GRACE_US` | 8 minutes | Grace period after boot before supervisor arms |
| `CONNECTIVITY_DISCONNECT_TIMEOUT_US` | 20 minutes | Max time in disconnected state before reset |
| `CONNECTIVITY_SUPERVISOR_INTERVAL_US` | 10 seconds | Check interval |
| `CONNECTIVITY_ACTIVITY_TIMEOUT_US` | 5 minutes | Max time without successful MQTT activity |

### 6.2 Logic (runs every 10 seconds)

```
periodic_timer_callback():
    ├── Record boot time (first call only)
    ├── If within 8-minute boot grace period → skip (not armed)
    ├── Check health:
    │     healthy = (IHUB connected) AND
    │               (time since last activity ≤ 5 minutes)
    ├── If healthy:
    │     ├── Update last_successful_activity timestamp
    │     ├── Reset disconnect_start timer
    │     └── Return
    ├── If unhealthy:
    │     ├── Start disconnect timer (if not started)
    │     └── If disconnected for ≥ 20 minutes:
    │           ├── Stop timer
    │           └── Restart_ESP_Xface(1) → device reboot
    └── Return
```

**Restart sequence:**
1. Set LED to REBOOT_MODE
2. Unmount JFS filesystem
3. Unmount SPIFFS volumes
4. Wait 1 second
5. Disable all watchdogs
6. Reconfigure watchdog with 200 ms timeout
7. Spin in infinite loop → watchdog triggers system reset

---

## 7. Connection Monitor (MonSrvConn)

After the initial `Server_Connect` task succeeds (reaches `STATE_CONNECTED`), the connection is handed off to `MonSrvConn` — a persistent monitoring task that watches for disconnections and triggers reconnection.

### Monitored conditions:
- Wi-Fi connection status
- Ethernet connection status
- IHUB connection status
- Network scan results (for fallback SSIDs)

### Recovery actions:
- On Wi-Fi disconnect: trigger stack reset
- On IHUB disconnect: trigger stack reset
- On network unavailable: wait and retry with rescue SSID

### Stack Reset (`Reset_Stack`):
1. Deinit HTTP, WIFI, and IHUB actors
2. Wait 500 ms
3. Reinitialize IHUB actor
4. Wait 2 seconds
5. Issue `SYSTEM.SERVER_CONNECT()` to restart the full flow

---

## 8. Error States Reference

| Error State | Meaning | Recovery |
|-------------|---------|----------|
| `STATE_FAILED_1` | Network not available | 30s delay → stack reset → `CHECK_CREDENTIALS` |
| `STATE_FAILED_2` | Invalid JSON in Server Connect queue | 30s delay → stack reset → `CHECK_CREDENTIALS` |
| `STATE_FAILED_3` | Wi-Fi connection failed or timed out (60s) | 30s delay → stack reset → `CHECK_CREDENTIALS` |
| `STATE_FAILED_4` | Ethernet connection failed | If Wi-Fi disabled: wait 10 min first. 30s delay → stack reset |
| `STATE_FAILED_5` | No response from ETH actor (3s timeout) | 30s delay → stack reset → `CHECK_CREDENTIALS` |
| `STATE_FAILED_6` | DNS ping OK but Device Announce failed | 30s delay → stack reset → `CHECK_CREDENTIALS` |
| `STATE_FAILED_10` | DNS+GW ping OK but Device Announce failed | 30s delay → stack reset → `CHECK_CREDENTIALS` |
| `STATE_FAILED_11` | IHUB connection returned DISCONNECTED | 30s delay → stack reset → `CHECK_CREDENTIALS` |
| `STATE_FAILED_12` | No IHUB response within 60s | 30s delay → stack reset → `CHECK_CREDENTIALS` |
| `STATE_FAILED_13` | IHUB credentials missing/partial from DA | 30s delay → stack reset → `CHECK_CREDENTIALS` |
| `STATE_FAILED_PING_GATEWAY` | Gateway ping failed (LAN issue) | 30s delay → stack reset → `CHECK_CREDENTIALS` |

---

## 9. Timeout Constants Reference

| Constant | Value | Used In |
|----------|-------|---------|
| `SER_CONN_BLE_MAX_DELAY` | 5 min (300,000 ms) | BLE credential provisioning wait |
| `SER_CONN_NORMAL_DELAY` | 3 s (3,000 ms) | Normal queue receive delay |
| `WIFI_ENABLED_TIMEOUT_SECONDS` | 3 s | Waiting for Wi-Fi enabled status |
| `WIFI_CONNECT_TIMEOUT_SECONDS` | 60 s | Waiting for Wi-Fi connection |
| `ETHERNET_TIMEOUT_SECONDS` | 3 s | Waiting for Ethernet connection |
| `NETWORK_SCAN_TIMEOUT_SECONDS` | 3 s | Waiting for network scan results |
| `DEVICE_ANNOUNCE_TIMEOUT_SECONDS` | 51 s | Waiting for HTTP response from DA server |
| `PING_DNS_TIMEOUT_SECONDS` | 40 s | Waiting for DNS ping response |
| `PING_GATEWAY_TIMEOUT_SECONDS` | 40 s | Waiting for gateway ping response |
| `IHUB_RESP_TIMEOUT_SECONDS` | 60 s | Waiting for IoT Hub connection result |
| `sampleazureiotCONNACK_RECV_TIMEOUT_MS` | 10 s | MQTT CONNACK response timeout |
| `sampleazureiotSUBSCRIBE_TIMEOUT` | 10 s | MQTT subscribe acknowledgment timeout |
| `sampleazureiotTRANSPORT_SEND_RECV_TIMEOUT_MS` | 5 s | TLS socket send/receive timeout |
| `sampleazureiotRETRY_MAX_ATTEMPTS` | 5 | TLS connection retry attempts |
| `sampleazureiotRETRY_BACKOFF_BASE_MS` | 500 ms | Exponential backoff base |
| `sampleazureiotRETRY_MAX_BACKOFF_DELAY_MS` | 5,000 ms | Exponential backoff ceiling |
| `sampleazureiotPROCESS_LOOP_TIMEOUT_MS` | 2 s (B394) / 60 s (others) | MQTT process loop |
| `D2C_INTERVAL_SECONDS` | 10 min (600 s) | Telemetry heartbeat interval |
| `CONNECTIVITY_BOOT_GRACE_US` | 8 min | Supervisor grace period after boot |
| `CONNECTIVITY_DISCONNECT_TIMEOUT_US` | 20 min | Max disconnect before device reset |
| `CONNECTIVITY_ACTIVITY_TIMEOUT_US` | 5 min | Max idle before considered unhealthy |
| `CONNECTIVITY_SUPERVISOR_INTERVAL_US` | 10 s | Health check timer period |
| `WATCHDOG_TIMEOUT_MS` | 15 s | FreeRTOS task watchdog timeout |

---

## 10. LED Indicator States

| LED Pattern | Category | Meaning |
|-------------|----------|---------|
| 1-blink | none | Missing API key / Device Announce URL — waiting for BLE provisioning |
| 2-blink | none | Network not available — waiting for network credentials via BLE |
| 3-blink | none | Wi-Fi connection failed — waiting for new credentials via BLE |
| 4-blink | none | DNS/gateway ping failed — no internet connectivity |
| 5-blink | Server | Device Announce server timeout (HTTP 408) |
| 5-blink | iHUB | IHUB credentials missing/partial, or IHUB error 6/12 |
| 6-blink | Server | Device Announce auth failure (HTTP 401/405) |
| 6-blink | iHUB | IHUB connection error 1 |
| 7-blink | Server | Device Announce other error |
| 7-blink | iHUB | IHUB connection general failure / no response |
| NET CONNECTED | none | Network (Wi-Fi or Ethernet) connected successfully |
| REBOOT_MODE | none | Device is rebooting |

---

## 11. Flow Diagram

```
╔══════════════════════════════════════════════════════════════════╗
║                        DEVICE BOOT                              ║
╚══════════════════════════════════╦═══════════════════════════════╝
                                   ║
                    ┌──────────────▼──────────────┐
                    │   Hardware Init (app_main)   │
                    │  NVS, RTC, Console, Watchdog │
                    └──────────────┬──────────────┘
                                   │
                    ┌──────────────▼──────────────┐
                    │     InitActors Task          │
                    │  SPIFFS → WIFI → ETH →       │
                    │  SYSTEM → LED → BLE → NTP    │
                    └──────────────┬──────────────┘
                                   │
               ┌───────────────────▼───────────────────┐
               │  Ethernet Init (runs in background)   │
               │  W5500 SPI → DHCP → got_ip event      │
               └───────────────────┬───────────────────┘
                                   │
         ┌─────────────────────────▼─────────────────────────┐
         │            SYSTEM.SERVER_CONNECT()                 │
         │         (triggered after initialization)           │
         └─────────────────────────┬─────────────────────────┘
                                   │
         ┌─────────────────────────▼─────────────────────────┐
         │  ┌─────────────────────────────────────────────┐  │
         │  │         STATE: CHECK_CREDENTIALS             │  │
         │  │  API Key + Device Announce URL present?      │  │
         │  └──────────────┬──────────────┬───────────────┘  │
         │            NO   │              │  YES              │
         │    ┌────────────▼────────┐     │                   │
         │    │ Start BLE Advertising│    │                   │
         │    │ Wait 5 min for creds │    │                   │
         │    │ LED: 1-blink         │    │                   │
         │    └────────────┬────────┘     │                   │
         │          Timeout│/ Got creds   │                   │
         │                 └──────────────┤                   │
         │                                │                   │
         │  ┌─────────────────────────────▼───────────────┐  │
         │  │         STATE: CHECK_WIFI_ENABLED            │  │
         │  │  Is Wi-Fi enabled in configuration?          │  │
         │  └──────────┬──────────────────┬───────────────┘  │
         │        YES  │                  │  NO               │
         │  ┌──────────▼──────────┐  ┌────▼────────────────┐ │
         │  │  STATE: NETWORK_SCAN│  │ STATE: ETH_CONNECT  │ │
         │  │  Scan Wi-Fi networks│  │ Check ETH_STATUS    │ │
         │  └─────┬─────────┬────┘  └──┬──────────────┬───┘ │
         │   SSID │    ETH  │     OK   │         FAIL │     │
         │   found│    only │          │              │     │
         │  ┌─────▼─────┐   │   ┌──────▼──────┐      │     │
         │  │WIFI_CONNECT│  │   │  NTP_SYNC    │      │     │
         │  │  SSID      │  └──►│  CHECK       │      │     │
         │  └──┬─────┬──┘      └──────┬───────┘      │     │
         │  OK │  FAIL│               │               │     │
         │     │     │          ┌─────▼──────────┐    │     │
         │     │     │          │DEVICE_ANNOUNCE  │    │     │
         │     │     │          │HTTP POST to DA  │    │     │
         │     │     │          │server           │    │     │
         │     │     │          └──┬──────────┬──┘    │     │
         │     │     │      200 OK│      Error│       │     │
         │     │     │            │           │       │     │
         │     │     │   ┌────────▼────┐  ┌───▼─────┐│     │
         │     │     │   │ IHUB creds  │  │PING_DNS ││     │
         │     │     │   │ populated?  │  │  (diag) ││     │
         │     │     │   └──┬──────┬──┘  └───┬─────┘│     │
         │     │     │  YES │   NO │         │      │     │
         │     │     │      │      │    ┌────▼────┐ │     │
         │     │     │      │      │    │PING_GW  │ │     │
         │     │     │      │      │    │ (diag)  │ │     │
         │     │     │      │      │    └────┬────┘ │     │
         │  ┌──▼─────▼──┐   │      │         │      │     │
         │  │WAIT_IHUB   │   │      │         │      │     │
         │  │RESPONSE    │   │      │         │      │     │
         │  │(60s timeout)│  │      │         │      │     │
         │  └──┬──────┬──┘   │      │         │      │     │
         │  OK │  FAIL│      │      │         │      │     │
         │     │      │      │      │         │      │     │
         │  ┌──▼──┐   │      │      │         │      │     │
         │  │CONN-│   │      │      │         │      │     │
         │  │ECTED│   │      │      │         │      │     │
         │  └─────┘   │      │      │         │      │     │
         │             │      │      │         │      │     │
         │      ┌──────▼──────▼──────▼─────────▼──────▼┐   │
         │      │          FAILED STATE                 │   │
         │      │  30s delay → Reset_Stack()            │   │
         │      │  Deinit HTTP/WIFI/IHUB → Reinit       │   │
         │      │  → Restart SERVER_CONNECT              │   │
         │      └───────────────────┬───────────────────┘   │
         │                          │                        │
         │                   (loops back to                  │
         │                    CHECK_CREDENTIALS)             │
         └───────────────────────────────────────────────────┘

══════════════════════════════════════════════════════════════════
         AFTER STATE_CONNECTED: Ongoing Monitoring
══════════════════════════════════════════════════════════════════

  ┌─────────────────────────────────────────────────────────┐
  │              Connectivity Supervisor Timer               │
  │              (every 10 seconds)                          │
  │                                                         │
  │  8-min boot grace → then:                               │
  │  If IHUB connected + active within 5 min → healthy      │
  │  If unhealthy for 20 min → DEVICE RESET                 │
  └─────────────────────────────────────────────────────────┘

  ┌─────────────────────────────────────────────────────────┐
  │              MonSrvConn Task                             │
  │              (persistent monitoring)                     │
  │                                                         │
  │  Monitors WIFI/ETH/IHUB status                          │
  │  On disconnect → Stack Reset → SERVER_CONNECT restart   │
  └─────────────────────────────────────────────────────────┘

  ┌─────────────────────────────────────────────────────────┐
  │              IHUB Connected Loop                         │
  │              (prvAzureDemoTask)                          │
  │                                                         │
  │  MQTT Process Loop (2s/60s) for keep-alive              │
  │  D2C telemetry heartbeat (every 10 min)                 │
  │  Twin property updates                                  │
  │  Command/C2D message handling                           │
  │  Stale connection detection → force reconnect           │
  └─────────────────────────────────────────────────────────┘

  ┌─────────────────────────────────────────────────────────┐
  │              Task Watchdog (15s)                          │
  │  Monitors FreeRTOS idle tasks on both cores             │
  │  On timeout → panic → deep sleep → reboot              │
  └─────────────────────────────────────────────────────────┘
```
