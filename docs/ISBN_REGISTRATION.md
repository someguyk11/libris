# ISBN scanning and shelf registration

The existing **Admin → Books** page has two API-mode actions:

- **Add book / Scan ISBN**: scan or type an ISBN, review/correct metadata, select a shelf, and register. An existing edition requires explicit shelf-reassignment confirmation; its metadata is never overwritten.
- **Scan to shelf**: select one shelf, scan consecutive books, review/confirm each entry, then register a batch of up to 50 distinct editions. Remove mistaken scans before saving. Existing editions are reported as skipped and are never moved.

CSV import is not part of this workflow. Demo mode retains its existing local book editor; scanning uses the actual backend, not seeded metadata.

## Existing schema boundaries

The current database has `books` (unique ISBN) and `shelves`, not separate physical-copy or library-ownership tables. This workflow therefore registers **distinct ISBN editions**, not multiple copies of the same edition. ISBN-10 and its equivalent ISBN-13 identify one edition. Repeated scans cannot add copies. Conflicting legacy alias records return a conflict for librarian resolution rather than being silently merged.

Title, author, publication date, availability, and shelf assignment keep their existing fields. Optional publisher, description, edition, cover URL, and the equivalent/original ISBN-10 are stored in a nullable `books.bibliography` JSON column. Missing provider fields remain blank; the server can derive the equivalent ISBN-10 for a 978 ISBN-13.

### Local schema prerequisite

For an **existing disposable local/test database**, follow [Phase 1 database compatibility](PHASE1_DATABASE.md): explicitly select the database, run `python book_migrate.py --check`, back up/copy it, and review the targeted `--apply` operation on the disposable copy. SQLite and PostgreSQL use the same validated upgrade procedure. The PostgreSQL-only SQL alternative is `backend/migrations/20261024_book_bibliography.sql` (targets `public`). Both add only the nullable JSON column and a migration marker; they never rewrite ISBNs or existing metadata. Startup does not apply this upgrade: it checks compatibility and fails clearly if the column is missing or incompatible. New test databases created from SQLAlchemy metadata already contain the column.

**No production database was connected to, no migration was executed, and no production data was changed during implementation.** Applying schema changes to a real environment requires a separate reviewed operation; do not point local development at production.

## Camera and manual fallback

- Camera permission is requested only after **Start camera**, never on page/dialog opening.
- The lazily loaded MIT-licensed `@zxing/browser` / `@zxing/library` decoder recognizes EAN-13; the ISBN validator rejects non-book EANs and incorrect checksums. It prefers the rear camera.
- HTTPS (or localhost) and a browser supporting camera access are required. A LAN HTTP address generally cannot access the camera. Embedded contexts may also need camera permission from their parent/browser policy.
- Permission denied, missing/busy camera, unsupported context, and decoder failures explain manual entry. Stop/restart is available. Closing, unmounting, saving, and canceling a pending camera start release camera tracks.
- ISBN-10 (including X), ISBN-13, spaces, and hyphens are supported manually. Both client and server validate checksums; the server is authoritative. Inputs are canonicalized to ISBN-13 before insertion.

## API and safety

All calls use the existing credentialed fetch/CSRF utility and existing staff authorization. Metadata reads explicitly require staff access even though ordinary catalogue search is public. The current installation uses the existing trusted-staff allowlist, not invented per-library roles or shelf ownership.

- `GET /book/metadata/{isbn}` returns `{isbn, found, metadata, existing}`. It consults the local catalogue first and **never writes**. Unknown editions can be entered manually, including after provider failures.
- `POST /book/register` uses the existing registration endpoint with optional `bibliography`, `require_new`, and `expected_existing_id`. New scanner registrations use `require_new: true`; confirmed existing moves use the expected record ID. Legacy callers retain their existing reassignment semantics.
- `POST /book/register-bulk` accepts `{shelf_id, books}` with 1–50 items. New items have `isbn`, `title`, `author`, optional `publish_date`/`bibliography`; confirmed existing items use `{isbn, existing_id}`. Results contain per-item `created`, `existing`, or `duplicate` status plus the actual book/shelf where applicable. A stale existing reference, invalid item, or database integrity conflict saves **none** of the batch.

Bulk inserts run in one transaction. PostgreSQL advisory locks serialize overlapping canonical ISBNs, alongside the existing unique ISBN constraint. Retries skip editions already registered, preserving their shelves and metadata. A single-book timeout is also safe against duplicate creation; a subsequent conflict prompts a fresh lookup. The UI retains a failed batch for correction/retry, but unsaved batches are not persisted across reloads.

Provider logic exists only in `backend/book_metadata.py`: Open Library Books API data and edition details, HTTPS, bounded response size, 3-second per-request/6-second overall timeout, safe 429/502/504 errors, a per-worker 256-entry cache (10-minute hits/1-minute misses), and coalesced simultaneous lookups. No credentials are needed. Covers returned by the provider are restricted to its HTTPS cover host. Metadata is reviewed before saving; provider responses cannot overwrite existing books.

## Verification

From the project root:

```sh
npm run check
```

From `backend/`, using an environment with `requirements.txt` installed:

```sh
python -m unittest discover -v
```

Automated coverage includes ISBN/checksum/alias validation, metadata mapping and provider failures/timeouts/cache/coalescing, server authorization and CSRF, atomic batch failures/retries, duplicate/legacy records, corrected metadata persistence, and camera permission/start/close/decode races.

Implementation verification: **36 JavaScript tests, 114 backend tests, and Vite production build passed**. Browser smoke tests used explicitly isolated HTTP fixtures (not a real database): denied permission followed by corrected manual ISBN-10 registration, and actual ZXing decoding of two consecutive EAN-13 barcodes from a canvas video stream at 390px. The equivalent ISBN-10 did not create another queue item; tracks ended on save. These checks do not replace real iOS/Android camera testing or validation against a disposable PostgreSQL instance. No live Open Library availability or production deployment is implied.
