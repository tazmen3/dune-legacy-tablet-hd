# NAT Traversal via STUN + Hole Punching - Implementation Log

## State
Branch: `master` (not started)
Commit: N/A

## Current Step
**Rev 4 submitted for re-review.** Addressed all Rev 3 blockers:
1. STUN only pre-connection when no peers exist (safe - no packet interleaving)
2. Explicit host/client punch roles (host punches only, client punches + connects)
3. `command=add` extended with optional `stun_port`, returns `session_id`

Also addressed non-blocking notes:
- Port range 1-65535 (not 1024+)
- C++17 compatible string check (not starts_with)

Awaiting Codex re-review of `design.md` Rev 4.

## How To Validate
Design phase - no code to validate yet.

After implementation begins:
```bash
# Build (no new dependencies - STUN client is custom)
cd dunelegacy/build && cmake --build . -j8

# Test STUN client
./bin/dunelegacy.app/Contents/MacOS/dunelegacy --test-stun

# Run and check hole punch logs
./bin/dunelegacy.app/Contents/MacOS/dunelegacy
# Create Internet Game, check logs for STUN/punch messages
tail -f "~/Library/Application Support/Dune Legacy/Dune Legacy.log" | grep -iE "stun|punch"
```

## Changes Made
None yet.

## Tests Added / Updated
None yet.

## Review Notes (Codex)
### Rev 3 Outcome
REJECT (much closer; remaining issues are fixable but correctness-critical).

### Rev 3 Blocking Issues
1. **The STUN receive story is still incorrect/dangerous.** Rev 3 says STUN runs on `host->socket` (good), but the proposed `enet_socket_receive()` loop cannot “leave non‑STUN packets in the socket buffer for ENet”. Once you `receive()`, the datagram is consumed. If this runs while ENet traffic exists, you will drop real ENet packets and/or race `enet_host_service()`.
   - Required: pick one safe mechanism and document it precisely:
     - **Preferred:** use ENet’s intercept callback (`host->intercept`) to capture STUN responses while continuing to service ENet; consume only STUN packets.
     - **Acceptable:** guarantee STUN queries only occur when no ENet traffic can exist (pre-connection / no peers) and suspend `enet_host_service()` during the STUN query window.
2. **Host vs client responsibilities during punching need to be explicit.** The current `executeHolePunch()` snippet ends with `enet_host_connect()`, which is client-side behavior. The host should generally *not* call `enet_host_connect()`; it should only send punch packets and then accept the incoming ENet connect.
   - Required: spell out “client does X, host does Y” and where each is triggered in the existing code paths.
3. **Metaserver endpoint consistency:** the flow mentions “POST /announce”, but the endpoint list does not define it, and the current system uses `command=add`/`update` via query params. Decide whether:
   - `command=add` accepts optional `stun_port` (recommended; minimal change), or
   - you introduce a new JSON-based announce endpoint and update the client accordingly.

### Rev 3 Non-blocking Notes
- `stunPort` range check: consider allowing `1–65535` (not `1024+`) since NATs can theoretically map to low ports; the anti-abuse story is already handled by “IP derived from connection”.
- The `list2` selection snippet uses `starts_with` which is C++20; Dune Legacy is C++17—use a C++17-friendly check in implementation.

### Rev 2 Outcome
REJECT (design is close, but two core assumptions are still wrong/unsafe).

### Rev 2 Blocking Issues
1. **STUN on a separate socket likely returns the wrong external port.** The design says STUN uses a temporary socket and “binds to same local port as ENet if possible”, but ENet is already bound (clients use `settings.network.serverPort`), so this will usually be impossible. If STUN is not performed on the *same UDP socket/port* that will send/receive gameplay traffic, the discovered external `IP:port` may not match ENet’s NAT mapping and hole punching will fail.
   - Required: define a STUN query path that uses ENet’s socket (`host->socket`) for the Binding Request/Response, or restructure startup so the same local port is used deterministically without competing binds.
2. **Adding `session_id` as a 13th `list` field breaks compatibility.** Current client parsing expects 9–12 tab-separated fields; a 13th field requires code changes in this repo (fine), but older shipped clients will likely reject the response. This is avoidable.
   - Required: keep `command=list` output stable and add a new endpoint (e.g. `command=list2` or `command=list_json`) for the new fields (`session_id`, `external_port`, etc.), or otherwise provide a backward-compatible mechanism.
3. **Signaling allows IP spoofing / abuse unless metaserver overwrites IPs.** `client_addr`/`host_addr` are client-provided strings. If accepted as-is, an attacker can make the host send UDP to arbitrary victims.
   - Required: metaserver must derive IP from `getRealClientIP()` and only accept the *port* (from STUN) from the client/host; reject mismatched IPs and enforce tight validation.

### Rev 2 Non-blocking Notes
- The hole-punch “DLHP” raw UDP packet is fine for NAT mapping creation, but it may generate ENet “invalid packet” noise; call that out and ensure it cannot destabilize ENet processing.
- Prefer `punch_in_seconds` over absolute `punch_at` to avoid clock skew, as already noted in open questions.
- Rate limiting via APCu is environment-dependent; specify the fallback concretely (even if “none for v1”).

### Blocking issues
1. **ICE/ENet integration is not defined (likely incorrect as written).** The design assumes “ICE negotiates an address, then ENet connects to it”. ICE hole punching typically depends on the *same UDP socket/port mapping* that ran connectivity checks; switching to a different socket (ENet’s) can invalidate the NAT mapping. The design must specify exactly how packets flow during negotiation and how ENet reuses (or is layered on) the ICE-established transport.
2. **Metaserver API uses `secret`, but clients don’t have it.** The current server list response does not include `secret` (only IP/port/name/etc). Any client→metaserver signaling must use a public game/session identifier, not the host’s secret, and must prevent arbitrary third parties from injecting/polling payloads.
3. **SDP/candidate payload size + transport mismatch.** Current metaserver input sanitization truncates to 255 chars; SDP/local descriptions are larger. Current client HTTP helper only supports GET query params; URLs will exceed safe limits. Design must specify POST (or another transport) + explicit payload size limits + storage/TTL.
4. **Abuse/rate-limit and cleanup plan missing.** Candidate exchange introduces new write/poll endpoints. Need: per-session TTL cleanup, bounded storage, per-IP rate limiting, and validation of payloads (size/content). Otherwise `servers.dat` can balloon and polling can become a DoS vector.

### High-impact design questions (answer in `design.md`)
- What is the *exact* socket ownership model?
  - Option A: libjuice drives a UDP socket and ENet must be adapted to use that same socket/port (ENet changes likely).
  - Option B: ENet owns the socket; ICE uses callbacks to send/recv on ENet’s socket and ENet must demux ICE vs ENet traffic during negotiation.
  - Option C: Don’t do ICE: implement STUN + coordinated UDP hole punching on the existing ENet socket (smaller scope, still no TURN).
- What is the public identifier that clients use for signaling, and how is it obtained from `list` results?
- What are the exact metaserver endpoints (method, params/body, responses), size limits, and TTLs?

### Non-blocking suggestions
- Put STUN servers behind config (`Dune Legacy.ini`) with sane defaults; avoid hard-coding to Google.
- Define timeouts/retries per phase (gather, exchange, punch, fallback) and how they surface in UI/logs.
- Clarify privacy implications: local candidates may leak LAN IPs; decide if/when to redact host candidates.

## Open Issues / Follow-ups
- [x] Update `design.md` per Codex review (Rev 1 → Rev 2)
- [x] Decide ICE/ENet integration option - **Option C (STUN + hole punch)**
- [x] Update Rev 2 per Codex blockers - **Done Rev 3**
  - [x] STUN on ENet socket (not temp socket)
  - [x] New `list2` endpoint for backward compat
  - [x] Metaserver derives IP, only accepts port
- [x] Update Rev 3 per Codex blockers - **Done Rev 4**
  - [x] STUN only pre-connection (no packet interleaving)
  - [x] Explicit host/client punch roles
  - [x] `command=add` extended (not new endpoint)
- [ ] Codex re-review of Rev 4
- [ ] Owner approval of revised design
- [ ] No external library needed (STUN client ~150 lines on ENet socket)
