# NAT Traversal via STUN + Hole Punching - Implementation Log

## State
Branch: `master` (not started)
Commit: N/A

## Current Step
**Rev 2 submitted for re-review.** Design revised to address all blocking issues:
- Changed from full ICE/libjuice to STUN + coordinated hole punching (Option C)
- Explicit socket ownership model (ENet keeps socket, STUN uses temporary socket)
- Public `session_id` for client signaling (not secret)
- POST endpoints with JSON, explicit size limits
- TTL cleanup, bounded storage, rate limiting specified

Awaiting Codex re-review of `design.md` Rev 2.

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
### Outcome
REJECT (needs design changes before coding).

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
- [x] Update `design.md` per Codex review (blocking issues) - **Done Rev 2**
- [x] Decide ICE/ENet integration option - **Chose Option C (STUN + hole punch)**
- [ ] Codex re-review of Rev 2
- [ ] Owner approval of revised design
- [ ] No external library needed (custom STUN client ~150 lines)
