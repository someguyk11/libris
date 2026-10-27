# Atlas guidance authorization — protocol v2

This is shelf guidance, **not book checkout**. Atlas display codes and the reader code-entry step are retired. Email verification and account password recovery still use Supabase email codes. Movement remains protected by reader ownership, CSRF, keyed devices and physical-readiness checks.

## Deployment prerequisites

- Configure Supabase Auth, numeric email verification, SMTP and secure sessions as described in `AUTH.md`.
- Set `GUIDANCE_ENABLED=true`, `AUTH_STAFF_USER_IDS` to a comma-separated list of verified staff account UUIDs, and `ATLAS_DEVICE_KEYS` to a JSON object mapping each robot ID to an independently generated secret of at least 32 characters. Never expose these secrets through `VITE_*` variables.
- Use HTTPS and preferably serve `/api` through the same origin as Vue. Browser mutations require the session cookie, allowed `Origin`, and `X-CSRF-Token`.
- Run **one backend worker**. Temporary deliveries and physical attestations are in memory. A restart fails outstanding requests and marks location unknown; replay never resumes automatically.
- Configure a home checkpoint, a shelf destination, and separately recorded, active, validated routes **home → shelf** and **shelf → home**, belonging to that robot. Do not reverse encoder segments to invent a return route.
- Firmware must implement protocol **2** below. Protocol 1 is rejected. The firmware connectivity client in this workspace does not implement full protocol-2 measured replay; physical replay/watchdog behavior is not verified by these backend tests.

Catalogue and shelf reads remain public. Operational writes and robot administration now require verified staff authorization even when guidance is disabled. Disabling `GUIDANCE_ENABLED` disables guidance, **not authentication**. Device polling/results use only the matching device key; browser sessions cannot substitute for it. Staff access currently uses a server UUID allowlist, not production multi-library roles. Tenant isolation remains unfinished: do not deploy this as a public multi-library service.

## Lifecycle

1. A signed-in, email-verified reader selects a live catalogue book and presses **Guide me**. Search never moves Atlas.
2. The backend validates shelf mapping and both recorded routes, and creates a request in `waiting_for_atlas`. One active request per reader is enforced. Other readers queue FIFO for the selected robot, including while it guides or returns.
3. Only a keyed, stopped robot at its configured, physically confirmed home can claim the next request. The backend atomically creates one unique session UUID and moves the request directly to `queued`, without a code or extra reader action. Recording, other active commands and invalid routes still prevent dispatch.
4. The same poll may deliver the first outbound replay segment. Repeated/concurrent polls reuse the same session and deterministic command ID, never create another session or authorize duplicate execution.
5. At the shelf, completed encoder results are insufficient proof of location. Staff must physically attest the actual checkpoint, and the keyed ESP32 must send the matching stopped arrival report. The accepted arrival automatically switches to `returning_home` and the separately recorded return route.
6. Atlas remains unavailable throughout its return. Only completed return measurements, stopped firmware, physical home attestation and the matching ESP32 arrival report complete the request and restore `at_home`. The next poll may then assign the next FIFO request.

Robot states are `unknown`, `at_home`, `guiding`, `returning_home`, and `fault`. Request states are `waiting_for_atlas`, `queued`, `awaiting_robot`, `running`, `returning_home`, and terminal `completed`, `cancelled`, `expired`, `failed`.

Cancellation is available before device dispatch. A fault during travel never fabricates home arrival. Staff must recover the robot and reconfirm actual home placement before further assignments. Waiting requests expire after 15 minutes so abandoned requests cannot block indefinitely.

## HTTP API

### Verified reader — cookie + CSRF on mutations

- `POST /guidance-requests` with `{ "book_id": 123 }`: create a waiting request; repeat requests for the same active book return the existing request.
- `GET /guidance-requests`: own recent requests only.
- `GET /guidance-requests/{id}`: own request status only.
- `POST /guidance-requests/{id}/cancel`: allowed while waiting or queued but not yet dispatched. Once offered to firmware, ask staff for an emergency stop instead.

Normal status includes phase/progress, robot and checkpoint IDs, session ID and failure reason, never device credentials. The retired Atlas verification endpoint is no longer exposed.

### Verified staff — cookie + CSRF on mutations

- `PATCH /shelves/{id}/destination` with `{ "destination_checkpoint_id": "UUID-or-null" }`.
- `PATCH /robots/{id}/home` with `{ "home_checkpoint_id": "UUID" }`: no outstanding requests allowed; changing home invalidates placement trust.
- `GET /robots/{id}/guidance-tasks`: recent requests and progress.
- `POST /robots/{id}/guidance-stop`: fail the active authorization and request stop. This is **not proof motors stopped**.
- `POST /robots/{id}/checkpoint-confirmation` with `{ "checkpoint_id": "UUID", "operator_confirmed": true }`: initial physical home placement while keyed firmware reports stopped.
- During an active route, include `task_id` in checkpoint confirmation. This records temporary physical arrival evidence only; the ESP32 must still report matching arrival. Only the completed current route's destination can be attested.

Existing Atlas Monitor and shelf editor provide these controls. Home/return routes use existing checkpoint and encoder-route APIs. A comprehensive recording/checkpoint editor is not added.

### ESP32 — `X-Atlas-Key: <this-robot-secret>`

Poll `GET /robot/{id}/guidance/command?protocol=2&stopped=true` (report actual stopped state, not a constant). The path robot must match the supplied key.

When ready, segment responses include `command: replay_segment`, `task_id`, `session_id`, current `route_id`, phase (`guidance` or `return`), deterministic `command_id`, ordered `sequence`, independent wheel `movement`, `execute`, `lease_ms: 1500`, and `segment_timeout_ms: 60000`. Otherwise polling returns stop; there is no code-display command.

POST `/robot/{id}/guidance/result` with these exact IDs/sequence plus `status` (`accepted`, `started`, `completed`, `failed`). `completed` requires the actual measured `movement`, matching the recorded target; a `started` acknowledgement is mandatory. Do not fabricate measured results from command targets. Backend comparison is currently exact; real encoder tolerances require explicit design and physical calibration before rollout.

Command IDs bind session, route and sequence. Persist/deduplicate before actuation; repeat polls resend the same ID until acknowledged. `execute: false` may renew only an already-known active command, never start an unknown command. Repeated completion is idempotent; conflicting measurements or late fault reports fail the active task. Stale/wrong session, robot, route or command context never advances progress. Firmware must never execute a command twice or restart a session after it has stopped or expired.

Poll at least every 500 ms while active. A gap beyond the **1.5-second delivery lease** fails the task before a reconnect can resume it; without an active segment, heartbeat timeout is 5 seconds. Firmware must independently stop on lease expiry, obstacles, encoder/controller faults, malformed/unauthorized HTTP responses, connectivity loss or unexpected session changes. Backend sweeps run every 0.5 seconds; they cannot physically stop a disconnected robot. A segment may run at most 60 seconds; preparation/arrival confirmation may wait at most 120 seconds; overall requests are bounded to 15 minutes. There is no automatic retry after a safety failure.

After a route finishes, polling returns stop plus `task_id`, `session_id`, `route_id`, `checkpoint_id`, `awaiting_confirmation: true`, and whether current staff evidence is present. After stopping and staff physical attestation, POST `/robot/{id}/guidance/arrival`:

```json
{
  "task_id": "UUID",
  "session_id": "UUID",
  "route_id": "UUID",
  "checkpoint_id": "UUID",
  "stopped": true,
  "confirmation_method": "operator_attestation"
}
```

Outbound arrival changes phase to return; home arrival completes the session. Matching duplicate arrival reports are acknowledged without starting another return or completing another request. Encoder completion, elapsed time, ultrasonic similarity or a generic `AT_HOME` flag alone are not accepted as physical checkpoint proof.

## Data and migration

Reuse existing books, shelves, robots, checkpoints, encoder routes and `guidance_tasks`; no separate account/session/command/recording entities are introduced for this flow. Additions are:

- `robots.home_checkpoint_id` and `robots.guidance_state`.
- Existing task: return-route reference/fingerprint and unique `session_id`.
- Expanded request status checks and active-user/claimed-robot partial unique indexes. Waiting requests do not claim the robot.

Startup `upgrade_guidance()` applies `migrations/20261027_guidance_direct_start.sql` on existing PostgreSQL guidance tables. It supports guidance v1/v2, removes the retired code columns/check, replaces active indexes, cancels pending code requests and marks their robots unknown for safe reconfirmation. Valid existing sessions and return routes survive; sessionless active tasks fail safely. SQLite rebuilds only the task table with equivalent handling. Books, shelves, checkpoints and recorded routes are not reset. The old v2 SQL remains only as historical provenance. No remote database migration was run by this coding session; deploy with robots stopped and reconfirm placement before use.

## Verification and remaining work

Automated tests cover direct dispatch without codes, request ownership, cancellation, concurrency/single session, FIFO while busy/returning, strict replay identities/results, duplicate handling, disconnect, return faults and physical home gating, plus real cookie/CSRF/device-key boundaries with mocked Supabase.

No physical sensor calibration, live Supabase/SMTP or native PostgreSQL deployment has been verified. There is no unique checkpoint-recognition hardware in this workspace. The strongest available fallback is an accountable staff physical attestation plus authenticated, contextual ESP32 stopped arrival and completed measured route. It is intentionally **not autonomous localization**. Before public operation implement firmware dedup/encoder feedback/obstacle stop/watchdog/arrival reporting, physically test both routes, verify real stop behavior and an independent physical emergency stop, and complete library-scoped authorization.
