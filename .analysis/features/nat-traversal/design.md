# NAT Traversal via STUN + Coordinated Hole Punching

## Status
IN_REVIEW (Rev 3 - addressing Codex feedback)

## Revision History
| Rev | Date | Changes |
|-----|------|---------|
| 1 | 2024-12-15 | Initial design with libjuice/ICE |
| 2 | 2024-12-20 | Dropped ICE, STUN + hole punch. Addressed socket ownership, auth, payload size, ops safeguards. |
| 3 | 2024-12-20 | **Fixes**: (1) STUN on ENet socket not temp socket, (2) New `list2` endpoint for compat, (3) Metaserver derives IP, only accepts port. |

## Problem / Goal

**Problem:** Internet multiplayer games fail to connect when the host is behind a NAT router without UPnP support or manual port forwarding.

**Goal:** Enable peer-to-peer connections through NAT without requiring users to configure port forwarding, using STUN for address discovery and coordinated UDP hole punching.

## Non-Goals

- Full ICE implementation
- TURN relay server (future work)
- Replacing ENet or modifying ENet internals
- Supporting symmetric NAT ↔ symmetric NAT (requires TURN)

## Design Decision: Option C (STUN + Coordinated Hole Punching)

**Why Option C:**
- Use STUN only for external address discovery
- ENet keeps its socket unchanged
- Metaserver coordinates simultaneous connection attempts
- Works for ~80% of NAT configurations

## Socket Ownership Model (Rev 3 - CORRECTED)

```
┌─────────────────────────────────────────────────────────────────┐
│                    SOCKET OWNERSHIP (Rev 3)                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   ENet owns the UDP socket.                                     │
│   STUN queries are sent ON THE SAME ENET SOCKET.                │
│                                                                 │
│   Why: A temporary socket would bind to a different local port, │
│   resulting in a different external port after NAT, making the  │
│   discovered address useless for ENet's actual traffic.         │
│                                                                 │
│   Implementation:                                               │
│   - Use enet_socket_send(host->socket, ...) for STUN request    │
│   - Use enet_socket_receive(host->socket, ...) for STUN response│
│   - Must handle interleaved STUN responses in ENet's recv loop  │
│   - STUN response identified by magic cookie 0x2112A442         │
│                                                                 │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                  ENet Socket (host->socket)             │   │
│   │                                                         │   │
│   │   ┌─────────────┐  ┌──────────────┐  ┌──────────────┐   │   │
│   │   │ STUN query  │  │ Hole punch   │  │ Game traffic │   │   │
│   │   │ (temporary) │  │ packets      │  │ (permanent)  │   │   │
│   │   └─────────────┘  └──────────────┘  └──────────────┘   │   │
│   │                                                         │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### STUN on ENet Socket - Implementation Detail

```cpp
// StunClient::queryOnSocket() - uses ENet's socket directly
StunClient::Result StunClient::queryOnSocket(
    ENetSocket socket,
    const std::string& stunServer,
    uint16_t stunPort,
    int timeoutMs
) {
    // Resolve STUN server address
    ENetAddress stunAddr;
    enet_address_set_host(&stunAddr, stunServer.c_str());
    stunAddr.port = stunPort;
    
    // Build STUN Binding Request (20 bytes)
    uint8_t request[20];
    buildBindingRequest(request, transactionId);
    
    // Send on ENet's socket
    ENetBuffer sendBuf = { request, 20 };
    if (enet_socket_send(socket, &stunAddr, &sendBuf, 1) < 0) {
        return { false, "", 0, "send failed" };
    }
    
    // Wait for response (with timeout)
    // Note: May receive ENet packets too - filter by STUN magic cookie
    uint8_t response[256];
    ENetAddress fromAddr;
    ENetBuffer recvBuf = { response, sizeof(response) };
    
    Uint32 startTime = SDL_GetTicks();
    while (SDL_GetTicks() - startTime < timeoutMs) {
        int len = enet_socket_receive(socket, &fromAddr, &recvBuf, 1);
        if (len >= 20 && isStunResponse(response, transactionId)) {
            return parseBindingResponse(response, len);
        }
        // Not a STUN response - ENet will handle it in main loop
        // We just keep polling until timeout
        SDL_Delay(10);
    }
    
    return { false, "", 0, "timeout" };
}
```

**Important:** STUN query should be done:
- **Host:** After `enet_host_create()` but before announcing to metaserver
- **Client:** Before initiating hole punch, after getting game list

### Handling Interleaved Packets

During STUN query, ENet may receive:
1. STUN Binding Response (for us)
2. Other UDP traffic (ENet packets, noise)

**Solution:** 
- STUN responses have magic cookie `0x2112A442` at bytes 4-7
- Check this before processing as STUN
- Non-STUN packets are left in socket buffer for ENet's next `enet_host_service()`

## Backward Compatible List Endpoint (Rev 3 - NEW)

### Problem
Adding `session_id` as field 13 to existing `list` response breaks older clients.

### Solution
Keep `command=list` unchanged. Add new `command=list2` endpoint.

| Endpoint | Format | Fields | Clients |
|----------|--------|--------|---------|
| `command=list` | Tab-separated | 12 fields (existing) | All versions |
| `command=list2` | JSON | All fields + session_id, stun_port | 0.99.5+ |

### `command=list2` Response Format

```json
{
  "status": "ok",
  "games": [
    {
      "ip": "1.2.3.4",
      "port": 28747,
      "name": "Player's Game",
      "version": "0.99.5",
      "map": "2P - 64x64 - Map",
      "numPlayers": 1,
      "maxPlayers": 2,
      "passwordProtected": false,
      "lastUpdate": 1703001234,
      "localIP": "192.168.1.100",
      "modName": "vanilla",
      "modVersion": "",
      "sessionId": "a1b2c3d4e5f6",
      "stunPort": 28747
    }
  ]
}
```

**Notes:**
- `sessionId` is generated by metaserver on game creation (12 char alphanumeric)
- `stunPort` is the external port discovered via STUN (may differ from `port` if host used STUN)
- Client parses JSON, uses `sessionId` for punch signaling

### Client Version Detection

```cpp
// In MetaServerClient
void refreshGameList() {
    // Try list2 first (new clients)
    std::string result = loadFromHttp(metaServerURL, {{"command", "list2"}});
    if (result.starts_with("{")) {
        parseJsonGameList(result);
    } else {
        // Fallback to old format (or old server)
        parseTabGameList(loadFromHttp(metaServerURL, {{"command", "list"}}));
        // Hole punching unavailable with old server
    }
}
```

## IP Spoofing Prevention (Rev 3 - CORRECTED)

### Problem
If metaserver accepts `client_addr`/`host_addr` as client-provided strings, attackers can:
1. Make hosts send UDP packets to arbitrary victims (amplification attack)
2. Impersonate other players

### Solution
Metaserver **derives IP** from connection, only accepts **port** from client.

```php
// punch_request endpoint
function handlePunchRequest() {
    $body = json_decode(file_get_contents('php://input'), true);
    
    // Derive IP from connection - cannot be spoofed
    $clientIP = getRealClientIP();
    
    // Only accept port from client (from their STUN discovery)
    $clientPort = intval($body['stun_port'] ?? 0);
    if ($clientPort < 1024 || $clientPort > 65535) {
        return jsonError('invalid port');
    }
    
    // Validate session exists
    $sessionId = $body['session_id'] ?? '';
    if (!validateSessionId($sessionId)) {
        return jsonError('invalid session');
    }
    
    // Store with derived IP
    $punchRequest = [
        'client_ip' => $clientIP,        // FROM CONNECTION, not body
        'client_port' => $clientPort,    // From body (STUN-discovered)
        'client_id' => generateNonce(),  // Server-generated
        'timestamp' => time()
    ];
    
    storePunchRequest($sessionId, $punchRequest);
    return jsonOk(['client_id' => $punchRequest['client_id']]);
}
```

### Same for Host Punch Ready

```php
function handlePunchReady() {
    $body = json_decode(file_get_contents('php://input'), true);
    
    // Validate host owns this game
    $secret = $body['secret'] ?? '';
    $game = getGameBySecret($secret);
    if (!$game) {
        return jsonError('invalid secret');
    }
    
    // Host address from game registration (already validated)
    // NOT from this request body
    $hostIP = $game['ip'];
    $hostPort = $game['stun_port'] ?? $game['port'];
    
    // Mark punch ready
    $clientId = $body['client_id'] ?? '';
    markPunchReady($game['session_id'], $clientId, $hostIP, $hostPort);
}
```

### Validation Summary

| Field | Source | Validation |
|-------|--------|------------|
| `client_ip` | `getRealClientIP()` | Cannot be spoofed |
| `client_port` | Client body (STUN result) | Range 1024-65535 |
| `host_ip` | Stored from game creation | Cannot be overridden |
| `host_port` | Stored from game creation | Cannot be overridden |
| `session_id` | Client body | Must exist, alphanumeric |
| `secret` | Host body | Must match game owner |

## High-Level Flow (Updated)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      HOLE PUNCH COORDINATION (Rev 3)                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  HOST                         METASERVER                        CLIENT      │
│   │                               │                               │         │
│   │── 1. enet_host_create() ─────>│                               │         │
│   │                               │                               │         │
│   │── 2. STUN query ─────────────>│ (on ENet socket!)             │         │
│   │      (stun.google.com)        │                               │         │
│   │<─ 3. External IP:stunPort ────│                               │         │
│   │                               │                               │         │
│   │── 4. Announce game ──────────>│ (IP from connection,          │         │
│   │      POST /announce           │  stunPort in body)            │         │
│   │      {stunPort, ...}          │  → generates session_id       │         │
│   │                               │                               │         │
│   │                               │<── 5. GET /list2 ─────────────│         │
│   │                               │─── 6. JSON game list ────────>│         │
│   │                               │    (includes session_id)      │         │
│   │                               │                               │         │
│   │                               │                ┌──────────────│         │
│   │                               │                │ 7. STUN query│         │
│   │                               │                │ (on ENet sock)         │
│   │                               │                └──────────────│         │
│   │                               │                               │         │
│   │                               │<── 8. POST /punch_request ────│         │
│   │                               │    {session_id, stun_port}    │         │
│   │                               │    (IP from getRealClientIP)  │         │
│   │                               │                               │         │
│   │<── 9. GET /punch_poll ────────│                               │         │
│   │    ?secret=X                  │                               │         │
│   │    → {client_ip, client_port} │                               │         │
│   │                               │                               │         │
│   │── 10. POST /punch_ready ─────>│                               │         │
│   │    {secret, client_id}        │                               │         │
│   │                               │                               │         │
│   │                               │<── 11. GET /punch_status ─────│         │
│   │                               │    ?session_id&client_id      │         │
│   │                               │─── 12. {ready, host_ip:port} >│         │
│   │                               │    punch_in_seconds: 2        │         │
│   │                               │                               │         │
│   │<══════════════ 13. SIMULTANEOUS UDP (on ENet sockets) ══════>│         │
│   │                               │                               │         │
│   │<─────────────── 14. ENet connection established ────────────>│         │
│   │                               │                               │         │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Metaserver Endpoints (Complete Specification)

### Existing Endpoints (Unchanged)
- `command=list` - Tab-separated game list (12 fields, backward compat)
- `command=announce` - Register game (existing, add `stun_port` param)
- `command=update` - Update player count
- `command=remove` - Remove game

### New Endpoints

#### `command=list2` (GET)
Returns JSON game list with hole punch fields.

**Request:** `GET /metaserver.php?command=list2`

**Response:**
```json
{"status": "ok", "games": [...]}
```

#### `command=punch_request` (POST)
Client requests hole punch with a host.

**Request:**
```
POST /metaserver.php?command=punch_request
Content-Type: application/json

{"session_id": "abc123", "stun_port": 28747}
```

**Server derives:** `client_ip` from `getRealClientIP()`

**Response:**
```json
{"status": "ok", "client_id": "xyz789"}
```

**Constraints:**
- Max 5 pending requests per session
- TTL: 60 seconds
- Rate limit: 10/min per IP

#### `command=punch_poll` (GET)
Host polls for pending punch requests.

**Request:** `GET /metaserver.php?command=punch_poll&secret=<secret>`

**Response:**
```json
{
  "status": "ok",
  "requests": [
    {"client_id": "xyz789", "client_ip": "5.6.7.8", "client_port": 28747}
  ]
}
```

#### `command=punch_ready` (POST)
Host signals ready to punch.

**Request:**
```
POST /metaserver.php?command=punch_ready
Content-Type: application/json

{"secret": "host_secret", "client_id": "xyz789"}
```

**Response:**
```json
{"status": "ok"}
```

#### `command=punch_status` (GET)
Client polls for punch readiness.

**Request:** `GET /metaserver.php?command=punch_status&session_id=abc&client_id=xyz`

**Response (waiting):**
```json
{"status": "waiting"}
```

**Response (ready):**
```json
{
  "status": "ready",
  "host_ip": "1.2.3.4",
  "host_port": 28747,
  "punch_in_seconds": 2
}
```

## Hole Punch Packet Format

```cpp
// 4-byte marker to identify hole punch packets
// Distinct from ENet protocol command bytes (0x00-0x0F)
const uint8_t HOLE_PUNCH_MARKER[4] = {0x44, 0x4C, 0x48, 0x50};  // "DLHP"

void NetworkManager::executeHolePunch(const ENetAddress& peerAddr) {
    // Send 10 packets over 2 seconds
    for (int i = 0; i < 10; i++) {
        ENetBuffer buffer = { (void*)HOLE_PUNCH_MARKER, 4 };
        enet_socket_send(host->socket, &peerAddr, &buffer, 1);
        SDL_Delay(200);
    }
    
    // Then attempt ENet connect
    connectPeer = enet_host_connect(host, &peerAddr, 2, 0);
}
```

**Note (from review):** These raw UDP packets may trigger ENet "invalid packet" warnings in logs. This is expected and harmless - document in release notes.

## Ops Safeguards

### TTL Cleanup
- Punch requests: 60 second TTL
- Punch ready signals: 30 second TTL
- Cleanup on every metaserver request (lazy) or cron

### Bounded Storage
- Max 5 pending punch requests per session
- Max 10 active sessions per IP (existing limit)
- Total punch data: ~1KB per session max

### Rate Limiting
- `punch_request`: 10/min per IP
- `punch_poll`: 30/min per IP
- `punch_status`: 30/min per IP

**Implementation:** APCu if available, else file-based, else skip for v1 (note in implementation.md).

## Configuration

```ini
[Network]
# STUN servers (comma-separated)
StunServers = stun.l.google.com:19302,stun.cloudflare.com:3478

# Enable NAT hole punching
EnableHolePunch = true

# Timeouts (seconds)
StunTimeout = 3
PunchTimeout = 10
```

## Test Plan

### Unit Tests
- [ ] STUN query on ENet socket returns valid external address
- [ ] STUN response parsing (XOR-MAPPED-ADDRESS)
- [ ] Session ID validation (alphanumeric, length)
- [ ] Metaserver IP derivation (mock getRealClientIP)

### Integration Tests
- [ ] Full flow: announce → list2 → punch_request → poll → ready → punch → connect
- [ ] Fallback when punch times out
- [ ] Old client with new server (uses `list`, no hole punch)
- [ ] New client with old server (fallback to `list`, no hole punch)

### Manual Tests
- Two machines behind different NATs, both cone NAT → connects
- One machine with port forward, other behind NAT → connects (no punch needed)
- Both behind symmetric NAT → fails gracefully with error message

## Rollout / Rollback

### Phases
1. Implement StunClient (on ENet socket)
2. Add `list2` and punch endpoints to metaserver
3. Implement client-side hole punch flow
4. Enable by default (`EnableHolePunch = true`)

### Rollback
- `EnableHolePunch = false` disables client-side
- Metaserver endpoints are additive (no removal needed)
- `list` endpoint unchanged (old clients unaffected)

## Risks / Open Questions

### Resolved (Rev 3)
1. **STUN socket mismatch** → STUN now uses ENet socket
2. **list compatibility** → New `list2` endpoint
3. **IP spoofing** → Metaserver derives IP from connection

### Remaining Risks
| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| STUN interleaved with ENet traffic | Medium | Low | Filter by magic cookie, tested |
| Clock skew for punch timing | Low | Low | Use `punch_in_seconds` (relative) |
| Symmetric NAT still fails | High | Medium | Clear error, suggest alternatives |

### Open Questions
1. **APCu availability?** → File-based fallback or skip rate limit for v1
2. **ENet "invalid packet" noise?** → Document as expected, log at DEBUG level
