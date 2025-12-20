# NAT Traversal via STUN + Coordinated Hole Punching

## Status
IN_REVIEW (Rev 4 - addressing Codex feedback)

## Revision History
| Rev | Date | Changes |
|-----|------|---------|
| 1 | 2024-12-15 | Initial design with libjuice/ICE |
| 2 | 2024-12-20 | Dropped ICE, STUN + hole punch |
| 3 | 2024-12-20 | STUN on ENet socket, `list2` endpoint, IP derived server-side |
| 4 | 2024-12-20 | **Fixes**: (1) STUN only pre-connection with no peers, (2) Explicit host/client punch roles, (3) `command=add` takes optional `stun_port` |

## Problem / Goal

**Problem:** Internet multiplayer games fail to connect when the host is behind a NAT router without UPnP support or manual port forwarding.

**Goal:** Enable peer-to-peer connections through NAT using STUN for address discovery and coordinated UDP hole punching.

## Non-Goals

- Full ICE implementation
- TURN relay server (future work)
- Modifying ENet internals
- Supporting symmetric NAT ↔ symmetric NAT

## STUN Receive Safety (Rev 4 - CORRECTED)

### Problem (from Rev 3 review)
The proposed `enet_socket_receive()` loop cannot "leave non-STUN packets in the socket buffer for ENet" - once you `receive()`, the datagram is consumed. Running this while ENet traffic exists will drop real packets.

### Solution: STUN Only Pre-Connection (Option B)

**Guarantee:** STUN queries only occur when:
1. No ENet peers are connected (`peerList.empty()` and `awaitingConnectionList.empty()`)
2. `enet_host_service()` is NOT being called during the STUN window

**Implementation:**

```cpp
// HOST: STUN runs immediately after enet_host_create(), before any peers can connect
void NetworkManager::startServer(...) {
    // 1. Create ENet host
    host = enet_host_create(&address, 32, 2, 0, 0);
    
    // 2. STUN query NOW - no peers exist yet, no enet_host_service() running
    //    Safe to use enet_socket_receive() exclusively
    if (!bLANServer && settings.network.enableHolePunch) {
        stunResult = StunClient::queryOnSocket(host->socket, stunServer, stunPort, 3000);
        if (stunResult.success) {
            externalStunPort = stunResult.externalPort;
            SDL_Log("STUN: External address %s:%d", stunResult.externalIP.c_str(), externalStunPort);
        }
    }
    
    // 3. NOW start announcing and accepting connections
    //    From this point, enet_host_service() owns the socket
    bIsServer = true;
    // ... announce to metaserver with stun_port
}

// CLIENT: STUN runs before initiating connection
void NetworkManager::connectWithHolePunch(const GameServerInfo& gameInfo) {
    // Precondition: not connected to anything yet
    assert(peerList.empty() && connectPeer == nullptr);
    
    // 1. STUN query - safe, no ENet traffic
    stunResult = StunClient::queryOnSocket(host->socket, stunServer, stunPort, 3000);
    
    // 2. Request hole punch via metaserver
    //    (metaserver derives our IP, we provide stun_port)
    requestHolePunch(gameInfo.sessionId, stunResult.externalPort);
    
    // 3. Wait for punch_ready, then punch + connect
    // ... (see Host/Client Punch Roles below)
}
```

### StunClient Implementation (Safe Version)

```cpp
StunClient::Result StunClient::queryOnSocket(
    ENetSocket socket,
    const std::string& stunServer,
    uint16_t stunPort,
    int timeoutMs
) {
    // PRECONDITION: Caller guarantees no concurrent enet_host_service()
    //               and no peers connected. All received packets are ours.
    
    ENetAddress stunAddr;
    enet_address_set_host(&stunAddr, stunServer.c_str());
    stunAddr.port = stunPort;
    
    // Build and send STUN Binding Request
    uint8_t request[20];
    uint8_t transactionId[12];
    generateTransactionId(transactionId);
    buildBindingRequest(request, transactionId);
    
    ENetBuffer sendBuf = { request, 20 };
    if (enet_socket_send(socket, &stunAddr, &sendBuf, 1) < 0) {
        return { false, "", 0, "send failed" };
    }
    
    // Receive response - we OWN the socket during this window
    uint8_t response[256];
    ENetAddress fromAddr;
    ENetBuffer recvBuf = { response, sizeof(response) };
    
    Uint32 deadline = SDL_GetTicks() + timeoutMs;
    while (SDL_GetTicks() < deadline) {
        // Set socket to non-blocking for polling
        enet_socket_set_option(socket, ENET_SOCKOPT_NONBLOCK, 1);
        int len = enet_socket_receive(socket, &fromAddr, &recvBuf, 1);
        
        if (len >= 20) {
            // Verify it's our STUN response (magic cookie + transaction ID)
            if (isStunBindingResponse(response, len, transactionId)) {
                return parseXorMappedAddress(response, len);
            }
            // Else: unexpected packet (shouldn't happen pre-connection)
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, 
                "STUN: Unexpected %d-byte packet during STUN query (dropped)", len);
        }
        
        SDL_Delay(50);  // Poll every 50ms
    }
    
    return { false, "", 0, "timeout" };
}
```

### Safety Guarantees Summary

| Phase | Who owns socket? | STUN safe? | ENet traffic? |
|-------|------------------|------------|---------------|
| Before `enet_host_create()` | Nobody | N/A | No |
| After create, before announce | STUN query | **Yes** | No |
| After announce, accepting peers | `enet_host_service()` | **No** | Yes |
| Client before connect | STUN query | **Yes** | No |
| Client after `enet_host_connect()` | `enet_host_service()` | **No** | Yes |

## Host vs Client Punch Roles (Rev 4 - EXPLICIT)

### Host Responsibilities

```cpp
// In NetworkManager (host side)

void NetworkManager::handlePunchPoll() {
    // Called periodically while hosting, before game starts
    auto requests = pollPunchRequests();  // GET /punch_poll?secret=X
    
    for (const auto& req : requests) {
        // Store client address for punching
        pendingPunchClients.push_back({
            .clientId = req.clientId,
            .clientIP = req.clientIP,
            .clientPort = req.clientPort
        });
        
        // Signal ready to metaserver
        sendPunchReady(req.clientId);  // POST /punch_ready
        
        // Schedule punch execution
        schedulePunch(req, SDL_GetTicks() + 2000);  // 2 seconds from now
    }
}

void NetworkManager::executePunchAsHost(const PunchTarget& target) {
    // HOST: Send punch packets but DO NOT call enet_host_connect()
    // The CLIENT will connect to us; we just create the NAT mapping
    
    ENetAddress clientAddr;
    enet_address_set_host(&clientAddr, target.clientIP.c_str());
    clientAddr.port = target.clientPort;
    
    SDL_Log("HOST: Punching to %s:%d", target.clientIP.c_str(), target.clientPort);
    
    // Send punch packets to create NAT mapping for return traffic
    for (int i = 0; i < 10; i++) {
        ENetBuffer buffer = { (void*)HOLE_PUNCH_MARKER, 4 };
        enet_socket_send(host->socket, &clientAddr, &buffer, 1);
        SDL_Delay(200);
    }
    
    // DO NOT connect - wait for client's ENet connect to arrive
    // ENet will handle incoming connection via enet_host_service()
    SDL_Log("HOST: Punch complete, awaiting client connection");
}
```

### Client Responsibilities

```cpp
// In NetworkManager or MultiPlayerMenu (client side)

void NetworkManager::connectWithHolePunch(const GameServerInfo& gameInfo) {
    // 1. STUN query (pre-connection, safe)
    auto stunResult = StunClient::queryOnSocket(host->socket, ...);
    
    // 2. Request punch from metaserver
    auto clientId = requestPunchRequest(gameInfo.sessionId, stunResult.externalPort);
    
    // 3. Poll for ready signal
    while (!timedOut) {
        auto status = pollPunchStatus(gameInfo.sessionId, clientId);
        if (status.ready) {
            hostAddr = status.hostAddr;
            punchInSeconds = status.punchInSeconds;
            break;
        }
        SDL_Delay(500);
    }
    
    // 4. Wait for coordinated punch time
    SDL_Delay(punchInSeconds * 1000);
    
    // 5. Execute punch AND connect (client initiates connection)
    executePunchAsClient(hostAddr);
}

void NetworkManager::executePunchAsClient(const ENetAddress& hostAddr) {
    // CLIENT: Send punch packets AND initiate ENet connection
    
    SDL_Log("CLIENT: Punching to %s:%d", Address2String(hostAddr).c_str(), hostAddr.port);
    
    // Send punch packets to create NAT mapping
    for (int i = 0; i < 10; i++) {
        ENetBuffer buffer = { (void*)HOLE_PUNCH_MARKER, 4 };
        enet_socket_send(host->socket, &hostAddr, &buffer, 1);
        SDL_Delay(200);
    }
    
    // CLIENT initiates ENet connection (host accepts)
    connectPeer = enet_host_connect(host, &hostAddr, 2, 0);
    if (connectPeer == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "CLIENT: enet_host_connect failed");
        return;
    }
    
    SDL_Log("CLIENT: Punch complete, ENet connect initiated");
    // Normal ENet connection flow continues from here
}
```

### Role Summary

| Action | Host | Client |
|--------|------|--------|
| STUN query | Before announce | Before punch request |
| Register with metaserver | `command=add` with `stun_port` | `command=punch_request` |
| Poll metaserver | `punch_poll` for client requests | `punch_status` for ready signal |
| Signal ready | `punch_ready` | (waits) |
| Send punch packets | Yes | Yes |
| Call `enet_host_connect()` | **NO** | **YES** |
| Accept connection | Via `enet_host_service()` | N/A |

## Metaserver Endpoint Consistency (Rev 4 - CORRECTED)

### Problem (from Rev 3 review)
Flow mentioned "POST /announce" but endpoints didn't define it. Current system uses `command=add`.

### Solution: Extend `command=add` with optional `stun_port`

**Minimal change to existing endpoint:**

```php
// metaserver.php - handleAdd() modification

function handleAdd() {
    // ... existing validation ...
    
    // NEW: Optional stun_port parameter
    $stunPort = isset($_GET['stun_port']) ? intval($_GET['stun_port']) : null;
    if ($stunPort !== null && ($stunPort < 1 || $stunPort > 65535)) {
        echo "ERROR: Invalid stun_port\n";
        return;
    }
    
    // Generate session_id for hole punch signaling
    $sessionId = bin2hex(random_bytes(6));  // 12 hex chars
    
    $game = [
        // ... existing fields ...
        'session_id' => $sessionId,
        'stun_port' => $stunPort ?? $port,  // Fall back to game port if no STUN
    ];
    
    // ... rest of existing logic ...
    
    echo "OK\n";
    echo $secret . "\n";
    echo $sessionId . "\n";  // NEW: Return session_id to host
}
```

**Client change:**
```cpp
// MetaServerClient.cpp - announce with stun_port
parameters["stun_port"] = std::to_string(externalStunPort);

// Parse response
// Line 1: OK
// Line 2: secret
// Line 3: session_id (NEW)
```

### Complete Endpoint List

| Endpoint | Method | Auth | Purpose | Changes in Rev 4 |
|----------|--------|------|---------|------------------|
| `command=add` | GET | None | Register game | + optional `stun_port`, returns `session_id` |
| `command=update` | GET | secret | Update player count | Unchanged |
| `command=remove` | GET | secret | Remove game | Unchanged |
| `command=list` | GET | None | Tab-separated list | Unchanged (backward compat) |
| `command=list2` | GET | None | JSON list | **NEW** - includes `sessionId`, `stunPort` |
| `command=punch_request` | POST | None | Client requests punch | **NEW** |
| `command=punch_poll` | GET | secret | Host polls for requests | **NEW** |
| `command=punch_ready` | POST | secret | Host signals ready | **NEW** |
| `command=punch_status` | GET | session_id+client_id | Client polls for ready | **NEW** |

### Port Validation (Rev 4 - relaxed per review)

```php
// Allow full port range 1-65535 (NATs can map to any port)
// Anti-abuse handled by IP derivation from connection
if ($stunPort < 1 || $stunPort > 65535) {
    echo "ERROR: Invalid port\n";
    return;
}
```

## High-Level Flow (Rev 4)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      HOLE PUNCH COORDINATION (Rev 4)                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  HOST                         METASERVER                        CLIENT      │
│   │                               │                               │         │
│   │── 1. enet_host_create() ─────>│                               │         │
│   │                               │                               │         │
│   │── 2. STUN query ─────────────>│ (on ENet socket,              │         │
│   │      (no peers yet = safe)    │  no peers = safe)             │         │
│   │<─ 3. External stunPort ───────│                               │         │
│   │                               │                               │         │
│   │── 4. command=add&stun_port=X >│                               │         │
│   │<─ 5. OK + secret + session_id │                               │         │
│   │                               │                               │         │
│   │                               │<── 6. command=list2 ──────────│         │
│   │                               │─── 7. JSON [{sessionId,...}] >│         │
│   │                               │                               │         │
│   │                               │                ┌──────────────│         │
│   │                               │                │ 8. STUN query│         │
│   │                               │                │ (no peers)   │         │
│   │                               │                └──────────────│         │
│   │                               │                               │         │
│   │                               │<── 9. punch_request ──────────│         │
│   │                               │    {session_id, stun_port}    │         │
│   │                               │    IP from getRealClientIP()  │         │
│   │                               │                               │         │
│   │<── 10. punch_poll ────────────│                               │         │
│   │    → {client_ip, client_port} │                               │         │
│   │                               │                               │         │
│   │── 11. punch_ready ───────────>│                               │         │
│   │    {secret, client_id}        │                               │         │
│   │                               │                               │         │
│   │                               │<── 12. punch_status ──────────│         │
│   │                               │─── 13. {ready, host_ip:port,  │         │
│   │                               │        punch_in_seconds: 2}  >│         │
│   │                               │                               │         │
│   │         (both wait 2 seconds, then punch simultaneously)      │         │
│   │                               │                               │         │
│   │── 14. Punch packets ─────────────────────────────────────────>│         │
│   │       (NO enet_host_connect)  │                               │         │
│   │                               │                               │         │
│   │<───────────────────────────────────────── 15. Punch packets ──│         │
│   │                               │                 + enet_host_connect()   │
│   │                               │                               │         │
│   │<─────────────── 16. ENet connection (client initiated) ──────>│         │
│   │                               │                               │         │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Implementation Notes

### C++17 Compatibility (from review)
`std::string::starts_with()` is C++20. Use C++17-compatible check:

```cpp
// Instead of: if (result.starts_with("{"))
if (result.length() > 0 && result[0] == '{')
```

### Hole Punch Packet Handling
ENet may log warnings for the 4-byte "DLHP" packets. This is expected:
- Packets are too small to be valid ENet protocol
- ENet discards them safely
- Document in release notes as expected behavior

## Acceptance Criteria

- [ ] Host STUN query completes before announcing (no ENet traffic interference)
- [ ] Client STUN query completes before punch request (no ENet traffic interference)  
- [ ] Host does NOT call `enet_host_connect()` - only punches and waits
- [ ] Client calls `enet_host_connect()` after punching
- [ ] `command=add` accepts optional `stun_port`, returns `session_id`
- [ ] `command=list2` returns JSON with `sessionId` and `stunPort`
- [ ] Old clients using `command=list` continue to work (no hole punch)
- [ ] Connection succeeds for compatible NAT types (~80%)
- [ ] Clear error for incompatible NAT types

## Test Plan

### Unit Tests
- [ ] STUN query on empty ENet host (no peers) succeeds
- [ ] STUN query returns valid external port
- [ ] `command=add` with `stun_port` returns `session_id`
- [ ] `command=list2` includes `sessionId` and `stunPort`

### Integration Tests
- [ ] Full flow: host STUN → add → client list2 → client STUN → punch → connect
- [ ] Host punch does NOT call enet_host_connect
- [ ] Client punch DOES call enet_host_connect
- [ ] Old client (uses `list`) falls back to direct connect

### Manual Tests
- Two machines behind different cone NATs → connects via hole punch
- Host behind NAT, client on open internet → connects (punch optional)
- Both behind symmetric NAT → fails with clear error message

## Rollout / Rollback

Same as Rev 3:
- `EnableHolePunch = false` disables feature
- `command=list` unchanged for old clients
- All new endpoints are additive

## Risks

| Risk | Mitigation |
|------|------------|
| STUN timeout delays server start | 3 second timeout, async possible in future |
| Symmetric NAT still fails | Clear error message, suggest port forward |
| Unexpected packets during STUN | Log and drop (shouldn't happen pre-connection) |

## Resolved Issues

| Rev | Issue | Resolution |
|-----|-------|------------|
| 1 | ICE socket ownership | Dropped ICE, use STUN only |
| 2 | Separate STUN socket wrong port | STUN on ENet socket |
| 2 | `list` field breaks compat | New `list2` endpoint |
| 2 | IP spoofing | Metaserver derives IP |
| 3 | STUN receive drops ENet packets | STUN only pre-connection |
| 3 | Host calls enet_host_connect | Host only punches, client connects |
| 3 | "POST /announce" undefined | Extend `command=add` with `stun_port` |
