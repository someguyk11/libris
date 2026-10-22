# Requested backend features versus database fields

Source of truth: the finalized Atlas V1 `libris/agents.md` (encoder recording architecture), not the superseded timed-route draft design.

## Finalized V1 fields: implemented

| Requirement | Implementation |
| --- | --- |
| `robot1` identity | `robots.id TEXT` primary key; no UUID robot identity or redundant robot-ID column |
| Last confirmed checkpoint/sequence | Nullable `residing_checkpoint_id`, `last_sequence_number`; historical, not live localization |
| Firmware version | Nullable `firmware_version`, updated only from a reported value |
| Checkpoints/directed neighbors | UUID checkpoint identity and `connected_checkpoint_ids UUID[]`; backend validates elements and direction |
| Optional library location | Nullable UUID `location_id`; see the missing entity relationship below |
| Route identity/robot/endpoints | UUID route, TEXT robot FK, required start/destination checkpoint FKs |
| Route lifecycle/version | `draft/active/disabled`, version 1, nullable verification timestamp |
| Ordered measured movements | JSONB `route_segments`, each with original sequence, checkpoint or null, measured wheel differences and directions |
| Checkpoint sensor snapshots | Optional/null ultrasonic values inside confirmed checkpoint segments, not a telemetry table |
| Timestamps | Creation/update TIMESTAMPTZ for all three Atlas tables |

No agreed V1 permanent field is being silently omitted. No route-step, connection, recording-session, temporary telemetry, or pending-command table is created.

## Real schema/integration gap

**Location relationship:** Libris currently has no UUID location table/model/API. Shelves only have a free-text `section` and numeric `floor`. Consequently `checkpoints.location_id` exists, but has no location FK or entity validation. It must remain null unless the caller knowingly provides metadata; it is not a verified library-location association. When an actual location entity is introduced, add the FK to that existing system rather than creating a second location system now.

## Requested processing state that intentionally has no permanent fields

These are **not missing database features**; the specification explicitly requires them to remain in FastAPI memory:

- Recording IDs, lifecycle and ordered temporary command buffers.
- Pending command UUIDs, delivery/started/completed/acknowledgement state and retry receipts.
- Intermediate encoder results and ultrasonic samples awaiting route finalization.
- Current connection/network telemetry, live command mailbox, pending credentials/reset requests.

One worker is required. A process restart loses unsaved recordings and delivery state; the hardware must stop safely. Persisting these in Supabase would contradict this V1 design.

## Implemented backend contract, but hardware/UI work remains

| Part | Remaining work; not solved by inventing a column |
| --- | --- |
| Real encoder recording | ESP32 must implement command-ID deduplication, started/completed/failed acknowledgements, measured per-wheel differences/directions, and capture-time sensor messages. No firmware source exists here to update or verify. |
| Checkpoint confirmation | Backend supports explicit operator-confirmed observations. There is no automatic checkpoint detector/localization algorithm, and none is invented. |
| Native Supabase storage | SQL migration is prepared; remote application and native PostgreSQL integration tests still require a configured, authorized Supabase target. Local SQLite verification is not remote verification. |
| `last_verified_at` | Field exists but is not set merely by activation; a genuine physical verification workflow has not been implemented. |
| Replay | Exact-route guidance task dispatch and acknowledged segment progression now exist; see [GUIDANCE.md](GUIDANCE.md). Firmware encoder-target control and physical safety testing remain required. Route activation alone never starts replay. |
| Operator workflow UI | Guidance start/arrival checkpoint confirmation and emergency stop are connected in Atlas Monitor. Dedicated recording controls and checkpoint CRUD screens remain to be connected to the implemented APIs. |
| Security | Supabase email OTP/cookie sessions exist. Operational APIs always require trusted staff UUID allowlisting or robot-bound device credentials, even when guidance is disabled. Guidance additionally requires the physical Atlas-displayed code (60 seconds, five attempts), consumed against the owned request before movement. Multi-library roles/tenant isolation remain absent; use a trusted single-installation test environment. See [AUTH.md](AUTH.md) and [GUIDANCE.md](GUIDANCE.md). |

## Older UI requests outside the finalized V1 schema

Battery percentage, operating-mode settings, permanent hardware event/error history, general-purpose task queues, motor/sensor configuration, and shelf category/aisle/row remain outside the finalized V1 database contract. The separate guidance extension adds only its narrow durable task lifecycle and shelf destination mapping; it is not a general scheduler. Unsupported controls must remain unavailable/placeholders. Estimated durations and motor speeds are explicitly prohibited in the permanent route representation.

The separate additive auth migration supplies verified accounts, sessions, rate counters and security events in private `libris_auth`, without changing the Atlas schema. Reader profiles, library roles/tenant isolation, loans, waitlists, bookmarks, collections and progress still need their own implementation; their mock data is not uploaded to any Atlas table.
