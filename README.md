# Libris

Libris brings a reader-facing library experience, a library administration workspace, and the Atlas ESP32 robot project together in one repository.

**Status: development / pre-deployment testing.** The browser demo runs independently. API mode requires a separately configured backend. Atlas firmware builds, but hardware configuration, TLS provisioning, and physical safety validation are not complete.

## Project layout

```text
src/                    Vue application, components, services, and demo data
public/                 Static browser assets
tests/                  JavaScript regression tests
backend/                FastAPI API, database models, migrations, and Python tests
hardware/Atlas/         Modular ESP32 firmware and offline Wi-Fi setup portal
hardware/libot-esp.ino  Legacy reference sketch; not the Atlas build entry point
docs/                   Frontend feature notes
.github/workflows/      Frontend and backend checks for this repository root
```

UI reference images remain in `src/assets/ui/`; they are design source material, not disposable build output.

## Quick start: browser demo

Requirements: **Node.js 24.x** (see `.nvmrc`) and npm.

```sh
npm ci
npm run dev
```

Open the URL printed by Vite. With `VITE_DATA_MODE` unset or set to `demo`, the app uses seeded data and browser-local persistence. No backend credentials are required. If you already have `.env.local`, check that it does not select API mode.

The frontend uses Vue 3, Vite, and GSAP. Reader pages include discovery, My Library, reading rooms, and profiles. The administration workspace manages the catalogue, shelves, members, and Atlas interfaces. See [frontend feature notes](docs/FRONTEND.md) for demo behavior and limitations.

## Run with the API

### 1. Start FastAPI

Use a supported Python 3 environment; CI uses Python 3.12. In PowerShell:

```powershell
cd backend
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
# Explicit LOCAL choice; do not overwrite a reviewed server configuration.
$env:DATABASE_URL = 'sqlite:///libris.db'
# For an existing catalogue, check compatibility before starting:
.\.venv\Scripts\python.exe book_migrate.py --check
.\.venv\Scripts\python.exe -m uvicorn main:app --host 127.0.0.1 --port 8000 --workers 1
```

On macOS/Linux, use `.venv/bin/python` instead of `.\.venv\Scripts\python.exe`. Run backend commands from `backend/` so the module imports resolve.

`DATABASE_URL` is required; missing/blank configuration fails instead of silently selecting SQLite. For server configuration, use `backend/.env.example` as a template for a private `backend/.env`; **replace the placeholders before use**. Do not blindly copy its PostgreSQL placeholder when intending to use SQLite. Process environment takes precedence over that file. Existing catalogues missing `books.bibliography` fail startup before migration DDL; follow the [Phase 1 database guide](docs/PHASE1_DATABASE.md) to review and upgrade a disposable local copy. A missing SQLite file is not created by the check command; a deliberately new database can be initialized by normal startup. Other existing startup schema/runtime changes still run on compatible databases: never point development startup at production.

PostgreSQL/Supabase is the production database target. Authentication, staff access, and robot device keys need explicit configuration. Disabling authentication/guidance features does **not** make protected operational endpoints public. Use one Uvicorn worker: live robot commands, recordings, and guidance coordination include in-memory state.

Health: `http://127.0.0.1:8000/ping` · API documentation: `http://127.0.0.1:8000/docs`

### 2. Enable frontend API mode

In another terminal, from the repository root:

```powershell
Copy-Item .env.example .env.local
npm run dev
```

Merge settings instead of overwriting an existing `.env.local`.

| Variable | Purpose |
| --- | --- |
| `VITE_DATA_MODE` | `demo` for isolated browser data; `api` for backend integration |
| `VITE_API_BASE_URL` | `/api` for the local proxy, or the deployed API URL |
| `LIBRIS_BACKEND_URL` | Development-only proxy target; defaults to `http://127.0.0.1:8000` |

Restart Vite after changing environment settings. API errors do not silently fall back to demo data, and demo records are not automatically uploaded.

**All `VITE_*` values are public browser configuration.** Never put database credentials, session secrets, robot keys, or private keys there.

## Verification

From the repository root:

```sh
npm run check
```

This runs the JavaScript tests and produces `dist/`. `npm run preview` serves that build locally; it is not a production server.

If Vite exits silently on Windows before printing its build banner, retry in PowerShell with `$env:NODE_DISABLE_COMPILE_CACHE='1'; npm run check`. This disables Node's compile cache for that shell session; it does not change project dependencies or build settings.

Backend tests, from `backend/`:

```powershell
.\.venv\Scripts\python.exe -m unittest discover -v
```

These tests use isolated databases and mocked providers/devices. Passing tests do not prove live Supabase, SMTP, robot motion, or physical safety. GitHub Actions runs frontend checks and backend tests when this folder is the repository root.

## Atlas firmware

The current PlatformIO project is `hardware/Atlas/`, environment `esp32dev`, using Arduino and Espressif32. See its [firmware README](hardware/Atlas/README.md) for architecture, configuration, build instructions, and hardware checklists.

- Local motor control, encoder measurement, ultrasonic sensing, safety checks, RAM route recording/replay, Wi-Fi provisioning, and backend communication are implemented in firmware.
- GPIO assignments and calibration are intentionally unconfigured. Do not invent wiring or enable motors before electrical review and calibration.
- Authenticated HTTPS requires the correct CA certificate and per-device key in an ignored `AtlasSecrets.local.h`, based on the example header. Never disable certificate verification to make a connection work.
- The approved layout uses a **3 MiB factory application partition on a configured 4 MiB ESP32**, with no OTA update workflow. The generated layout/build was validated, but the new partition table has not been flashed. Confirm physical flash capacity and plan backups before migration.
- Sensor stops and network leases are software safeguards, not an independent physical emergency stop. Normal/emergency stopping currently uses coast behavior; physical stopping distance is unverified.
- Local routes are RAM-only. Backend guidance protocol v2, Atlas OTP/display, exact independent-wheel route replay, and battery monitoring are not implemented in this firmware.

**No physical safety certification or deployment readiness is implied.** Do not upload or activate motors simply because compilation succeeds.

## Deployment

The Vue build can be hosted as static files. `vercel.json` configures Node 24, `npm ci`, `npm run check`, and `dist` output. Select this folder as the Vercel project root. Hash routing avoids SPA path-rewrite requirements.

Host FastAPI separately. Vite's `/api` proxy works only in development; production needs a real same-origin reverse proxy or an HTTPS API URL with the appropriate CORS/cookie configuration. Review authentication and database migrations before exposing the API. Fonts/icons and some book covers use external services; the frontend is not entirely offline.

## Documentation

- [Frontend features and demo behavior](docs/FRONTEND.md)
- [ISBN scanning and bulk shelf registration](docs/ISBN_REGISTRATION.md)
- [Phase 1 database compatibility and deployment verification](docs/PHASE1_DATABASE.md)
- [Backend integration and supported API boundaries](INTEGRATION.md)
- [Authentication and session configuration](backend/AUTH.md)
- [Guidance protocol and prerequisites](backend/GUIDANCE.md)
- [Atlas database and migrations](backend/ATLAS_DATABASE.md)
- [Known schema and integration gaps](backend/SCHEMA_GAPS.md)
- [Firmware, provisioning, partitions, and hardware validation](hardware/Atlas/README.md)
- [Generated artwork notes](src/assets/GENERATED_ASSETS.md)

## Before pushing to GitHub

Commit source, lockfiles, migrations, documentation, example configuration, and the generated portal header. Do not commit `.env` files, device secrets, database files/backups, virtual environments, `node_modules/`, `dist/`, or `.pio/` artifacts. The ignore rules cover these, but **do not remove secrets already tracked in Git**. Inspect `git status` and the staged diff before committing; rotate any credential previously published.

The portal header is intentionally checked in so firmware builds do not require a new build hook. After editing `hardware/Atlas/portal/index.html`, run its documented generator and `--check` before committing.

No open-source license is currently supplied. Agree on licensing and asset redistribution rights before treating the project as freely reusable.
