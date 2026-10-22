# LiBot backend → Libris

Only `LiBot/Back-End` supplied backend logic. No LiBot frontend, assets, virtual environment, caches, or credentials were imported. The source LiBot folder is unchanged.

## Start locally (PowerShell)

In `libris/backend`:

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
.\.venv\Scripts\python.exe -m uvicorn main:app --host 127.0.0.1 --port 8000
```

Atlas V1 production storage is Supabase/PostgreSQL. Configure the real server-only `DATABASE_URL` in `backend/.env` (the example contains placeholders). The existing `backend/libris.db` SQLite fallback is retained for local development/tests. No original LiBot secrets were copied or accessed. Guidance adds a nullable shelf destination checkpoint and a separate task table; books and finalized Atlas tables are preserved. The Atlas upgrade replaces only empty legacy Atlas tables, never inventing encoder measurements. PostgreSQL SQL is prepared but **not applied remotely** here. See [Atlas database](backend/ATLAS_DATABASE.md), [guidance](backend/GUIDANCE.md) and [schema gaps](backend/SCHEMA_GAPS.md).

In a second terminal, in `libris`:

```powershell
Copy-Item .env.example .env.local
npm run dev
```

Do not overwrite an existing `.env.local`; merge the example variables instead. `VITE_DATA_MODE=api` enables the integration; `demo` (or an unset mode) preserves the original browser demo. Restart Vite after changing configuration. `VITE_API_BASE_URL=/api` uses the development proxy; `LIBRIS_BACKEND_URL` selects its Python server target.

Open `/#/admin/shelves`, `/#/admin/books`, and `/#/admin/atlas`. API errors are visible and **never fall back to demo records**. Nothing from demo storage is automatically uploaded to the database.

## Integrated features and limits

| Feature | Backend | Libris interface |
| --- | --- | --- |
| Shelf registration/list/edit/delete | `POST/GET /shelves/`, `GET/PUT/DELETE /shelves/{id}` | Existing Register Shelf page, now with editing and confirmed deletion. Assigned shelves cannot be deleted. |
| Book registration/search | `POST /book/register`, `GET /book/search/{query}` | Existing catalogue loads the full database using the `%` wildcard; its search/filter/sort/pagination/export remain client-side. |
| Book shelf reassignment | Existing-ISBN registration | Catalogue editor makes existing title/author/ISBN/publication fields read-only. LiBot only updates shelf assignments for an existing ISBN. |
| Persistent Atlas registrations | `GET/POST /robots/`, `PATCH /robots/{id}` | Overview and settings store robot names and stable firmware IDs in the database. Previous browser records have an explicit import action. |
| Encoder routes | `GET/POST /robots/{id}/routes`, `GET/PUT/PATCH/DELETE /routes/{id}` | V1 JSON encoder segments and checkpoint endpoints; retrieval, measured JSON draft import, rename, delete. Timed drafts are no longer submitted by API mode. |
| Checkpoints | `GET/POST /checkpoints/`, `GET/PUT/DELETE /checkpoints/{id}` | Implemented API with directed array validation and soft deactivation; dedicated checkpoint UI remains future work. |
| Temporary recordings | `/robot/{id}/recordings`, `/recordings/{id}` plus `/checkpoints`, `/finalize`, `/save` | Implemented buffer/result/finalization workflow with idempotent saves. Dedicated operator UI and ESP32 result reporting still need integration. |
| Atlas commands | `POST /robot/{id}/command` | Existing Drive & Record workspace sends forward, backward, left, right, left_turn, right_turn, and stop. No direct ESP32 connection. |
| Atlas monitoring | `GET /robot/{id}/status` | Overview and Monitor poll every two seconds. Network, signal strength, uptime, last-seen, and command sequence are actual backend values; unreported readings are not shown as telemetry. Signal units are not assumed because the inherited schema does not specify them. |
| Wi-Fi provisioning | `POST /robot/{id}/reconfigure` | New network form in existing Atlas settings. Credentials are cleared from the form after success, excluded from browser persistence, and omitted from status responses. |
| Factory reset | `POST /robot/{id}/reset` | Confirmed reset action in Atlas settings. Success means queued, **not completed**. |
| Firmware polling/results | `GET /robot/{id}/command`, `POST /robot/{id}/telemetry`, `/command/result`, `/sensors` | Legacy envelope retained, internal command/recording IDs and execution acknowledgements added. Firmware must deduplicate before actuation; no physical firmware was available to verify. |
| Health/OpenAPI | `/ping`, `/docs` | Server diagnostics. |
| Email/password + OTP + sessions | `/auth/*`; see [AUTH.md](backend/AUTH.md) | Existing auth page now supports signup, numeric OTP/resend, login and logout in API mode. Recovery/logout-all backend exists; dedicated recovery UI is future work. Requires Supabase Auth and production custom SMTP. |
| Reader catalogue and guidance | `/book/search/{query}`, `/guidance-requests` | Discover searches actual backend books; book details offer explicit guidance and owned task status/cancellation. Search alone never dispatches Atlas. |
| Shelf destination and task monitor | `/shelves/{id}/destination`, `/robots/{id}/guidance-tasks`, `/checkpoint-confirmation`, `/guidance/stop` | Existing shelf editor maps a checkpoint; Atlas Monitor provides physical start/arrival confirmation, task state and emergency stop. |
| Atlas OTP and return protocol v2 | `/guidance-requests/{id}/verify`, `/robot/{id}/guidance/command`, `/guidance/result`, `/guidance/arrival` | Device-only six-digit display code (60 seconds, five attempts), owned atomic authorization, FIFO queue, acknowledged outbound/return replay, physical checkpoint attestation and contextual ESP32 arrival. Compatible firmware and physical validation remain required. No checkout verification. |

Shelf IDs are database-generated integers. LiBot has no shelf category/aisle/row columns, so API mode omits those demo fields. Floor and section are supported. Existing `path_id` values are preserved when editing but not presented as functional navigation routes.

Only finalized robot metadata, checkpoints, and completed JSON encoder routes are persisted in `robots`, `checkpoints`, and `routes`. No `route_steps` table remains. Pending commands, execution buffers, live telemetry, credentials/reset flags, and intermediate samples are temporary; use **one Uvicorn worker**. Restart loses unsaved recordings and never implies current-location confirmation. Browser-only robot names/IDs may be explicitly imported; demo simulations are not uploaded.

The scene remains illustrative, not live pose/obstacle mapping. Recording and acknowledged replay APIs require compatible firmware and physical safety testing. Guidance uses exact active outbound and separately recorded return routes, not a navigation algorithm. Only confirmed home/stopped Atlas issues its display code; physical checkpoint attestation plus contextual ESP32 arrival gates the return and final home completion. Multi-library tenancy, fleet scheduling, battery diagnostics, circulation, archiving, bulk import and book metadata updates remain absent. Other reader sections remain demos; preferences remain browser-local. See [AUTH.md](backend/AUTH.md) and [GUIDANCE.md](backend/GUIDANCE.md). No live Supabase/SMTP configuration or remote migration was performed.

Demo mode retains the existing simulated driving, recording, saved-route management, member/loan management, and seeded data, isolated from backend mode.

## Hardware safety and deployment

- Physically test the firmware's command meanings before driving near people or shelves. HTTP acceptance does not acknowledge movement or a completed stop.
- Movement requires recent firmware command polling. The UI refreshes held commands and sends stop on release, focus loss, hidden tabs, navigation, and teardown. Requests are serialized to prevent a late movement request overtaking stop.
- The merged backend expires movement after one second without refresh, returning an incremented stop command on polling. This **cannot stop disconnected motors**: firmware must independently stop motors on lost connectivity/poll failure. A physical emergency stop is still needed.
- Network/reset flags retain LiBot's one-shot pickup protocol. Pickup is not an application acknowledgement; firmware retry/acknowledgement support is future work.
- Verified reader sessions, a server-managed staff UUID allowlist and independent per-robot keys protect guidance and legacy operational endpoints. Disabling `GUIDANCE_ENABLED` never bypasses operational authentication. Library-scoped owner-managed roles remain future work; the allowlist is only a restricted v0.1 deployment boundary, not multi-library authorization. CORS is not authentication.
- For production, host Python separately and set `VITE_API_BASE_URL` to its HTTPS origin, configuring `CORS_ORIGINS` explicitly, or supply a same-origin `/api` reverse proxy. Vite's proxy runs only during development; static Vercel hosting does not run this Python server.

## Validation

```powershell
# libris
npm run check

# libris/backend (isolated in-memory DB; never uses configured production data)
 .\.venv\Scripts\python.exe -m unittest -v test_guidance test_auth test_api
```

The 26 JavaScript tests and 83 Python tests (28 Atlas/catalogue, 21 auth, 31 guidance, three real cookie/CSRF guidance-boundary tests) cover existing behavior plus request-bound Atlas codes, expiry/attempts, concurrent consumption, FIFO, confirmed return gating, exact routes/replay, stops/timeouts and migration. Providers/devices are mocked. PostgreSQL native types are DDL-compiled; live Supabase, SMTP and physical firmware tests require configured providers/hardware.

The V1 headless Edge smoke test passed temporary recording/save/retry, buffer clearing, text robot IDs, saved encoder-route inspection/import, reload persistence, and 390px layout checks against a separate temporary SQLite server with simulated firmware results. It is not a physical recording test.

The guidance Edge smoke test also passed real catalogue search → explicit request → simulated encoder acknowledgements → staff-confirmed arrival through the live HTTP API, at desktop and 390px widths. Identity and the device were mocked, not live Supabase or physical motors. The additive auth/guidance migrations were applied to the existing local SQLite database only; no remote database was changed.

The original LiBot virtual environment pointed at a missing Python 3.10 installation; a fresh ignored `backend/.venv` was used instead. The installed Starlette TestClient emits a non-failing httpx deprecation warning. Browser testing also identified invalid HTML-pattern hyphens in existing Atlas/shelf inputs; the touched inputs were corrected. No remaining build or page-JavaScript errors were observed. A deliberate assigned-shelf deletion test returns HTTP 400 as expected.

## File inventory

Created:

- `backend/main.py`, `backend/database.py`, `backend/books.py`, `backend/shelves.py`, `backend/robot.py` — adapted LiBot backend modules.
- `backend/requirements.txt`, `backend/.env.example`, `backend/test_api.py`.
- `backend/atlas_schemas.py`, `backend/routes.py`, `backend/migrate.py`, `backend/ATLAS_DATABASE.md` — persistent Atlas/route extension; local `backend/libris.db` is ignored.
- `backend/atlas_runtime.py`, `backend/atlas_validation.py`, `backend/checkpoints.py`, `backend/recordings.py`, `backend/SCHEMA_GAPS.md`, `backend/migrations/20261018_atlas_v1_encoder.sql` — finalized encoder recording architecture.
- `backend/auth.py`, `backend/auth_config.py`, `backend/auth_models.py`, `backend/auth_provider.py`, `backend/auth_security.py`, `backend/auth_migrate.py`, `backend/test_auth.py`, `backend/AUTH.md`, `backend/migrations/20261019_auth_v1.sql` — Supabase OTP authentication and private server-side sessions.
- `backend/guidance.py`, `backend/guidance_access.py`, `backend/guidance_models.py`, `backend/guidance_migrate.py`, `backend/test_guidance.py`, `backend/GUIDANCE.md`, `backend/migrations/20261019_guidance_v1.sql` — narrow recorded-route guidance and security boundary.
- `backend/migrations/20261019_atlas_otp_v2.sql` — additive home/state, request-bound Atlas OTP/session and return-route upgrade; no checkout verification.
- `src/components/book/ReaderGuidance.vue`, `src/views/admin/components/GuidanceMonitor.vue` — compact reader and staff task controls.
- `.env.example`, `INTEGRATION.md`.
- `src/services/api.js`, `src/services/backendData.js`.
- `src/composables/useAtlasTelemetry.js`.
- `tests/backend-data.test.js`.

Modified:

- `.gitignore`, `vite.config.js`, `README.md`.
- `src/views/admin/useAdminStore.js`, `src/views/admin/useAtlasStore.js`.
- `src/views/admin/atlasData.js` — preserve case-sensitive firmware IDs in API mode, retain demo normalization.
- `src/views/admin/AdminView.vue`, `src/views/admin/DashboardView.vue`, `src/views/admin/BooksView.vue`, `src/views/admin/ShelvesView.vue`.
- `src/views/admin/AtlasOverviewView.vue`, `src/views/admin/AtlasDetailView.vue`, `src/views/admin/SettingsView.vue`.
- `src/views/admin/components/AtlasDriveWorkspace.vue`, `src/views/admin/operations.css`.
- `src/views/AuthView.vue`, `src/views/DiscoverView.vue`, `src/views/BookDetailsView.vue` — API-mode authentication, actual catalogue search and explicit guidance; demo mode preserved.

Reused: existing admin layout/navigation/routes, catalogue table/filtering/editor, `AppButton`, `AdminDialog`, `BookCard`, `BookCover`, `EmptyState`, `AtlasScene`, `SearchBar`, `ScrollRail`, `SectionHeader`, Atlas stores/workspaces, typography, Material Symbols and existing CSS tokens. Explore and the LiBot frontend were not changed; Discover changes are limited to API-mode catalogue search.

Assumptions: preserve book/shelf functionality and firmware endpoint envelopes; replace the empty legacy Atlas-only schema with the finalized text-ID/checkpoint/JSONB V1 design; do not convert time into invented encoder values; keep recording state temporary and demo data separate; do not modify an unconfigured remote database or claim unverified hardware behavior.
