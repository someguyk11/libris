# Atlas V1: encoder routes, checkpoints, and temporary recordings

Implements the finalized `libris/agents.md` specification. This replaces the earlier UUID-robot/timed-command-draft design. No book/shelf or other unrelated Libris tables are reset.

## Migration status

- Prepared PostgreSQL/Supabase SQL: `migrations/20261018_atlas_v1_encoder.sql`.
- Applied locally to the **existing SQLite development database**, using its JSON/UUID compatibility types. This is not a new production database.
- **Not applied or verified on remote Supabase.** No remote target is configured in `backend/.env`; the example connection string is a placeholder. PostgreSQL JSONB/UUID[]/TIMESTAMPTZ model DDL is compiled in tests, not claimed as a live PostgreSQL test.
- The SQL and `migrate.py` replace only the obsolete Atlas tables when they are empty. Nonempty legacy tables fail explicitly because timed durations cannot be converted into measured encoders. No legacy values are fabricated. Migration is idempotent on the finalized schema.
- Version: `20261018_atlas_v1_encoder_v2`. `schema_migrations` is migration bookkeeping, not a recording/session table.

To apply to Supabase: stop the backend, review the SQL, back up if needed, then run it in Supabase SQL Editor using the schema owner, or configure the real server-only `DATABASE_URL` and run `python migrate.py`. Start one backend worker afterward. Never use frontend `VITE_*` variables for database credentials.

## Final permanent tables

### `robots`

| Column | PostgreSQL type | Meaning |
| --- | --- | --- |
| `id` | TEXT PK | Exact firmware ID, e.g. `robot1`; not a UUID |
| `name` | TEXT | Display name |
| `residing_checkpoint_id` | UUID, nullable FK | Last explicitly confirmed checkpoint, not continuous localization |
| `last_sequence_number` | INTEGER, nullable | Recording sequence where that observation was confirmed |
| `firmware_version` | TEXT, nullable | Only stored when reported, never invented |
| `created_at`, `updated_at` | TIMESTAMPTZ | UTC metadata |

Historical checkpoint/sequence values survive restart. They do not prove current physical position after movement, shutdown, or connectivity loss. Runtime status explicitly separates `checkpoint_location_verified` from historical metadata. No command mailbox, Wi-Fi password, temporary telemetry, battery, or motor-speed columns are added.

### `checkpoints`

`id UUID PK`, `name TEXT`, `location_id UUID NULL`, `connected_checkpoint_ids UUID[]`, `is_active BOOLEAN DEFAULT TRUE`, and creation/update TIMESTAMPTZ columns.

Connections are directional. `C02 → C03` does not create `C03 → C02`. The backend checks every referenced array element; PostgreSQL does not enforce element foreign keys. Deactivation removes inbound connections and disables active routes whose endpoints, intermediate visits, or directed transitions become invalid. DELETE is a documented **soft deletion** so historical route and robot references remain valid; no dangling UUID references are created.

No existing Libris location entity/table was found (shelf `section` is free text). `location_id` is therefore nullable UUID metadata **without a fabricated location table or FK**. See `SCHEMA_GAPS.md`.

### `routes`

`id UUID PK`, `robot_id TEXT FK robots.id`, `name TEXT`, `start_checkpoint_id UUID FK`, `destination_checkpoint_id UUID FK`, `status TEXT`, `recording_version INTEGER`, `route_segments JSONB`, `last_verified_at TIMESTAMPTZ NULL`, and creation/update TIMESTAMPTZ columns.

- Status is only `draft`, `active`, or `disabled`; format version is `1`.
- The route UUID doubles as the source recording UUID for retry-safe saves; no permanent recording table or extra recording-ID column is needed.
- The whole ordered encoder array is stored directly in JSONB. No `route_steps`, `route_segments`, or checkpoint-connection join table exists.
- No estimated duration, speed, internal command UUID, continuous sensor stream, or absolute encoder position is persisted in a route.
- `active` means checkpoint/encoder data passed validation. It does not execute replay or certify a physically verified trajectory. `last_verified_at` remains NULL without a genuine verification workflow.

Segment shape (values must come from actual measurements):

```json
{
  "sequence": 1,
  "checkpoint_id": null,
  "movement": {
    "left_encoder_ticks": 520,
    "right_encoder_ticks": 518,
    "left_direction": "forward",
    "right_direction": "forward"
  }
}
```

Directions are `forward`, `backward`, or `stop`, independently per wheel. Tick differences are nonnegative integers; a stopped wheel cannot have nonzero ticks. Sequences are positive and strictly increasing in original array order, never sorted or grouped by checkpoint. Repeated checkpoint visits are valid. Only confirmed checkpoint segments may contain `checkpoint_sensor_readings` with nullable/omitted `front_cm`, `back_cm`, `left_cm`, `right_cm`. Invalid measurements must be sent as null/omitted, not guessed or defaulted.

## Temporary state and reliable delivery

`atlas_runtime.py` holds per-robot state, `active_recordings`, separate `pending_commands`, and bounded acknowledgement receipts. Use **one Uvicorn worker**. Restart loses all temporary commands, credentials, samples, and unsaved recordings; firmware must stop on lost communication. No permanent tables are used to hide these buffers.

Each command has an internal UUID, exact robot/recording association, local recording sequence, legacy transport sequence, requested movement, delivery/execution status, measured result, and timestamp-keyed samples. Only ESP32 execution results mark commands completed. Sending or polling a command does not imply completion.

Repeated polls return the **same command UUID and transport sequence**, not a new movement. Until a `started` acknowledgement arrives the server may redeliver that ID, recovering a lost response. The ESP32 **must deduplicate command IDs before actuating**, then retry acknowledgements/results without running the motors again. After `started`, repeated polls use `execute: false`. Matching duplicate results are acknowledged; conflicting results, wrong robots, wrong recordings, and wrong sequences are rejected. HTTP alone cannot guarantee exactly-once physical actuation; no firmware implementation was found in this workspace.

Outside recordings, the existing website's hold/release mailbox is retained. Superseding a manual command is labelled `superseded`, never `completed`. During recording, a different non-stop movement waits for the outstanding result; Stop may interrupt for safety, and both execution results must still arrive. Motor loops and emergency stopping remain on the ESP32. Pending movements expire to a Stop after one second without refresh; this cannot stop disconnected motors without a firmware watchdog.

## Checkpoint confirmation basis

There is no existing automatic checkpoint detector/localization algorithm. The smallest explicit V1 basis is an operator-confirmed observation of a **completed, measured command**, sent with `confirmation_basis: "operator_confirmed"`. Completion or a similar ultrasonic pattern alone never confirms location.

To activate a recorded path, record/acknowledge an initial stationary observation at the starting checkpoint and explicitly confirm it, then confirm subsequent actual visits. First and final confirmed segment checkpoints must equal the metadata endpoints. Consecutive distinct visits must follow active directed connections. Repeated visits such as C02 → C03 → C05 → C03 are preserved independently.

If a snapshot is wanted, select its exact `sensor_captured_at` timestamp from the correct command. The backend keys readings by command ID, recording ID, sequence, and capture timestamp—not HTTP arrival order. Intermediate readings remain temporary and are removed after save. A selected checkpoint snapshot that has not arrived blocks save and retains the buffer; no values are guessed. Trusted manual JSON imports explicitly assert already-measured/confirmed data and are saved as drafts by the website, not as fabricated live readings.

## API contract

All endpoints below are implemented; the existing firmware response envelope `{status: 200, data: ...}` and legacy `command`, `sequence`, `factory_reset`, `new_network` fields remain. The updated firmware must also implement the added identity/acknowledgement fields.

| Method | Path | Purpose |
| --- | --- | --- |
| GET / POST | `/robots/` | List/register text IDs; `{id, name}`; legacy registration alias `{robot_id, name}` accepted |
| PATCH | `/robots/{robot_id}` | Rename; identity is stable |
| GET / POST | `/checkpoints/` | List/create checkpoints |
| GET / PUT / DELETE | `/checkpoints/{uuid}` | Read/update/soft-deactivate |
| GET / POST | `/robots/{robot_id}/routes` | Retrieve/import JSON encoder routes |
| GET / PUT / PATCH / DELETE | `/routes/{uuid}` | Retrieve/replace/rename or change status/delete |
| POST | `/robot/{robot_id}/command` | `{command, request_id?, recording_id?}`; internal ID + sequence returned |
| GET | `/robot/{robot_id}/command` | Firmware delivery; same ID on retries; includes `execute`, `recording_id`, `recording_sequence` |
| POST | `/robot/{robot_id}/command/result` | Execution started/completed/failed and measured differences |
| POST | `/robot/{robot_id}/sensors` | Temporarily associate a capture-time sample with an exact command |
| POST | `/robot/{robot_id}/recordings` | Start `{start_checkpoint_id}`; one active recording per robot |
| GET | `/recordings/{uuid}` | Inspect ordered temporary commands/results |
| POST | `/recordings/{uuid}/checkpoints` | Explicit operator confirmation for `{sequence, checkpoint_id, confirmation_basis, sensor_captured_at?}` |
| POST | `/recordings/{uuid}/finalize` | Stop accepting movement and queue a safe stop |
| POST | `/recordings/{uuid}/save` | Finalize/validate/save `{name, destination_checkpoint_id, status: draft|active}` |
| DELETE | `/recordings/{uuid}` | Stop and discard only after execution results; may return 202 while waiting |

Legacy `/telemetry`, `/status`, `/reconfigure`, `/reset` remain. Network telemetry is temporary; optional reported firmware version updates the permanent robot identity. Status reads never expose passwords or fabricate battery values.

Execution result body:

```json
{
  "command_id": "UUID_FROM_POLL",
  "recording_id": "UUID_FROM_RECORDING_START",
  "sequence": 1,
  "status": "completed",
  "captured_at": "2026-10-18T10:00:00Z",
  "movement": {
    "left_encoder_ticks": 520,
    "right_encoder_ticks": 518,
    "left_direction": "forward",
    "right_direction": "forward"
  },
  "sensor_readings": { "front_cm": null, "back_cm": 60 }
}
```

The result's `sequence` is `recording_sequence` from the poll when recording; otherwise it is the legacy transport `sequence`. For `started`, movement can be omitted; for `completed`, both measured wheel results/directions are mandatory. `failed` is explicit and blocks saving/activation until the incomplete recording is discarded/re-recorded. Timestamps require a timezone and are normalized to UTC.

## Save lifecycle and retries

Save freezes the name/destination/status for that finalization attempt, rejects new movements, and queues Stop if necessary. It returns **HTTP 202** while any ESP32 result is outstanding; this is not a saved route. Retry the same save body after the firmware results and checkpoint observations arrive.

Once consistent, it writes one complete JSONB route in a transaction. Only successful persistence releases the recording, intermediate samples, and buffered commands. A database failure returns 409/503 with the buffer retained. A retry uses the recording UUID as the route UUID, so a committed write or lost HTTP response cannot create duplicates. Even a failure after commit but before refresh is handled on retry. Late matching result acknowledgements are accepted from bounded receipts; late samples after release are acknowledged but do not alter the saved route.

## Website scope

Existing Atlas pages/stores/services now use text IDs, checkpoints, and encoder segments. Saved Routes can inspect/rename/delete V1 data and import measured JSON as a draft. The duration-based editor is removed in API mode; demo simulations remain isolated. Manual driving still uses the same backend command endpoint. Recording/checkpoint/finalization service methods are implemented and callable through `/docs`; a dedicated operator recording/confirmation UI and checkpoint-management page are not built in this backend/schema task. No legacy timed route is converted into fake encoder data.

## Verification and remaining gaps

Run `python -m unittest -v test_api` in this folder and `npm run check` in `libris`. Tests use isolated in-memory SQLite and PostgreSQL DDL compilation. Browser smoke tests use a separate temporary SQLite server with a simulated encoder-result producer; they do not verify physical motors, sensors, floor behavior, or remote Supabase.

Verified for this revision: 28 backend tests and 26 JavaScript tests passed; demo and API production builds passed. The headless Edge V1 smoke test passed encoder recording/save/retry, buffer clearing, text-ID route retrieval, measured-JSON import, reload persistence, and a 390px layout without page overflow or page-JavaScript errors. Its initial selector timeout was a test-locator issue (select labels include option text); the corrected locator passed. Native PostgreSQL/Supabase and physical ESP32 tests remain unrun.

See `SCHEMA_GAPS.md` for the distinction between implemented V1 fields, intentionally temporary state, absent location entities, and firmware/replay/security work still required. Existing Starlette TestClient emits a non-failing httpx deprecation warning.

## Files changed for this revision

Created: `atlas_runtime.py`, `atlas_validation.py`, `checkpoints.py`, `recordings.py`, `migrations/20261018_atlas_v1_encoder.sql`, `SCHEMA_GAPS.md`.

Updated: `database.py`, `atlas_schemas.py`, `robot.py`, `routes.py`, `migrate.py`, `main.py`, `shelves.py`, `test_api.py`, `.env.example`, this document; `src/services/api.js`, `src/services/backendData.js`, `src/views/admin/useAtlasStore.js`, `AtlasDetailView.vue`, `SettingsView.vue`, `components/AtlasDriveWorkspace.vue`; `tests/backend-data.test.js`, `README.md`, `INTEGRATION.md`.
