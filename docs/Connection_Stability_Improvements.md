# HavenB480 — Connection Stability & Recovery Improvement Strategy

This document identifies design weaknesses in the current IoT Hub connection flow and proposes concrete improvements. Each finding is backed by specific code references and includes a severity rating, estimated impact, and implementation guidance.

Priorities are ordered by impact on **preventing failures from occurring**, then **initial connection speed**, then **long-term resilience**.

---

## Executive Summary

### Design Philosophy: Full Reset on Failure

The current recovery architecture follows a deliberate, conservative design: **every failure triggers a full stack reset** — deinit of HTTP, Wi-Fi, and IHUB actors, followed by a complete restart of the connection state machine from scratch. This approach is the preferred strategy for the following reasons:

- **Reduced risk:** A clean slate on every retry eliminates entire classes of bugs caused by partially-initialized state, stale socket handles, or leaked resources.
- **Reduced testing surface:** One recovery path means one path to test, rather than a combinatorial explosion of partial-recovery scenarios.
- **Predictable behavior:** Field diagnostics can always assume the device goes through the same sequence, making logs easier to interpret.

Recovery time on failure is **not a primary concern**. The priority is that the device reliably reaches a connected state, even if each attempt takes longer.

### Where Improvements Are Needed

Given this philosophy, the improvements in this document focus on three areas:

| Tier | Theme | Goal |
|------|-------|------|
| **Tier 1** | Prevent unnecessary failures | Avoid entering failure states when success was possible |
| **Tier 2** | Speed up the happy path | Reach IoT Hub faster on clean boot and after reset |
| **Tier 3** | Structural resilience | Prevent infinite loops and improve diagnostics |

---

## Design Decision: Full Stack Reset on All Failures (Retained)

The current architecture routes every `STATE_FAILED_*` state through `handle_failed_state()`, which unconditionally waits 30 seconds, calls `Reset_Stack()` (deinit HTTP, Wi-Fi, IHUB actors), and restarts the entire `SERVER_CONNECT` state machine from `STATE_CHECK_CREDENTIALS`.

**This design is intentionally retained.** While tiered or partial recovery could reduce per-cycle recovery time, it would introduce significant complexity:

- Multiple recovery paths to test and validate in the field.
- Risk of partial state corruption (e.g., a Wi-Fi driver with a stale socket handle surviving a reset).
- Harder-to-diagnose field failures when different error types follow different paths.

The full-reset approach guarantees a clean slate on every retry. Recovery time sensitivity is low — **reliability and predictability are the priorities.** Each retry starts from identical initial conditions, eliminating an entire class of stateful bugs.

**Code reference:** `SYSTEM_Actor.c` lines 3432-3437 (`handle_failed_state`) and 3628-3654 (`Reset_Stack`).

The remaining findings in this document focus on improvements that work **within** this full-reset philosophy: preventing unnecessary failures, speeding up each attempt, and improving long-term resilience.

---

## Finding 1: Network Selection Doesn't Try Both Interfaces Per Cycle (High)

### Problem

The state machine treats Wi-Fi and Ethernet as mutually exclusive paths chosen once during `STATE_NETWORK_SCAN`. If Wi-Fi is selected and then fails at connect or Device Announce, the system never tries Ethernet within the same attempt — it goes through PING_DNS → PING_GATEWAY → FAILED → full reset → restart from scratch. On the next cycle, depending on scan results, it may select the same failing interface again.

This means the device can repeatedly fail on one interface while the other is perfectly healthy, burning full reset cycles each time.

**Code reference:** `handle_network_scan()` in `SYSTEM_Actor.c` lines 2798-2918. The function picks either WIFI or ETH but never both.

### Proposed Solution: Try Both Interfaces Before Declaring Failure

Within a single connection attempt, try both available interfaces before entering a FAILED state and triggering the full reset. This keeps the full-reset-on-failure guarantee intact — if *both* interfaces fail, the device still does a complete reset as today.

1. **At `STATE_NETWORK_SCAN`:** Record which interfaces are available (Wi-Fi SSID found, Ethernet AVAILABLE) in local state variables.
2. **Track which interface was tried this cycle.** If the first-choice interface fails at connect, route to the alternate interface within the same attempt (before reaching any FAILED state).
3. **If both fail:** Enter FAILED as normal, triggering the standard full reset.

### Implementation Sketch

Add per-cycle tracking:

```c
static uint8_t wifi_available = 0;
static uint8_t eth_available = 0;
static uint8_t tried_wifi_this_cycle = 0;
static uint8_t tried_eth_this_cycle = 0;

// Reset at start of each Server_Connect cycle
tried_wifi_this_cycle = 0;
tried_eth_this_cycle = 0;
```

Modify the Wi-Fi and Ethernet connect handlers to try the alternate before giving up:

```c
// In handle_wifi_connect_ssid, on connection failure:
tried_wifi_this_cycle = 1;
if (eth_available && !tried_eth_this_cycle) {
    *state = STATE_ETH_CONNECT;  // try Ethernet before giving up
} else {
    *state = STATE_FAILED_3;     // both tried or only Wi-Fi available
}

// In handle_eth_connect, on connection failure:
tried_eth_this_cycle = 1;
if (wifi_available && !tried_wifi_this_cycle) {
    *state = STATE_WIFI_CONNECT_SSID;  // try Wi-Fi before giving up
} else {
    *state = STATE_FAILED_4;           // both tried or only ETH available
}
```

### Impact

- Each connection attempt tries all available paths before triggering a full reset.
- No change to the reset-on-failure philosophy: if both interfaces fail, the standard full reset still occurs.
- Particularly valuable for installations where Ethernet is the primary connection but Wi-Fi is available as a backup (or vice versa).

---

## Finding 2: Ethernet Is Already Connected at Boot But Ignored (High)

### Problem

`ETH.INIT()` in `InitActors` starts the Ethernet driver and DHCP client immediately. The W5500's DHCP typically completes in 1-3 seconds. By the time `SERVER_CONNECT` runs (after all actors are initialized), Ethernet likely already has an IP address.

However, the state machine still goes through `CHECK_WIFI_ENABLED` → `NETWORK_SCAN` → wait for Wi-Fi scan results → only then check Ethernet. The Wi-Fi scan alone adds 3-5 seconds.

**Code reference:** `Ethernet_Init()` in `ETH_Actor.c` line 1021 starts DHCP. `InitActors` in `console_actor.c` line 2260 sends `ETH.INIT()`. The Server Connect state machine starts at `CHECK_CREDENTIALS` and doesn't check ETH status until much later.

### Proposed Solution: Check Ethernet First

Add an early check at the start of the Server Connect flow, right after credentials are verified:

```c
case STATE_CHECK_CREDENTIALS:
    // ... existing credential check ...
    if (credentials_present) {
        // NEW: Check if Ethernet already has IP before scanning Wi-Fi
        if (eth_status == E_ETH_CONNECTED) {
            *state = STATE_NTP_SYNC_CHECK;  // skip Wi-Fi scan entirely
            return;
        }
        *state = STATE_CHECK_WIFI_ENABLED;  // existing path
    }
```

Alternatively, add a new state `STATE_CHECK_ETH_READY` between `CHECK_CREDENTIALS` and `CHECK_WIFI_ENABLED` that takes only a single queue cycle (~3ms) to check if Ethernet already has an IP.

### Impact

- Saves 5-15 seconds on initial boot when Ethernet cable is connected.
- The device can reach `STATE_DEVICE_ANNOUNCE` within 3-5 seconds of boot instead of 10-20 seconds.

---

## Finding 3: Device Announce Backoff Delay Has Pathological Behavior (High)

### Problem

The `Device_Announce_F_delay_u32` doubles on every failure and is applied as a blocking `vTaskDelay` inside `STATE_NTP_SYNC_CHECK`. The progression is:

```
0s → 0s → 10s → 20s → 40s → 80s → 160s → 320s → 640s → ...
```

This continues until it exceeds 100,000 seconds (~27.7 hours), at which point it resets to 10 seconds. This creates a sawtooth pattern where the device may wait many hours between attempts when the server is temporarily down, then suddenly start hammering it every 10 seconds.

Worse, this delay is applied **inside the state machine task**, blocking it from receiving any messages (like a BLE credential update or a manual reset command) during the wait.

**Code reference:** `SYSTEM_Actor.c` lines 2520-2564.

### Proposed Solution: Bounded Backoff with Non-Blocking Wait

1. **Cap the maximum delay at a reasonable value** (e.g., 5 minutes / 300 seconds), not 100,000 seconds.
2. **Use a non-blocking wait** by setting the queue receive timeout to the backoff delay, so the task can still receive messages (like credential updates or reset commands) during the wait.
3. **Reset the backoff counter on success**, not when it overflows.

```c
#define DA_BACKOFF_BASE_S      5
#define DA_BACKOFF_MAX_S       300   // 5 minutes max
#define DA_BACKOFF_MULTIPLIER  2

case STATE_NTP_SYNC_CHECK:
    Send_CMD_To_Other_Actor(NTP, "NTP", "\0", 0, "CONNECT");

    // Non-blocking backoff: use queue delay instead of vTaskDelay
    uint32_t backoff = DA_BACKOFF_BASE_S * (1 << min(device_ann_delay_count, 6));
    if (backoff > DA_BACKOFF_MAX_S) backoff = DA_BACKOFF_MAX_S;
    que_rx_delay = backoff * 1000;
    device_ann_delay_count++;

    state = STATE_DEVICE_ANNOUNCE;
    break;

// On successful Device Announce:
device_ann_delay_count = 0;
```

### Impact

- Maximum wait between Device Announce retries drops from 27+ hours to 5 minutes.
- The state machine remains responsive to BLE credential updates and manual commands during the wait.
- Eliminates the sawtooth pattern.

---

## Finding 4: NTP Sync Is Fire-and-Forget (Medium)

### Problem

`STATE_NTP_SYNC_CHECK` sends `NTP.CONNECT()` and immediately transitions to `STATE_DEVICE_ANNOUNCE` without verifying that the time was actually synchronized. If SNTP synchronization hasn't completed, TLS certificate validation will fail (certificates are time-bound), causing the IHUB TLS connection to fail with a cryptic error.

The device then enters FAILED_11 or FAILED_12, resets the entire stack, and tries again — but if NTP still hasn't synced, the same thing happens in an infinite loop.

**Code reference:** `SYSTEM_Actor.c` lines 2519-2568. The code sends `NTP.CONNECT()` then immediately sets `state = STATE_DEVICE_ANNOUNCE`.

### Proposed Solution: Verify NTP Sync Before Proceeding

Add an NTP sync verification step. The device already has a hardware RTC (PCF8563) that is set during boot, so the system time should be close. But SNTP provides the authoritative time needed for TLS.

```c
case STATE_NTP_SYNC_CHECK:
    Send_CMD_To_Other_Actor(NTP, "NTP", "\0", 0, "CONNECT");

    // Wait for NTP sync (poll sntp_get_sync_status)
    for (int i = 0; i < 10; i++) {       // 10 attempts, 1s apart
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
            break;
        }
    }

    if (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
        // RTC time from boot is used; log warning but proceed
        // (RTC is usually close enough for TLS)
        Add_Response_msg("Warning: NTP sync not confirmed. Using RTC time.",
                         s_Message_Rx, payLoadData_Server);
    }

    state = STATE_DEVICE_ANNOUNCE;
    break;
```

### Impact

- Prevents TLS failures caused by clock skew from cascading into full stack resets.
- Adds at most 10 seconds to the happy path if NTP is slow, but saves 45-90 seconds per failed TLS cycle.

---

## Finding 5: No IHUB Credential Caching (Medium)

### Problem

After a successful Device Announce, the IHUB hostname and primary key are received as commands in the DA response and stored in the IHUB actor's parameters. However, if a stack reset occurs (which deinitializes the IHUB actor), these credentials may be lost and must be re-obtained via another Device Announce call.

This means every recovery cycle includes an HTTP POST to the Device Announce server, even though the credentials haven't changed. If the DA server is temporarily down, the device cannot reconnect to IHUB even though it has valid, unexpired credentials.

**Code reference:** `handle_device_announce()` in `SYSTEM_Actor.c` lines 3177-3186 checks for HOSTNAME and PRIMARY_KEY in the DA response. These are set via actor messages but not persisted to flash.

### Proposed Solution: Cache IHUB Credentials in NVS or SPIFFS

1. After a successful Device Announce that provides IHUB credentials, save them to NVS (or the existing SYSTEM.json in JFS).
2. At `STATE_NTP_SYNC_CHECK`, check if cached IHUB credentials exist. If they do, skip Device Announce and go directly to `STATE_WAITING_FOR_IHUB_RESPONSE`.
3. If the cached credentials fail (IHUB returns an auth error), invalidate the cache and fall back to Device Announce.

```c
case STATE_NTP_SYNC_CHECK:
    // ... NTP sync ...

    // Check for cached IHUB credentials
    if (ihub_hostname_cached && ihub_key_cached) {
        // Apply cached credentials
        Send_CMD_To_Other_Actor(IHUB, "IHUB", cached_creds, len, "SET");
        Send_CMD_To_Other_Actor(IHUB, "IHUB", "\0", 0, "CONNECT");
        state = STATE_WAITING_FOR_IHUB_RESPONSE;
    } else {
        state = STATE_DEVICE_ANNOUNCE;
    }
    break;
```

### Impact

- Eliminates the Device Announce HTTP call on every recovery cycle (~2-10 seconds saved).
- Enables reconnection when the DA server is down but IHUB credentials are still valid.
- Reduces dependency on the DA server for ongoing operation.

---

## Finding 6: Ping Diagnostics Are Too Slow (Medium)

### Problem

When Device Announce fails, the state machine enters a diagnostic sequence: `PING_DNS` (40s timeout) → `PING_GATEWAY` (40s timeout). In the worst case where both time out, this adds **80 seconds** of waiting before reaching a FAILED state, which then adds another 30 seconds of wait before the stack reset.

The pings use `www.google.com` (DNS name) for both DNS and gateway tests, which conflates DNS resolution failures with connectivity failures.

**Code reference:** `PING_DNS_TIMEOUT_SECONDS = 40` and `PING_GATEWAY_TIMEOUT_SECONDS = 40` in `SYSTEM_Actor.c` lines 180-181.

### Proposed Solution

1. **Reduce ping timeouts to 5-10 seconds.** A ping to `www.google.com` that takes more than 5 seconds indicates serious connectivity issues. There's no diagnostic value in waiting 40 seconds.

2. **Use IP addresses for gateway ping.** Ping the actual gateway IP (from DHCP/static config) instead of `www.google.com` for the gateway test. This isolates DNS resolution from network reachability.

3. **Run both pings from one state.** Instead of two sequential states with separate timeouts, send both pings simultaneously and collect results. Use a single combined timeout.

4. **Make diagnostics optional on repeat failures.** On the first DA failure, run diagnostics. On subsequent failures within the same session, skip them — the diagnostic result is the same and the device should spend its time reconnecting, not pinging.

```c
#define PING_DNS_TIMEOUT_SECONDS      5   // was 40
#define PING_GATEWAY_TIMEOUT_SECONDS  5   // was 40
```

### Impact

- Worst-case diagnostic time drops from 80 seconds to 10 seconds.
- Reduces the time spent in diagnostics before reaching the full reset, getting the device back to a clean retry faster.

---

## Finding 7: The 15-Second Delay in InitActors Slows Boot (Medium)

### Problem

`InitActors` in `console_actor.c` has a hardcoded 15-second delay between initializing NTP and initializing EVENT_ACTOR/LIGHTING. The comment says this is "to initialize light actor after Ihub connection (to avoid light flickering)."

This delay occurs before `SERVER_CONNECT` can even begin (since the InitActors task sends the SYSTEM.INIT which leads to SERVER_CONNECT). It means the device is idle for 15 seconds during every boot.

**Code reference:** `console_actor.c` line 2271: `vTaskDelay(15000 / portTICK_PERIOD_MS);`

### Proposed Solution

Move the lighting initialization to a separate deferred task that runs independently after a delay, and let the connection flow proceed immediately:

```c
// In InitActors:
Send_CMD_To_Other_Actor(BLE, "BLE", "\0", 0, "INIT");
Send_CMD_To_Other_Actor(NTP, "NTP", "\0", 0, "INIT");

// Don't wait 15s - launch deferred lighting init as separate task
xTaskCreate(DeferredLightingInit, "LIGHT_INIT", 4096, NULL, 1, NULL);

// InitActors can exit immediately
```

```c
static void DeferredLightingInit(void *param) {
    vTaskDelay(15000 / portTICK_PERIOD_MS);
    Send_CMD_To_Other_Actor(EVENT_ACTOR, "EVENT_ACTOR", "\0", 0, "INIT");
    Send_CMD_To_Other_Actor(LIGHTING, "LIGHTING", "\0", 0, "INIT");
    // ... etc
    vTaskDelete(NULL);
}
```

### Impact

- Boot-to-first-connection-attempt time reduced by 15 seconds.
- Lighting still initializes at the same time as before; only the connection flow is unblocked.

---

## Finding 8: BLE Provisioning Blocks the State Machine (Medium)

### Problem

When credentials or network info is missing, the state machine starts BLE advertising and sets the queue receive delay to 5 minutes (`SER_CONN_BLE_MAX_DELAY = 300,000 ms`). During this 5-minute window, the state machine is effectively blocked — it's sitting in `xQueueReceive` and can only process messages that arrive on the `ServerConnect_Que`.

If Ethernet comes online during this 5-minute wait (e.g., a cable is plugged in), the state machine won't notice until the timeout expires.

**Code reference:** `handle_check_credentials()` sets `*que_rx_delay = SER_CONN_BLE_MAX_DELAY` at lines 2645 and 2651. `handle_network_scan()` does the same at line 2875.

### Proposed Solution

1. **Reduce the BLE wait to shorter polling intervals.** Instead of a single 5-minute wait, use 15-30 second intervals and re-check network conditions each time.

2. **Use event-driven notification.** When the Ethernet `got_ip` event fires, have the ETH actor send a message directly to the `ServerConnect_Que` so the state machine wakes up immediately.

```c
// In ETH_Actor.c got_ip_event_handler(), add:
if (ServerConnect_Que != NULL) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "ETH_CONNECTION_STATUS", "CONNECTED");
    // ... format and send to ServerConnect_Que ...
}
```

```c
// In handle_check_credentials / handle_network_scan:
*que_rx_delay = 30000;  // 30 seconds instead of 5 minutes
// The BLE timeout is tracked separately with a counter:
static uint8_t ble_wait_cycles = 0;
if (ble_wait_cycles++ >= 10) {  // 10 × 30s = 5 minutes total
    // BLE timeout reached
}
```

### Impact

- The state machine responds to Ethernet cable-plug events within 30 seconds instead of up to 5 minutes.
- BLE provisioning still has the same total timeout window (5 minutes), just polled more frequently.

---

## Finding 9: Connectivity Supervisor Grace Period Is Too Long (Low-Medium)

### Problem

The connectivity supervisor has an 8-minute grace period after boot (`CONNECTIVITY_BOOT_GRACE_US`). During this time, it doesn't monitor anything. If the initial connection attempt spirals into a failure loop, there's no automated escalation (beyond the state machine's own retry logic) for 8 minutes.

After the grace period, the supervisor then allows 20 minutes of unhealthy state before resetting. Combined with the 8-minute grace, a completely stuck device won't auto-recover for **28 minutes**.

**Code reference:** `iHub_Actor.c` lines 100-103.

### Proposed Solution

1. **Reduce the boot grace period to 3-4 minutes.** The device should be connected within 2-3 minutes under normal conditions. An 8-minute grace is excessive.

2. **Add a tiered supervisor response:**
   - 0-3 min: Grace period (no action)
   - 3-10 min: Log warning, increase retry frequency
   - 10-15 min: Force stack reset if state machine is stuck
   - 15+ min: Device reboot

3. **Reduce the disconnect timeout to 10 minutes.** 20 minutes offline is a very long time for an IoT device. Most transient issues resolve within 5 minutes.

```c
#define CONNECTIVITY_BOOT_GRACE_US        (3 * 60 * 1000000LL)   // was 8 min
#define CONNECTIVITY_DISCONNECT_TIMEOUT_US (10 * 60 * 1000000LL) // was 20 min
```

### Impact

- Maximum time stuck in a failure state before forced reboot drops from 28 minutes to 13 minutes.
- Ensures the device doesn't stay stuck indefinitely when the state machine is cycling through failures.

---

## Finding 10: Stale Connection Detection Is Sluggish (Low-Medium)

### Problem

`Azure_MQTTContextIsStale()` checks if the MQTT context is stale by comparing against `keepAliveIntervalSec * 3000` (3x keep-alive). With a typical keep-alive of 240 seconds, a stale connection isn't detected for up to **12 minutes**.

During this time, the device appears connected but D2C messages and twin updates silently fail or queue up. The connectivity supervisor's 5-minute activity timeout provides some coverage, but it only triggers a reboot — not a reconnect.

**Code reference:** `iHub_Actor.c` lines 2282-2309.

### Proposed Solution

1. **Tighten the stale detection multiplier** from 3x to 1.5x keep-alive.
2. **Add an active probe:** If no data has been received for 1x keep-alive, send a MQTT PINGREQ and track the response.
3. **React to staleness promptly.** Currently, staleness is detected in the connected loop but the reaction is just to break out and set DISCONNECTED. Ensure the DISCONNECTED state is reported immediately to the SYSTEM actor so the full reset cycle can begin without additional delay.

```c
// Tighter stale detection
timeSinceLastActivity = now - ctx->lastPacketTime;
if (timeSinceLastActivity > (ctx->keepAliveIntervalSec * 1500))  // 1.5x
{
    return true;
}
```

### Impact

- Stale connections detected in ~6 minutes instead of ~12 minutes.
- The full reset cycle begins sooner, reducing the total time the device is silently disconnected from ~15+ minutes to ~6-7 minutes.

---

## Finding 11: No Persistent Failure Counters Across Reboots (Low)

### Problem

The `device_ann_delay_count` and failure counters are all in RAM and reset to 0 on every reboot. If the device is in a reboot loop (e.g., the connectivity supervisor triggers a reset every 28 minutes), it starts from scratch each time — 0 backoff, full connection attempt, same failure, reset, repeat.

There's no way to detect that the device has been failing for hours or days.

### Proposed Solution

1. **Store a boot counter and last failure reason in NVS.** Increment on each boot, reset to 0 on successful IHUB connection.
2. **If the boot counter exceeds a threshold** (e.g., 10 consecutive boot-without-connect cycles), switch to a power-saving mode: attempt connection once every 30 minutes instead of continuously, and keep BLE advertising active for manual intervention.
3. **Log the boot counter to the audit log** so field diagnostics can identify devices stuck in reboot loops.

### Impact

- Prevents devices from burning power in futile retry loops when there's a permanent configuration issue.
- Makes field debugging easier by surfacing how many reboots occurred before the device connected.

---

## Finding 12: The State Machine Ping Tests Both Use DNS Names (Low)

### Problem

Both `STATE_PING_DNS` and `STATE_PING_GATEWAY` ping `www.google.com`. The purpose of a "gateway ping" is to test local network connectivity independently of DNS and internet access. Using a DNS name for the gateway test means a DNS failure will cause both tests to fail, making them indistinguishable.

**Code reference:** `handle_ping_dns()` at line 3122 and `handle_ping_gateway()` — both use `www.google.com`.

### Proposed Solution

- **DNS test:** Ping `www.google.com` (tests DNS + internet) — keep as-is.
- **Gateway test:** Ping the actual gateway IP address (from `s_Para.GW` in ETH or from Wi-Fi IP info). This tests LAN connectivity without requiring DNS.

If DNS ping fails but gateway ping succeeds: DNS issue or ISP problem.
If both fail: Local network is broken.

---

## Summary: Prioritized Implementation Order

All improvements below preserve the full-reset-on-failure recovery model. They focus on preventing unnecessary failure cycles, speeding up each attempt, and improving long-term resilience.

| Priority | Finding | Theme | Effort | Benefit |
|----------|---------|-------|--------|---------|
| **P1** | #1 Try both network interfaces per cycle | Prevent failures | Medium | Avoids wasted reset cycles when alternate interface is healthy |
| **P1** | #3 Fix DA backoff (cap at 5 min, non-blocking) | Prevent failures | Low | Prevents multi-hour stalls between attempts |
| **P1** | #4 Verify NTP sync before proceeding | Prevent failures | Low | Prevents TLS failure spirals caused by clock skew |
| **P1** | #6 Reduce ping timeouts (40s → 5s) | Speed up cycle | Trivial | Saves 70s per DA failure before reaching reset |
| **P2** | #2 Check Ethernet first at boot | Speed up boot | Low | 5-15s faster initial connection |
| **P2** | #5 Cache IHUB credentials in NVS | Speed up cycle | Medium | Skip DA on recovery; works when DA server is down |
| **P2** | #7 Remove 15s InitActors delay | Speed up boot | Low | 15s faster boot-to-first-attempt |
| **P2** | #8 Non-blocking BLE wait | Responsiveness | Medium | Reacts to cable-plug events within 30s vs 5 min |
| **P3** | #9 Reduce supervisor grace/timeout | Resilience | Trivial | 18 min worst-case reduction for stuck devices |
| **P3** | #10 Tighter stale MQTT detection | Resilience | Low | Detects dead connections ~6 min faster |
| **P3** | #11 Persistent failure counters | Diagnostics | Low | Detects and mitigates infinite reboot loops |
| **P3** | #12 Use gateway IP for ping test | Diagnostics | Trivial | Distinguishes DNS vs LAN failures |

### Combined Impact Estimates

All scenarios assume the full-reset-on-failure model is retained. Improvements reduce time wasted *within* each cycle and prevent avoidable failures that trigger resets.

**Current worst case** (DA server temporarily down, network healthy):
```
DA timeout (51s) + DNS ping timeout (40s) + GW ping timeout (40s)
+ failed delay (30s) + stack reset (4s) + credential check (3s)
+ wifi scan (5s) + wifi connect (5s) + NTP (1s) + DA retry ...
= ~179 seconds per cycle, repeating indefinitely
```

**After P1 improvements** (reduced ping timeouts, bounded backoff, NTP verification):
```
DA timeout (51s) + DNS ping (5s) + GW ping (5s)
+ failed delay (30s) + stack reset (4s) + credential check (3s)
+ wifi scan (5s) + wifi connect (5s) + NTP verified (2s) + DA retry ...
= ~110 seconds per cycle (vs 179s), with bounded backoff capping retries at 5 min
```

**After all improvements:**
```
Same full-reset cycle, but:
- Ethernet fast-path skips Wi-Fi scan when cable is connected (~95s per cycle)
- Cached IHUB credentials skip DA entirely on recovery (~60s per cycle)
- Both interfaces tried before declaring failure (fewer wasted reset cycles)
- NTP verified before TLS (fewer TLS-induced failure cycles)
```

| Scenario | Current | After Improvements |
|----------|---------|-------------------|
| DA server down, network OK | 120-180s/cycle | ~110s/cycle, capped at 5 min backoff |
| Wi-Fi fails, Ethernet available | Full reset + retry same interface | Tries Ethernet within same cycle |
| Boot with Ethernet cable | 25-40s to first DA | 8-15s to first DA |
| NTP not synced → TLS fails | Infinite failure loop | NTP verified; proceeds with RTC if needed |
| Stuck in failure loop, supervisor reset | 28 min | 13 min |
