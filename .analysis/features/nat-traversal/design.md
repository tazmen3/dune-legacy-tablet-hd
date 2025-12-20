# NAT Traversal via libjuice (ICE Protocol)

## Status
PROPOSAL

## Problem / Goal

**Problem:** Internet multiplayer games fail to connect when the host is behind a NAT router without UPnP support or manual port forwarding. Current behavior:
1. Host creates game, UPnP fails (many routers don't support it properly)
2. Game is listed on metaserver with host's external IP
3. Client tries to connect directly to external IP:28747
4. Router drops incoming packets (no port mapping exists)
5. Connection times out

**Goal:** Enable peer-to-peer connections through NAT without requiring users to configure port forwarding, using the ICE protocol (STUN + TURN) via the libjuice library.

## Non-Goals

- Replacing ENet (keep existing reliable UDP transport)
- Implementing our own ICE/STUN/TURN from scratch
- Supporting all possible network configurations (symmetric NAT behind symmetric NAT will still fail without TURN relay)
- Running a TURN relay server (initial implementation uses STUN only; relay is future work)

## Acceptance Criteria

- [ ] Two players behind different NAT routers can connect without port forwarding (when NAT types are compatible)
- [ ] Connection falls back to direct connection if ICE negotiation fails
- [ ] Existing LAN game discovery continues to work unchanged
- [ ] Connection establishment takes <10 seconds in typical cases
- [ ] Clear error messages when connection fails with suggested remediation
- [ ] No regression in existing multiplayer functionality

## Approach

### High-Level Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           CONNECTION ESTABLISHMENT                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  HOST                        METASERVER                        CLIENT       │
│   │                              │                               │          │
│   │──── 1. Create Game ─────────>│                               │          │
│   │      (get session_id)        │                               │          │
│   │                              │                               │          │
│   │<─── 2. ICE Candidates ───────│<──── 3. Request Join ─────────│          │
│   │      (STUN discovery)        │       (get host candidates)   │          │
│   │                              │                               │          │
│   │                              │────── 4. Host Candidates ────>│          │
│   │                              │                               │          │
│   │                              │<───── 5. Client Candidates ───│          │
│   │                              │       (STUN discovery)        │          │
│   │<─── 6. Client Candidates ────│                               │          │
│   │                              │                               │          │
│   │<═══════════════════ 7. ICE Hole Punch ═════════════════════>│          │
│   │                              │                               │          │
│   │<─────────────── 8. ENet Connection (existing) ─────────────>│          │
│   │                              │                               │          │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Components

#### 1. IceAgent Class (new)
Wrapper around libjuice that manages ICE candidate gathering and connection establishment.

```cpp
class IceAgent {
public:
    enum class State { New, Gathering, Complete, Connected, Failed };
    
    IceAgent(bool isControlling);  // Host = controlling, Client = controlled
    ~IceAgent();
    
    // Start gathering local ICE candidates (calls STUN server)
    void gatherCandidates(const std::string& stunServer = "stun.l.google.com:19302");
    
    // Get local description (SDP-like format) to send to peer
    std::string getLocalDescription() const;
    
    // Set remote description received from peer
    void setRemoteDescription(const std::string& sdp);
    
    // Get negotiated address for ENet connection (after ICE completes)
    bool getSelectedAddress(ENetAddress& outAddress) const;
    
    // Callbacks
    std::function<void(State)> onStateChange;
    std::function<void(const std::string&)> onLocalCandidate;  // For trickle ICE
    
    State getState() const;
    
private:
    juice_agent_t* agent = nullptr;
    State state = State::New;
    // ... internal state
};
```

#### 2. Metaserver Signaling API (additions)

New endpoints for ICE candidate exchange:

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `?command=ice_offer&secret=X&sdp=...` | GET | Host sends ICE offer |
| `?command=ice_answer&secret=X&peer=Y&sdp=...` | GET | Client sends ICE answer |
| `?command=ice_poll&secret=X` | GET | Poll for peer candidates |

Data stored per-game on metaserver:
```php
$gameServers[$secret]['ice'] = [
    'host_sdp' => '...',           // Host's ICE candidates
    'pending_peers' => [           // Clients waiting to connect
        'peer_id' => ['sdp' => '...', 'timestamp' => ...]
    ]
];
```

#### 3. NetworkManager Modifications

Add ICE negotiation to `connect()` flow:

```cpp
void NetworkManager::connect(ENetAddress address, const std::string& playerName) {
    // NEW: If this is an internet game, try ICE first
    if (isInternetGame && !iceAgent) {
        iceAgent = std::make_unique<IceAgent>(false);  // Client = controlled
        iceAgent->onStateChange = [this](IceAgent::State state) {
            handleIceStateChange(state);
        };
        
        // Start ICE negotiation via metaserver
        requestIceCandidatesFromMetaserver(gameServerInfo);
        return;  // Will call continueConnect() when ICE completes
    }
    
    // EXISTING: Direct ENet connection
    connectPeer = enet_host_connect(host, &address, 2, 0);
    // ...
}

void NetworkManager::handleIceStateChange(IceAgent::State state) {
    if (state == IceAgent::State::Connected) {
        ENetAddress negotiatedAddress;
        if (iceAgent->getSelectedAddress(negotiatedAddress)) {
            // ICE succeeded - connect via negotiated address
            continueConnect(negotiatedAddress);
        }
    } else if (state == IceAgent::State::Failed) {
        // ICE failed - try direct connection as fallback
        continueConnect(originalAddress);
    }
}
```

#### 4. Host-Side Flow (CustomGamePlayers / startServer)

```cpp
void NetworkManager::startServer(...) {
    // EXISTING: Create ENet host
    // EXISTING: UPnP attempt
    
    // NEW: Start ICE agent for incoming connections
    if (!bLANServer) {
        hostIceAgent = std::make_unique<IceAgent>(true);  // Host = controlling
        hostIceAgent->gatherCandidates();
        hostIceAgent->onStateChange = [this](IceAgent::State state) {
            if (state == IceAgent::State::Complete) {
                // Send ICE offer to metaserver
                sendIceOfferToMetaserver(hostIceAgent->getLocalDescription());
            }
        };
    }
    
    // EXISTING: Announce to metaserver
}
```

### STUN Server

Use public STUN servers initially:
- `stun.l.google.com:19302` (Google, reliable)
- `stun.cloudflare.com:3478` (Cloudflare, backup)

Future: Run our own STUN server on dunelegacy.com for reliability.

### Fallback Strategy

```
1. Try ICE negotiation (5 second timeout)
   ├── Success → Use negotiated address with ENet
   └── Failure → 
       2. Try direct connection to external IP (3 second timeout)
          ├── Success → Connected (UPnP/port forward worked)
          └── Failure →
              3. Show error: "Could not connect. Host may need to enable UPnP 
                 or forward port 28747. Alternatively, try Tailscale/ZeroTier."
```

## Interfaces / Touch Points

### Files Modified

| File | Changes |
|------|---------|
| `include/Network/IceAgent.h` | **NEW** - IceAgent class declaration |
| `src/Network/IceAgent.cpp` | **NEW** - IceAgent implementation |
| `include/Network/NetworkManager.h` | Add IceAgent member, ICE-related methods |
| `src/Network/NetworkManager.cpp` | ICE negotiation in connect/startServer flows |
| `src/Network/MetaServerClient.cpp` | ICE signaling methods (send/poll candidates) |
| `dunelegacy.com/metaserver/metaserver.php` | ICE signaling endpoints |
| `CMakeLists.txt` | Add libjuice dependency |
| `vcpkg.json` | Add libjuice dependency |

### External Dependencies

| Dependency | Version | License | Purpose |
|------------|---------|---------|---------|
| libjuice | 1.3+ | LGPL-2.1 or MPL-2.0 | ICE/STUN implementation |

Check vcpkg availability:
```bash
vcpkg search libjuice
```

### API Contracts

#### IceAgent → libjuice
- `juice_create()` / `juice_destroy()`
- `juice_gather_candidates()`
- `juice_get_local_description()` / `juice_set_remote_description()`
- `juice_get_selected_candidates()`
- Callbacks: `on_state_changed`, `on_candidate`, `on_gathering_done`

#### Metaserver ICE Signaling
- Request: `?command=ice_offer&secret=X&sdp=<url_encoded_sdp>`
- Response: `OK` or `ERROR: <message>`
- Request: `?command=ice_poll&secret=X`
- Response: `OK\n<peer_id>\t<sdp>\n...` or `OK\n` (no pending peers)

## Test Plan

### Unit Tests
- [ ] `IceAgent` state machine transitions
- [ ] SDP parsing/serialization
- [ ] Metaserver ICE signaling endpoints

### Integration Tests
- [ ] Two clients on same LAN (should use direct, skip ICE)
- [ ] Two clients on different LANs with compatible NAT (ICE should succeed)
- [ ] ICE timeout fallback to direct connection
- [ ] Metaserver ICE candidate exchange

### Manual Tests

| Scenario | Steps | Expected Result |
|----------|-------|-----------------|
| Compatible NAT | Host on Network A (behind NAT), Client on Network B (behind NAT) | Connection established via ICE |
| ICE Fail + UPnP | Host has UPnP, ICE disabled | Falls back to direct, connects |
| Both Fail | No UPnP, symmetric NAT | Clear error message |
| LAN Game | Both on same network | Uses LAN discovery, ICE skipped |

### Test Commands
```bash
# Build
cd dunelegacy/build && cmake --build . -j8

# Run game (manual testing)
./bin/dunelegacy.app/Contents/MacOS/dunelegacy

# Check logs for ICE
tail -f "~/Library/Application Support/Dune Legacy/Dune Legacy.log" | grep -i ice
```

## Tools / Prereqs

### Build Dependencies
```bash
# Add libjuice to vcpkg.json
# vcpkg will install automatically on next cmake configure

# Verify installation
ls build/vcpkg_installed/*/include/juice/
```

### vcpkg.json Addition
```json
{
  "dependencies": [
    // ... existing ...
    "libjuice"
  ]
}
```

### CMakeLists.txt Addition
```cmake
find_package(LibJuice CONFIG REQUIRED)
target_link_libraries(dunelegacy PRIVATE LibJuice::LibJuice)
```

## Rollout / Rollback

### Rollout Plan
1. **Phase 1**: Implement IceAgent + metaserver signaling (disabled by default)
2. **Phase 2**: Enable for internet games with feature flag in `Dune Legacy.ini`
   ```ini
   [Network]
   UseIceNatTraversal = true
   ```
3. **Phase 3**: Enable by default after testing, keep flag for disable
4. **Phase 4**: Remove flag, always-on

### Rollback
- Set `UseIceNatTraversal = false` in ini to disable
- Or revert commits (ICE code is additive, not replacing ENet)

### Feature Flag Implementation
```cpp
// In Settings class
bool useIceNatTraversal = true;  // Default on after Phase 3

// In NetworkManager::connect()
if (settings.network.useIceNatTraversal && isInternetGame) {
    // Try ICE
} else {
    // Direct connection only
}
```

## Risks / Open Questions

### Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| libjuice not in vcpkg | Low | Medium | Build from source, add to vcpkg overlay |
| STUN server unreliable | Medium | Low | Multiple fallback STUN servers |
| Symmetric NAT fails | High | Medium | Clear error message, suggest TURN/Tailscale |
| ICE adds latency to connect | Medium | Low | 5s timeout, async negotiation |
| Metaserver overload from polling | Low | Medium | Rate limit, use websockets future |

### Open Questions

1. **TURN Relay**: Should we implement TURN relay for symmetric NAT? (deferred to future)
   - Would require running a TURN server (~$5-10/mo)
   - Or use free public TURN (unreliable, bandwidth limited)

2. **ICE Trickling**: Should we trickle candidates or gather all first?
   - Gather-all is simpler, slightly slower
   - Trickling is faster but more complex signaling
   - **Decision**: Start with gather-all, optimize later

3. **Candidate Types Priority**: 
   - Host candidates (local) → Server reflexive (STUN) → Relay (TURN)
   - libjuice handles this automatically

4. **iOS Compatibility**: Does libjuice work on iOS for future port?
   - Yes, libjuice supports iOS
   - May need socket permission adjustments

5. **Existing Keep-Alive**: Does the recently added KEEPALIVE packet interfere?
   - No, ICE negotiation happens before ENet connection
   - KEEPALIVE is for after connection is established

## Appendix: libjuice API Example

```c
#include <juice/juice.h>

juice_config_t config = {
    .stun_server_host = "stun.l.google.com",
    .stun_server_port = 19302,
    .cb_state_changed = on_state_changed,
    .cb_candidate = on_candidate,
    .cb_gathering_done = on_gathering_done,
    .user_ptr = this
};

juice_agent_t *agent = juice_create(&config);
juice_gather_candidates(agent);

// After gathering done, get local SDP
char sdp[JUICE_MAX_SDP_STRING_LEN];
juice_get_local_description(agent, sdp, sizeof(sdp));

// Send sdp to peer via metaserver signaling
// Receive remote sdp from peer
juice_set_remote_description(agent, remote_sdp);

// Wait for state == JUICE_STATE_CONNECTED
// Get selected candidate pair
juice_get_selected_candidates(agent, local, sizeof(local), remote, sizeof(remote));

// Parse remote to get IP:port for ENet
// Connect ENet to that address
```

