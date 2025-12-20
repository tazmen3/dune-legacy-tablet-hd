# NAT Traversal via STUN + Hole Punching - Implementation Log

## State
Branch: `master` (not started)
Commit: N/A

## Current Step
**Rev 3 submitted for re-review.** Addressed all Rev 2 blockers:
1. STUN now uses ENet socket (`host->socket`), not temporary socket
2. New `list2` endpoint for JSON + session_id (keeps `list` stable)
3. Metaserver derives IP from `getRealClientIP()`, only accepts port from client

Awaiting Codex re-review of `design.md` Rev 3.

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
- [ ] Codex re-review of Rev 3
- [ ] Owner approval of revised design
- [ ] No external library needed (STUN client ~150 lines on ENet socket)
