# Libris authentication and profiles

Supabase Auth owns passwords, numeric email OTPs and refresh tokens. FastAPI owns
Libris account eligibility, opaque browser sessions, CSRF and local rate limits.
No CAPTCHA, Google login, membership, staff assignment or checkout QR is added.

**This does not make the entire application safe to expose publicly.** Existing
catalogue, shelf and Atlas endpoints are still unauthenticated. Library-scoped
authorization and independent robot credentials must be implemented before public
deployment. `require_user` checks a verified reader, not staff/library permissions.
The Vue screen connects optional signup, verification/resend, login, password
recovery and profile editing in API mode. Browsing remains public; personal and
staff features retain their session guards. See [GUIDANCE.md](GUIDANCE.md) for
the opt-in operational staff/device boundary; library-scoped roles remain future work.

## Providers / configuration you need

1. **Supabase project** with Auth and PostgreSQL. Obtain its project URL,
   publishable key (or legacy `anon` key), and a server-only database connection
   string. A service-role key / `sb_secret_*` is neither needed nor accepted by the
   auth client. Database credentials are separate and must remain server-only.
2. **Production transactional email/SMTP provider**, e.g. Resend, Postmark or SES,
   configured in Supabase Auth's custom SMTP settings. Verify a sending domain and
   its required DNS records (SPF/DKIM, with an appropriate DMARC policy). Supabase's
   built-in mail is restricted and unsuitable for general production signup.
3. **HTTPS hosting/reverse proxy** for Vue + FastAPI, preferably the same origin
   (`https://app.example/api`). No new authentication SDK, Redis or CAPTCHA service
   is required. Keep Atlas on one worker as required by its in-memory runtime.

No provider account or live email has been created, and no remote migration has
been performed here. Read-only diagnostics found upstream email-send quota errors.
Do not paste credentials into chat or `VITE_*`.

### Mandatory Supabase Dashboard settings

- Enable the Email provider, email/password signup and **Confirm email**. Disable
  anonymous signup and unused identity providers. Signup/resend fail closed if
  `/auth/v1/settings` reports auto-confirmation or email disabled.
- For **Confirm signup** and **Reset password** email templates, display the code:

  ```html
  <p>Your Libris verification code is <strong>{{ .Token }}</strong>.</p>
  <p>Enter this code in Libris. Never share it. Ignore this email if you did not request it.</p>
  ```

   Numeric codes remain supported. Default `{{ .ConfirmationURL }}` links are also
   supported: Supabase redirects to the frontend with tokens in the fragment, which
   Libris immediately removes from the URL and exchanges for an HttpOnly session.
   Recovery links open the new-password form rather than the normal signed-in UI.
   Alternatively, use a token-hash link pointing to
   `https://your-app.example/?token_hash={{ .TokenHash }}&type=recovery` for the reset
   template (use `type=signup` for confirmation). Do not use a PKCE `code` link:
   this BFF does not initiate a PKCE flow. Supabase generates, expires and consumes
   codes/hashes; Libris never stores or logs them. Test real delivery, expiry and
   replay rejection before launch. Numeric verification uses `type=email` and
   recovery uses `type=recovery`.
- Set the Site URL to the deployed **frontend**, not the API. Configure allowed redirect URLs narrowly; do not allow wildcard
  preview domains. Configure provider password policy and upstream rate limits.
  OTP expiry, code length, upstream resend cooldown, cleanup, MFA and disposable-email policy
  remain deployment decisions, as requested; the API does not pretend to set them.
- Do **not** expose `libris_auth` in Supabase API exposed schemas. The additive
  migration revokes `PUBLIC`, `anon`, `authenticated` access and enables RLS with no
  public policies. FastAPI currently needs a server-only DB owner connection;
  a future least-privilege backend DB role needs explicit privileges/RLS policies.

### Server setup

Merge `backend/.env.example` into `backend/.env` without overwriting existing
credentials. Preserve the existing database target. Generate two independent keys:

```powershell
.\.venv\Scripts\python.exe -c "from cryptography.fernet import Fernet; print(Fernet.generate_key().decode())"
.\.venv\Scripts\python.exe -c "import secrets; print(secrets.token_urlsafe(48))"
```

Use the first as `AUTH_SESSION_ENCRYPTION_KEY`, the second as `AUTH_SECURITY_SECRET`.
Store both in deployment secret storage. Never publish generated output. Set:

- `AUTH_ENABLED=true`, `SUPABASE_URL`, `SUPABASE_PUBLISHABLE_KEY`, `DATABASE_URL`.
- Production: `AUTH_ENV=production`, `AUTH_COOKIE_SECURE=true`, exact HTTPS
  `CORS_ORIGINS` without paths/trailing slashes. HTTP/insecure cookie config fails
  startup. For localhost only, explicitly set `AUTH_ENV=development` and
  `AUTH_COOKIE_SECURE=false`.
- Prefer a same-origin proxy or same-site API subdomain with the default
  `AUTH_COOKIE_SAMESITE=lax`. Direct Netlify-to-Render authentication requires
  `AUTH_COOKIE_SAMESITE=none` and `AUTH_COOKIE_SECURE=true`; Origin and CSRF checks
  remain mandatory. Browsers blocking third-party cookies can still require a
  same-site deployment.

### Render / Netlify CORS configuration

Set Render's `CORS_ORIGINS=https://libotsystems.netlify.app` (no trailing slash),
`AUTH_COOKIE_SAMESITE=none`, `AUTH_COOKIE_SECURE=true`, and `AUTH_ENV=production`.
Enable/configure auth with `AUTH_ENABLED=true`, the Supabase public credentials,
`AUTH_SESSION_ENCRYPTION_KEY`, and `AUTH_SECURITY_SECRET` described above; retain
the existing `DATABASE_URL`. Do not use wildcard origins or disable authentication.
The backend loads `.env` without overriding Render's environment variables.
Environment changes require a restart/redeployment. Normal application startup
runs the existing migrations; coordinate deployment under your database policy.

Verify after deployment, without touching the database:

```sh
curl -i -X OPTIONS https://libot-8okb.onrender.com/checkpoints/ -H "Origin: https://libotsystems.netlify.app" -H "Access-Control-Request-Method: GET" -H "Access-Control-Request-Headers: content-type,x-csrf-token"
curl -i https://libot-8okb.onrender.com/ping -H "Origin: https://libotsystems.netlify.app"
```

Expect preflight 200 and exact `Access-Control-Allow-Origin` plus
`Access-Control-Allow-Credentials: true` on both responses. Repeat preflight for
`/book/search/%25`, `/robots/`, and `/shelves/`. An unlisted origin must not receive
`Access-Control-Allow-Origin`. Check the real authenticated browser flow too.
An API 401/403/503 with CORS headers is an authentication/configuration error,
not a CORS failure; proxy-generated errors cannot be fixed by FastAPI middleware.

Install `requirements.txt`, then run `python -m uvicorn main:app --host 127.0.0.1
--port 8000`. Normal startup applies the additive auth migration transactionally
(even when auth is disabled); existing Atlas/book/shelf models are unchanged.
For manual Supabase application, review/run
`migrations/20261019_auth_v1.sql`, then `migrations/20261027_auth_profiles_v2.sql`
as DB owner. They create only private auth tables and migration ledger entries.
Remote SQL has **not** been applied here. The connected Supabase public schema
differs from this backend's operational models; do not switch database targets or
apply Atlas/catalogue migrations to it as part of an auth repair.

## HTTP contract

All routes below use `/auth` (or `/api/auth` through the existing proxy).
Mutation requests require an exact allowlisted `Origin`, including scripts/tests.
JSON bodies are limited to 16 KiB. Responses are `no-store`; errors do not echo
passwords, codes, provider responses or tokens. Cookie-based mutations additionally
require `X-CSRF-Token` from `/auth/session` or a successful sign-in response.

| Method/path | Body / behavior |
| --- | --- |
| `POST /signup` | `{email,password,display_name?,username?}` → 202 generic response, **no app account/session** |
| `POST /otp/resend` | `{email}` → 202 generic response |
| `POST /otp/verify` | `{email,token}` → confirmed account, opaque cookie, user + CSRF |
| `POST /login` | `{email,password}` → cookie after Supabase confirms credentials and verified email, including pre-existing provider accounts |
| `POST /callback` | `{type,token_hash}` or `{type,access_token,refresh_token}` → verified user or restricted recovery session; tokens are validated by Supabase |
| `GET /session` | Bootstrap user/recovery session + CSRF; no cookie → 401 |
| `GET /me` | Verified reader session; recovery session rejected |
| `PATCH /profile` | `{display_name,username}` + reader cookie + CSRF; persisted name/unique username; conflict → 409 |
| `POST /refresh` | CSRF required; rotate provider tokens server-side, preserve absolute deadline |
| `POST /logout` | CSRF if session exists; revoke local session, clear cookie even on provider outage |
| `POST /logout-all` | Valid unexpired cookie + CSRF; revoke all local sessions; provider logout best-effort |
| `POST /password/forgot` | `{email}` → 202 generic response |
| `POST /password/verify` | `{email,token}` → **restricted recovery session**, not reader access |
| `POST /password/reset` | `{password}` + recovery cookie + CSRF; change password, revoke all local sessions, require new login |

Successful verification/login: `{authenticated,purpose,csrf_token,user:{id,email,
email_verified,is_staff,display_name,username}}`. Session bootstrap also returns `expires_at`. There are no role
claims or memberships; registration never enrolls anyone in a library. The
Vue integration uses `credentials:'include'`, keeps CSRF in memory and never
stores Supabase tokens in localStorage. Google remains outside this implementation.

Disabled auth returns 503. Invalid credentials/codes/session return 401; wrong
purpose/CSRF/Origin 403; invalid input 422; oversized input 413; non-JSON 415;
throttling 429 with `Retry-After`; provider outage 503. Eligibility-related email
responses are generic, but network timing is not guaranteed constant-time.

## Storage and safeguards

Separate `AuthBase` leaves the finalized Atlas metadata/migrations unchanged.
PostgreSQL uses private `libris_auth`; SQLite development uses prefixed tables:

- `auth_accounts`: Supabase UUID string, canonical email, confirmation-observed
  time (`otp_verified_at`, retained for compatibility), active/suspended state and
  creation time. Created only after provider-confirmed identity. No password hashes here.
- `auth_profiles`: one-to-one FK to `auth_accounts.user_id`, display name (80
  characters), optional unique lowercase username (3–30 ASCII letters/digits/underscores)
  and update timestamp. Signup metadata is copied only after confirmation; taken
  usernames do not prevent sign-in. Edit profile to choose a replacement. Profile
  fields do not assign staff privileges and provider metadata never authorizes actions.
- `auth_sessions`: random cookie **hash only**, user FK, user/recovery purpose,
  Fernet-encrypted access/refresh tokens, access expiry, absolute/idle timestamps,
  revocation. No JWTs or refresh tokens leave the backend.
- `auth_rate_limits`: atomic database counters keyed by HMAC email/IP/action;
  survives process restart and counts failures. Shared signup/resend/recovery send
  budget prevents bypass by changing endpoint. Limits are configurable initial
  guardrails in `.env.example`, **not finalized product policies**.
- `auth_events`: success/failure events with user ID when available, HMAC email/IP
  and timestamp. No raw OTP, password, cookie, provider tokens or raw IP in events.
  These pseudonymous records still require controlled access/retention decisions.

### Identity linkage

`user.id` is always the canonical Supabase `auth.users.id`, not a username or email.
Profiles and local sessions reference the same local account ID; guidance requests
already use that identity. For server-side joins to Supabase's UUID `borrow_records.user_id`
or `reservations.user_id`, compare to `auth_accounts.user_id::uuid`. Always take the
ID from `require_user`, never a client-supplied owner. Those public tables are not
managed by this backend's model metadata, so no speculative FK or borrowing endpoint
is installed here. Keep private profiles unexposed; access them through `/auth/profile`.

Each authenticated check queries Supabase `/user` and validates verified email,
UUID identity and local account status; it never trusts client identity or merely
decoded JWTs. PostgreSQL account/session row locks serialize refresh and revocation;
SQLite has a single-process development lock. Cookies are host-only, HttpOnly,
Secure in production, SameSite=Lax by default (configurable above), Path=/
(`__Host-libris_session` in production).
HTTPS, Origin validation and session-bound CSRF protect mutations/login CSRF.
Logout clears encrypted tokens locally even when upstream revocation fails;
`provider_revoked:false` is reported honestly. Upstream JWTs can remain valid until
their provider expiry: local logout is not a claim of immediate global JWT invalidation.

Rate limits use the server-visible client IP and ignore browser-supplied forwarded
headers. Behind a proxy configure Uvicorn forwarded-header trust **only for the
actual trusted proxy addresses**, never `*` on a public interface. Otherwise every
client may share one IP budget or spoofed headers could bypass it. Supabase sees
the BFF egress IP, so its upstream limits may aggregate all users. This implementation
does not send `Sb-Forwarded-For` or use a secret/admin key to bypass those limits.

Local attempt and shared-email windows now default to **60 seconds**, rather than
5/15 minutes. Redundant signup-specific attempt counters are removed; shared
email-send abuse protection remains (3/email and 30/IP per window by default).
Old buckets extending beyond a newly shortened window reset on the next request.
The UI waits 30 seconds before a successful resend and displays the server's actual
`Retry-After` on 429 (also exposed through CORS). Set both window env vars to `60`
when an existing deployment overrides the defaults. These changes cannot remove
Supabase/SMTP hourly quotas: configure custom SMTP and review Supabase Auth rate
limits. Provider `Retry-After` is preserved, not shortened dishonestly; when absent,
the UI offers a retry after the local window, which is not a promise of quota recovery.

Email OTP proves inbox control, **not humanity**. No implementation can guarantee
zero spam accounts. Local quotas + upstream quotas + unverified-access restrictions
reduce abuse without CAPTCHA. Attackers can still register many real inboxes and
call Supabase's public auth endpoints directly; production provider limits and
monitoring are essential. Libris creates no usable local account until identity is confirmed.

## Operations / remaining work

- No live provider/SMTP/native PostgreSQL test has run. Test delivery, expired and
  replayed codes, recovery, provider policy mismatches and concurrent PostgreSQL
  refresh/logout before production. Unit tests use a mock provider, not evidence
  of actual mail delivery or Supabase token behavior.
- Authentication does **not** implement library-scoped owner/staff roles, tenant
  isolation, device credentials or protection of existing operational endpoints.
  Do not wrap hardware polling in browser cookies; it needs independent device auth.
- Staff invitations, QR borrowing and MFA remain separate follow-up work.
  No fines/reviews/support/feedback backend was added.
- Use TLS DB connections, restrict DB/backups, protect encryption keys and rotate
  secrets deliberately. Changing the Fernet key without a key-migration plan makes
  existing sessions unreadable; changing the security secret invalidates CSRF/rate
  keys. Existing secrets are not copied or exposed by this task.
- No automatic deletion schedule is enabled while retention/cleanup policy is
  deferred. Plan scheduled removal of expired rate rows and expired/revoked session
  credentials, audit retention, and eligible unverified **provider** accounts.
  Do not delete verified accounts or operational records as blanket cleanup.
- Apply request/time limits at the reverse proxy and monitor email volume/429s.
  Keep API request bodies, cookies and authorization headers out of access/APM logs.
- Run `python -m unittest test_auth test_api -v` in `backend/`; frontend regression:
  `npm run check` from `libris/`.
