# Phase 1: database compatibility and deployment verification

## Scope and observed environment

This work does not authorize production migrations, dashboard edits, firmware changes, or application redesign. No production connection, migration, or deployment change was performed. The original local database was inspected read-only and was **not upgraded**; all upgrade execution in this phase uses isolated temporary test databases.

At inspection time, the shell had no `DATABASE_URL`; neither `backend/.env` nor the frontend environment files were present. Previously that selected `backend/libris.db` implicitly. Its `books` table has the previous fields but no `bibliography`, and its migration markers contain Atlas/auth/guidance versions, not `20261024_book_bibliography`. This is evidence about **this local file only**, not deployed PostgreSQL.

The exact defect is that `BookModel` now selects nullable JSON `books.bibliography`, while startup `Base.metadata.create_all()` only creates missing tables and cannot add a column to an existing table. The standalone bibliography SQL was not part of startup and had not been applied to this file. An ORM query therefore fails even when no rows are requested.

## Explicit database selection

- `backend/database.py` loads private `backend/.env` without overriding existing process variables, then requires a nonblank `DATABASE_URL`. Invalid/unsupported values fail with a message that withholds the value. SQLite and PostgreSQL remain supported. There is **no implicit SQLite fallback**.
- An explicit SQLite URL still means SQLite; this change does not claim that an operator-supplied database is the intended production project. Review its target privately. SQLite is for local development/tests, not the documented production target.
- SQLite relative paths resolve from the command's working directory. Run commands below from `backend/`. The targeted CLI refuses missing SQLite files rather than accidentally creating a database; default/check mode opens existing files using SQLite `mode=ro`.
- Never copy database URLs, passwords, auth keys, device secrets, or complete environment dumps into issue reports, logs, browser configuration, or screenshots.

## Migration tracking and safety

Existing Atlas/auth/guidance functions insert version strings into `schema_migrations(version PRIMARY KEY)` and may perform idempotent checks/actions again at startup. This is bookkeeping, not a general sequential migration runner; an old marker never proves every current ORM column exists.

The targeted bibliography command is deliberately separate from startup:

| Condition | Check/startup | Explicit upgrade |
| --- | --- | --- |
| Previous books table, no column/marker | Check exits 1; startup fails before other migration DDL | Add nullable JSON column, validate it, record version |
| Compatible column, no marker (e.g. current `create_all`) | Compatible; marker reported absent | Validate and record version only |
| Compatible column and marker | Compatible | Repeatable; existing values retained |
| Marker present but column/table absent | Fail with explicit schema-review error | Fail, no silent skip/repair |
| Column has wrong type or is NOT NULL | Fail | Fail, no conversion/rebuild |
| No books table | Check exits 1; normal startup may create a deliberately new database | Refuse; targeted upgrade does not initialize a catalogue |

The upgrade changes only the nullable `bibliography` column and its version record. There is no backfill, row update, index/constraint removal, table replacement, or drop. Existing rows read NULL for the new field. SQLite uses explicit `BEGIN IMMEDIATE` so DDL and marker roll back together; PostgreSQL uses a transaction, migration advisory lock, and five-second lock timeout. PostgreSQL `ALTER TABLE` still requires an exclusive table lock and appropriate owner privileges: additive does **not** mean zero operational risk. Keep the application stopped for local upgrades; arrange a separately approved maintenance procedure for any future deployment.

Both the Python command and standalone PostgreSQL SQL validate actual column type/nullability and reject inconsistent tracking. Python follows the connection's schema resolution; the SQL file explicitly targets `public`. Verify PostgreSQL `search_path`/ORM resolution matches the intended schema before considering either. The existing Atlas/auth/guidance migration logic was not replaced. Normal compatible startup still runs those functions and runtime recovery; it is **not a read-only validation method**.

## Safe local procedure (not a production runbook)

1. Stop local application writers. Make a consistent disposable copy/backup with SQLite's backup API or trusted database tooling. If WAL is active, copying only the main SQLite file may omit committed data; do not do that. Confirm the copy can be opened and contains the expected records. For PostgreSQL use a separately provisioned disposable database/restore, never a production connection.
2. In a new local shell, from `backend/`, explicitly set `DATABASE_URL` to **that disposable copy**, not the original file or any hosted environment. For example, a copy named `local-review.db` in this folder uses:

   ```powershell
   $env:DATABASE_URL = 'sqlite:///local-review.db'
   python book_migrate.py --check
   ```

   Omitted arguments also mean check-only. Missing/unsupported configuration fails. Checks return 0 for a compatible schema, 1 for upgrade required/missing/incompatible/error; output includes only database dialect, schema status, and marker status. PostgreSQL check mode sets a read-only transaction.
3. Review the reported target privately and review `backend/book_migrate.py`. Only on the disposable database:

   ```powershell
   python book_migrate.py --apply
   python book_migrate.py --check
   ```

   `--apply` is the explicit write opt-in; it is never invoked by frontend builds or startup. Alternatively, review `migrations/20261024_book_bibliography.sql` for a disposable PostgreSQL `public` schema and execute using trusted tooling that stops on the first error (e.g. psql `ON_ERROR_STOP`). Do not run both indiscriminately; compatible repeats are validated, not blind ALTERs.
4. Check preserved row counts/keys/ISBNs/shelf references and unrelated tables; read/write bibliography using the ORM on the disposable copy, then repeat the command to confirm idempotency. Automated tests cover these checks in temporary databases. Do not delete the new column to roll back: stop writers and use a reviewed backup/restore plan if necessary.
5. Applying to the original local file is a separate explicit operator decision after backup and successful copy validation; this phase leaves it unchanged. Production requires a separately approved plan, not these local commands.

## Deployment checks: verified in source, not a live deployment certification

| Area | Verified locally | Read-only hosting verification still required |
| --- | --- | --- |
| Frontend mode/API | `src/services/api.js`: only `VITE_DATA_MODE=api` selects backend; base defaults to `/api`; requests include credentials. Vite values are build-time/public. | **Netlify:** inspect deployed commit, build context/environment names, build/publish directories (`npm run check`, `dist`), actual bundle's public API target, and production vs preview differences. Never export secret values. |
| Proxy/routing | `vite.config.js` proxies `/api` to `LIBRIS_BACKEND_URL` (default localhost:8000) and strips `/api` **only in development**. `vercel.json` has build/headers but no API rewrite. No Netlify `_redirects`/`netlify.toml` or Render blueprint was found in the project. | **Netlify:** inspect dashboard rules/proxies. If `/api` is used, confirm it reaches FastAPI with the expected prefix stripped, not SPA HTML. If a direct Render HTTPS base is used, verify exact CORS origin and cookie behavior. Local absence does not prove externally configured rules are absent. |
| Backend startup | `main.py` runs config validation, migration preflight/Atlas upgrades, auth/guidance upgrades, runtime recovery and watchdog. One worker is required by existing in-memory coordination. No live lifespan was started during validation. | **Render:** inspect service root/start command, commit, worker count, successful startup logs, environment-variable presence/source, intended database identity/dialect, TLS/pooler configuration. Do not restart or change variables. A missing URL will now fail before connecting instead of creating/selecting SQLite. |
| Database compatibility | Read-only local SQLite defect reproduced in isolated previous-schema tests; additive upgrade/rollback/repeatability verified there. | **Supabase/PostgreSQL:** with authorized read-only access verify intended project/database/schema, `books.bibliography` type/nullability, migration markers, existing table constraints, `search_path`, role/grant/RLS exposure, backup/restore readiness and startup-role privileges. Do not infer a defect from local SQLite. PostgreSQL DDL execution remains untested against a live disposable PostgreSQL server. |
| Auth/session | Source retains exact-origin credentialed CORS, HTTPS/secure-cookie checks, CSRF, trusted-staff and per-device guards. `AUTH_ENABLED=false` does not expose protected operations. No settings were relaxed. | **Render/Netlify/Supabase:** read-only inspection of enabled auth/allowed-origin configuration, provider project and redirect/OTP/SMTP setup, staff/device configuration presence; inspect existing approved session/network observations without sharing tokens. Direct cross-site Netlify→Render cookies require Secure + SameSite=None and can still be blocked by browser third-party-cookie policy; same-origin proxy cookie/CSRF forwarding must be verified. Creating login sessions/sending OTPs is not a read-only dashboard check and requires separate approval/testing. |

No Netlify/Render/Supabase dashboard settings or production database were accessed or changed in this phase. The locally inferred database selection is known; the deployed service's actual database remains **unverified**.

## Automated verification

```sh
# Repository root
npm run check
# backend/, in an environment with requirements.txt installed
python -m unittest -v test_book_migrate
python -m unittest discover -v
```

The new test module uses temporary SQLite files and the existing isolated test harness, never the application lifespan or configured production data. It reproduces the prior ORM failure, upgrades with preserved data/constraints/indexes, round-trips JSON, repeats safely, proves rollback on marker failure, rejects incompatible/falsely recorded schema, exercises read-only/default CLI checks and missing-file safety, and validates explicit database selection without disclosing URLs. CI now includes this upgrade module alongside its existing backend selection. PostgreSQL execution and live hosting/session behavior are separate verification requirements, not claims made by these tests.

### Observed verification results

- `python -m unittest -v test_book_migrate`: **11 passed**. These tests also passed during full discovery.
- `python -m unittest discover -v`: **134 run, 36 setup errors**, all reporting missing `schema_migrations` when `test_api.py` calls `upgrade_guidance()` before initializing migration tracking. The failing setup does not call the new startup preflight. The existing test harness/guidance initialization was not changed in this phase.
- `npm run check`: **35/36 tests passed**; the API client test at `tests/backend-data.test.js:53` expects the old search URL without pagination, while the current client includes `?limit=50&offset=0`. The frontend client/test were not changed in this phase. The chained build does not run after this test failure.
- Separate `npm run build`: **passed** (Vite, 372 modules).
- Read-only CLI check against the original local SQLite file: `upgrade_required`, version not recorded, expected exit 1. Its file hash was identical before and after the check. No upgrade was applied to that file.

These results are not an all-green release certification. The unrelated existing test failures remain visible rather than being bypassed or repaired outside Phase 1.
