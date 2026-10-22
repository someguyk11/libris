# Frontend features

[Project setup and configuration](../README.md)

The behaviors below describe **demo mode** unless explicitly stated otherwise. Demo data is browser-local, separated from API-mode records, and tied to the hosting origin.

## Reader experience

- `/#/explore`: reader home and reading-room preview.
- `/#/rooms`: room selection and reading sessions.
- My Library, discovery, book details, and profile screens reuse the existing Libris components and design tokens.
- Reading sessions use a deadline-based timer, avoiding drift in inactive tabs. Pause, resume, reset, duration, book selection, and page changes are supported. Progress is stored per room; leaving pauses the session, while reloading an active session continues its countdown.
- Shared live room presence is not implemented. Interface preferences remain browser-local and respect reduced-motion settings.

## Administration

| Route | Workspace |
| --- | --- |
| `/#/admin` | Dashboard |
| `/#/admin/books` | Catalogue |
| `/#/admin/shelves` | Shelf registration and locations |
| `/#/admin/atlas` | Atlas overview |
| `/#/admin/atlas/atlas-01` | Demo Atlas detail, drive/record, monitor, saved routes |
| `/#/admin/settings` | Library details, members, Atlas links, animation preferences |

Profile → Library administration also opens the workspace. API-mode robot routes use the configured backend robot identifier, rather than assuming the demo ID.

The demo catalogue supports adding/editing titles, member loans and due dates, returns, reversible archiving, filtering, sorting, pagination, bulk actions, and CSV import/export. Dashboard totals and borrowing/return series use the same store; charts offer 7/30-day ranges, selectable series, keyboard-focusable points, and a data table.

CSV requires `title,author,isbn`; optional columns are `genre,shelf,status,language`. Imports validate all rows before adding records. ISBNs cannot be duplicated; borrowed books require a member and must be returned before archiving.

Shelf registration includes a name, unique code, category, section, and optional description/aisle/row. New shelves are available to book editing and CSV import. Settings includes searchable members and library details. Member categories are metadata, not authorization roles.

## Atlas demo is not robot actuation

Demo overview readings are labelled sample snapshots from `src/mock/data/atlas.js`; newly registered units have no telemetry. Direction controls move an illustrative scene only. Release, Stop, focus loss, hidden tabs, and workspace changes stop simulated motion. Record/finish/save creates a named local simulation; routes can be inspected, renamed, and deleted with confirmation.

Simulation records use `libris-atlas-demo-v1`. Unsaved drafts survive workspace-tab changes but not leaving the unit or reloading. Storage failures are shown, with changes retained for the current visit only.

`src/views/admin/components/AtlasScene.vue` uses the perspective SVG renderer in `atlasScene.js`. It accepts camera position/yaw/FOV, cuboid obstacles, grid settings, and obstacle scale. Coordinates are illustrative, not measured sensor units. Geometry is near-plane clipped and depth-sorted; no 3D engine is needed.

## API mode boundaries

Catalogue/shelf and supported Atlas workflows connect through the Python backend, not directly to an ESP32. Auth and narrow shelf-guidance interfaces require their documented server/provider configuration. Other reader features and browser preferences are not automatically server-backed. A visible or accepted command is not proof of completed hardware movement.

See the [integration matrix](../INTEGRATION.md) for supported API operations and the [firmware README](../hardware/Atlas/README.md) for actual hardware capabilities and unverified behavior. Full multi-library tenant isolation and physical validation remain deployment blockers.
