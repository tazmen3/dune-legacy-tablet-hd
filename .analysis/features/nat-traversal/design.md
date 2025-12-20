# NAT Traversal via STUN + Coordinated Hole Punching

## Status
IN_REVIEW (Rev 2 - addressing Codex feedback)

## Revision History
| Rev | Date | Changes |
|-----|------|---------|
| 1 | 2024-12-15 | Initial design with libjuice/ICE |
| 2 | 2024-12-20 | **Major revision**: Dropped full ICE in favor of STUN + coordinated hole punching on ENet socket. Addresses Codex blocking issues: socket ownership, signaling auth, payload size, ops safeguards. |

## Problem / Goal

**Problem:** Internet multiplayer games fail to connect when the host is behind a NAT router without UPnP support or manual port forwarding.

**Goal:** Enable peer-to-peer connections through NAT without requiring users to configure port forwarding, using STUN for address discovery and coordinated UDP hole punching.

## Non-Goals

- Full ICE implementation (PRIO/PRFLX candidate types, nomination)
- TURN relay server (future work if needed)
- Replacing ENet or modifying ENet internals
- Supporting symmetric NAT ↔ symmetric NAT (requires TURN)

## Design Decision: Option C (STUN + Coordinated Hole Punching)

### Why Not Full ICE (Options A/B)?

Per Codex review, full ICE has socket ownership issues:
- **Option A** (libjuice owns socket): Requires adapting ENet to use external socket - major changes
- **Option B** (ENet owns socket, ICE callbacks): Requires demuxing ICE vs ENet traffic, complex

**Option C** is simpler:
- Use STUN only for external address discovery (no libjuice needed)
- ENet keeps its socket unchanged
- Metaserver coordinates simultaneous connection attempts
- Both peers send to each other at the same time, creating NAT mappings
- Works for ~80% of NAT configurations (fails only for symmetric↔symmetric)

### Socket Ownership Model (Explicit)

```
┌─────────────────────────────────────────────────────────────────┐
│                    SOCKET OWNERSHIP                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   ENet owns the UDP socket throughout the entire lifetime.      │
│   STUN queries are sent on a SEPARATE temporary socket.         │
│   No socket handoff or sharing required.                        │
│                                                                 │
│   STUN Socket (temporary)     ENet Socket (permanent)           │
│   ┌─────────────────┐         ┌─────────────────┐               │
│   │ Query STUN      │         │ Game traffic    │               │
│   │ Get external IP │         │ Hole punch pkts │               │
│   │ Close when done │         │ All ENet comms  │               │
│   └─────────────────┘         └─────────────────┘               │
│                                                                 │
│   The hole punch packets ARE ENet packets (empty/ping),         │
│   sent from ENet's socket to create NAT mappings.               │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

## Acceptance Criteria

- [ ] Two players behind different NAT routers can connect without port forwarding (compatible NAT types)
- [ ] Connection falls back to direct connection if hole punching fails
- [ ] Existing LAN game discovery continues to work unchanged
- [ ] Connection establishment takes <15 seconds in typical cases
- [ ] Clear error messages when connection fails
- [ ] No regression in existing multiplayer functionality
- [ ] Metaserver signaling uses public session_id (not secret)
- [ ] Signaling endpoints have TTL cleanup, rate limiting, bounded storage

## Approach

### High-Level Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      HOLE PUNCH COORDINATION                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  HOST                         METASERVER                        CLIENT      │
│   │                               │                               │         │
│   │── 1. STUN query ─────────────>│ (to stun.l.google.com)        │         │
│   │<─ 2. External IP:port ────────│                               │         │
│   │                               │                               │         │
│   │── 3. Announce game ──────────>│ (includes external_addr,      │         │
│   │      session_id = random      │  session_id, enet_port)       │         │
│   │                               │                               │         │
│   │                               │<── 4. List games ─────────────│         │
│   │                               │─── 5. Game list ─────────────>│         │
│   │                               │    (includes session_id,      │         │
│   │                               │     external_addr)            │         │
│   │                               │                               │         │
│   │                               │<── 6. STUN query ─────────────│         │
│   │                               │    (to stun server)           │         │
│   │                               │─── 7. External IP:port ──────>│         │
│   │                               │                               │         │
│   │                               │<── 8. Join request ───────────│         │
│   │                               │    POST /punch_request        │         │
│   │                               │    {session_id, client_addr}  │         │
│   │                               │                               │         │
│   │<── 9. Poll punch requests ────│                               │         │
│   │    GET /punch_poll?secret=X   │                               │         │
│   │    Response: client_addr      │                               │         │
│   │                               │                               │         │
│   │                               │─── 10. Punch ready ──────────>│         │
│   │                               │    (tells client to start)    │         │
│   │                               │                               │         │
│   │<══════════════ 11. SIMULTANEOUS UDP PACKETS ════════════════>│         │
│   │    (both send ENet ping to each other's external addr)       │         │
│   │    (NAT mappings created, packets start flowing)             │         │
│   │                               │                               │         │
│   │<─────────────── 12. ENet connection established ────────────>│         │
│   │                               │                               │         │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Components

#### 1. StunClient Class (new, simple)

Minimal STUN client for external address discovery only (RFC 5389 Binding Request).

```cpp
// include/Network/StunClient.h
class StunClient {
public:
    struct Result {
        bool success;
        std::string externalIP;
        uint16_t externalPort;
        std::string error;
    };
    
    // Blocking call - query STUN server, return external address
    // Uses a temporary UDP socket (not ENet's socket)
    static Result queryExternalAddress(
        const std::string& stunServer = "stun.l.google.com",
        uint16_t stunPort = 19302,
        int timeoutMs = 3000
    );
};
```

**Implementation notes:**
- Creates temporary UDP socket, binds to same local port as ENet if possible
- Sends STUN Binding Request
- Parses XOR-MAPPED-ADDRESS from response
- Closes socket when done
- ~100 lines of code, no external library needed

#### 2. Session ID for Signaling (Public Identifier)

**Problem (from review):** Clients don't have the host's `secret`, so can't use it for signaling.

**Solution:** Add a public `session_id` to game announcements:

```php
// Metaserver stores per-game:
$game = [
    'secret' => 'abc123...',        // Private, host-only (existing)
    'session_id' => 'xyz789...',    // Public, included in list response (NEW)
    'external_addr' => '1.2.3.4:28747',
    'punch_requests' => [],          // Pending hole punch requests (NEW)
    // ... existing fields
];
```

**List response change:**
```
# Current format (tab-separated):
<ip>\t<port>\t<name>\t<version>\t<map>\t<numplayers>\t<maxplayers>\t...

# New format (add session_id as field 13):
<ip>\t<port>\t<name>\t<version>\t<map>\t<numplayers>\t<maxplayers>\t<pwd>\t<lastupdate>\t<localip>\t<modname>\t<modversion>\t<session_id>
```

#### 3. Metaserver Signaling Endpoints (Revised)

All new endpoints use **POST** with JSON body to handle payload size.

##### 3.1 Punch Request (Client → Metaserver)

Client requests hole punch coordination with a host.

```
POST /metaserver.php?command=punch_request
Content-Type: application/json

{
    "session_id": "xyz789...",
    "client_addr": "5.6.7.8:28747",
    "client_id": "random_nonce"
}
```

**Response:**
```json
{"status": "ok", "message": "queued"}
```
or
```json
{"status": "error", "message": "session not found"}
```

**Server-side:**
- Validate `session_id` exists and game is active
- Store punch request with TTL (60 seconds)
- Max 5 pending requests per session (bounded storage)
- Rate limit: 10 requests/minute per IP

##### 3.2 Punch Poll (Host → Metaserver)

Host polls for pending punch requests.

```
GET /metaserver.php?command=punch_poll&secret=<host_secret>
```

**Response:**
```json
{
    "status": "ok",
    "requests": [
        {"client_id": "abc", "client_addr": "5.6.7.8:28747", "timestamp": 1703001234}
    ]
}
```

**Server-side:**
- Validate `secret` matches an active game
- Return pending requests (max 5)
- Mark requests as "notified" (so host doesn't get duplicates)

##### 3.3 Punch Ready (Host → Metaserver → Client polling)

Host acknowledges punch request, signals client to start punching.

```
POST /metaserver.php?command=punch_ready
Content-Type: application/json

{
    "secret": "abc123...",
    "client_id": "abc",
    "host_addr": "1.2.3.4:28747"
}
```

Client polls for ready signal:

```
GET /metaserver.php?command=punch_status&session_id=xyz&client_id=abc
```

**Response:**
```json
{"status": "ready", "host_addr": "1.2.3.4:28747", "punch_at": 1703001240}
```

`punch_at` is a Unix timestamp (server time) when both should start sending packets (gives 2 second coordination window).

#### 4. Hole Punch Execution

Both host and client, at `punch_at` time:

```cpp
void NetworkManager::executeHolePunch(const ENetAddress& peerExternalAddr) {
    // Send 10 empty UDP packets over 2 seconds to peer's external address
    // These are sent from ENet's socket to create NAT mapping
    for (int i = 0; i < 10; i++) {
        ENetBuffer buffer;
        char data[4] = {0x44, 0x4C, 0x48, 0x50};  // "DLHP" = Dune Legacy Hole Punch
        buffer.data = data;
        buffer.dataLength = 4;
        
        enet_socket_send(host->socket, &peerExternalAddr, &buffer, 1);
        SDL_Delay(200);  // 200ms between packets
    }
    
    // After punching, try normal ENet connect
    // NAT should now have mapping for return packets
    connectPeer = enet_host_connect(host, &peerExternalAddr, 2, 0);
}
```

**Key point:** Packets are sent from ENet's existing socket, so the NAT mapping is for ENet's port.

#### 5. Payload Size Limits and Validation

| Field | Max Size | Validation |
|-------|----------|------------|
| session_id | 32 chars | Alphanumeric only |
| client_id | 32 chars | Alphanumeric only |
| client_addr | 21 chars | IP:port format (xxx.xxx.xxx.xxx:xxxxx) |
| host_addr | 21 chars | IP:port format |

Total JSON body: <200 bytes (well under any reasonable limit)

**Sanitization:**
```php
function validateSessionId($id) {
    return preg_match('/^[a-zA-Z0-9]{8,32}$/', $id);
}

function validateAddr($addr) {
    return preg_match('/^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}:\d{1,5}$/', $addr);
}
```

#### 6. Ops Safeguards

##### TTL Cleanup
```php
// In metaserver update() or cron:
foreach ($games as $secret => &$game) {
    // Remove punch requests older than 60 seconds
    $game['punch_requests'] = array_filter(
        $game['punch_requests'],
        fn($r) => time() - $r['timestamp'] < 60
    );
    
    // Remove punch_ready signals older than 30 seconds
    $game['punch_ready'] = array_filter(
        $game['punch_ready'],
        fn($r) => time() - $r['timestamp'] < 30
    );
}
```

##### Bounded Storage
- Max 5 pending punch requests per game
- Max 10 active games per IP (existing)
- Punch data stored in memory (servers.dat), cleaned on TTL

##### Rate Limiting
```php
// Per-IP rate limiting for punch_request
$rateLimitKey = 'punch_' . $clientIP;
$requestCount = apcu_fetch($rateLimitKey) ?: 0;
if ($requestCount > 10) {  // 10 requests per minute
    http_response_code(429);
    die('Rate limited');
}
apcu_store($rateLimitKey, $requestCount + 1, 60);
```

**Note:** If APCu not available, use file-based rate limiting or skip (non-critical for initial release).

## Configuration

Add to `Dune Legacy.ini`:

```ini
[Network]
# STUN servers (comma-separated, tried in order)
StunServers = stun.l.google.com:19302,stun.cloudflare.com:3478

# Enable NAT hole punching for internet games
EnableHolePunch = true

# Hole punch timeout (seconds)
HolePunchTimeout = 10
```

## Interfaces / Touch Points

### Files Modified

| File | Changes |
|------|---------|
| `include/Network/StunClient.h` | **NEW** - STUN client declaration |
| `src/Network/StunClient.cpp` | **NEW** - STUN client implementation (~150 lines) |
| `include/Network/NetworkManager.h` | Add hole punch methods, session_id |
| `src/Network/NetworkManager.cpp` | STUN query on server start, hole punch execution |
| `src/Network/MetaServerClient.cpp` | Punch request/poll/status methods, POST support |
| `src/Menu/MultiPlayerMenu.cpp` | Parse session_id from list, initiate punch on join |
| `include/Network/ENetPacketStream.h` | (no change - using raw socket for punch) |
| `dunelegacy.com/metaserver/metaserver.php` | Punch signaling endpoints, session_id in list |
| `include/misc/Settings.h` | STUN server config, EnableHolePunch |

### External Dependencies

**None new.** STUN client is implemented directly (~150 lines), no libjuice needed.

## Test Plan

### Unit Tests
- [ ] `StunClient::queryExternalAddress()` returns valid IP for public STUN servers
- [ ] Session ID generation is unique and valid format
- [ ] Metaserver punch endpoints validate input correctly
- [ ] Rate limiting triggers after threshold

### Integration Tests
- [ ] Full hole punch flow: host announces, client requests, punch executes, ENet connects
- [ ] Fallback to direct connection when hole punch times out
- [ ] TTL cleanup removes stale punch requests

### Manual Tests

| Scenario | Steps | Expected Result |
|----------|-------|-----------------|
| Compatible NAT | Host behind NAT A, Client behind NAT B, both cone NAT | Connection via hole punch |
| One open NAT | Host behind NAT, Client on open internet | Falls back to direct, connects |
| Symmetric NAT | Host behind symmetric NAT | Hole punch fails, error shown, suggests port forward |
| LAN game | Both on same network | Uses LAN discovery, hole punch skipped |
| Timeout | Host never polls punch requests | Client times out after 10s, shows error |

### Test Commands
```bash
# Build
cd dunelegacy/build && cmake --build . -j8

# Test STUN client standalone
./bin/dunelegacy.app/Contents/MacOS/dunelegacy --test-stun

# Check logs for hole punch
tail -f "~/Library/Application Support/Dune Legacy/Dune Legacy.log" | grep -iE "stun|punch|hole"
```

## Timeouts and Phases

| Phase | Timeout | On Timeout |
|-------|---------|------------|
| STUN query | 3 seconds | Try next STUN server, then skip (use local IP) |
| Punch request delivery | 5 seconds | Retry once, then fail |
| Wait for punch_ready | 5 seconds | Fall back to direct connection |
| Hole punch execution | 5 seconds | Try direct connection |
| ENet connect after punch | 5 seconds | Show connection failed error |

**Total worst case:** ~15 seconds before showing error.

## Privacy Considerations

- External IP addresses are already shared via metaserver (existing behavior)
- Local/LAN IPs are included in existing `localip` field (existing behavior)
- STUN queries go to third-party servers (Google/Cloudflare by default)
- Users can configure custom STUN servers in ini file
- **No additional privacy exposure vs current design**

## Rollout / Rollback

### Rollout Plan
1. **Phase 1**: Implement StunClient, test independently
2. **Phase 2**: Add metaserver signaling endpoints (behind feature flag on server)
3. **Phase 3**: Implement client-side hole punch flow (behind `EnableHolePunch` setting)
4. **Phase 4**: Enable by default after testing
5. **Phase 5**: Remove feature flag, always-on

### Rollback
- Set `EnableHolePunch = false` in ini to disable
- Revert metaserver changes (signaling endpoints are additive)
- Code changes are additive, easy to revert

### Feature Flag
```cpp
if (settings.network.enableHolePunch && isInternetGame && !foundOnLAN) {
    initiateHolePunch(gameServerInfo);
} else {
    directConnect(gameServerInfo.serverAddress);
}
```

## Risks / Open Questions

### Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| STUN servers unavailable | Low | Low | Multiple fallback servers, graceful degradation |
| Symmetric NAT common | Medium | Medium | Clear error message, suggest alternatives |
| Time sync issues | Low | Medium | Use relative timing (punch_at = now + 2s) |
| Metaserver load from polling | Low | Low | Rate limiting, short poll interval, TTL cleanup |

### Resolved Questions (from Rev 1)

1. **Socket ownership model?**
   - **Answer:** ENet keeps its socket. STUN uses temporary separate socket. Hole punch packets sent from ENet's socket.

2. **Public identifier for signaling?**
   - **Answer:** New `session_id` field added to game announcements and list response.

3. **Payload size and transport?**
   - **Answer:** Use POST with JSON body. Payloads <200 bytes. Explicit validation.

4. **Ops safeguards?**
   - **Answer:** 60s TTL on punch requests, max 5 per game, per-IP rate limiting.

### Open Questions

1. **APCu availability for rate limiting?**
   - If not available, use file-based or skip rate limiting for v1.

2. **Clock skew between client/host?**
   - Use relative timing: server returns `punch_in_seconds: 2` instead of absolute timestamp.

## Appendix: STUN Binding Request/Response

```
STUN Binding Request (20 bytes):
  0x00 0x01             - Message Type: Binding Request
  0x00 0x00             - Message Length: 0 (no attributes)
  0x21 0x12 0xa4 0x42   - Magic Cookie
  <12 bytes>            - Transaction ID

STUN Binding Response:
  0x01 0x01             - Message Type: Binding Response
  <length>              - Message Length
  0x21 0x12 0xa4 0x42   - Magic Cookie
  <12 bytes>            - Transaction ID (same as request)
  
  XOR-MAPPED-ADDRESS attribute (0x0020):
    0x00 0x01           - Family: IPv4
    <2 bytes>           - Port XOR'd with magic cookie high bits
    <4 bytes>           - IP XOR'd with magic cookie
```

Implementation is straightforward, ~100 lines of code.
