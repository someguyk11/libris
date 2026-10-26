# Authentication v0.1 — backend

Supabase Auth owns passwords, numeric email OTPs and refresh tokens. FastAPI owns
Libris account eligibility, opaque browser sessions, CSRF and local rate limits.
No CAPTCHA, Google login, membership, staff assignment or checkout QR is added.

**This does not make the entire application safe to expose publicly.** Existing
catalogue, shelf and Atlas endpoints are still unauthenticated. Library-scoped
authorization and independent robot credentials must be implemented before public
deployment. `require_user` checks a verified reader, not staff/library permissions.
The Vue authentication screen now connects signup, OTP verification/resend, login
and logout in API mode. See [GUIDANCE.md](GUIDANCE.md) for the opt-in operational
staff/device boundary; library-scoped roles and the password-recovery UI remain future work.

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

No provider account has been created and no live email/Supabase request or remote
migration has been performed here. Do not paste credentials into chat or `VITE_*`.

### Mandatory Supabase Dashboard settings

- Enable the Email provider, email/password signup and **Confirm email**. Disable
  anonymous signup and unused identity providers. Signup/resend fail closed if
  `/auth/v1/settings` reports auto-confirmation or email disabled.
- For **Confirm signup** and **Reset password** email templates, display the code:

  ```html
  <p>Your Libris verification code is <strong>{{ .Token }}</strong>.</p>
  <p>Enter this code in Libris. Never share it. Ignore this email if you did not request it.</p>
  ```

  Do not substitute `{{ .ConfirmationURL }}`: this flow expects numeric OTP input,
  not an auto-consuming email link. Supabase generates, expires and consumes OTPs;
  Libris never stores, invents or logs them. Confirm template delivery and replay
  rejection in a real project before launch. Verification uses `type=email` and
  recovery uses `type=recovery`.
- Configure the Site URL / allowed redirect URLs narrowly; do not allow wildcard
  preview domains. Configure provider password policy and upstream rate limits.
  OTP expiry, code length, resend cooldown, cleanup, MFA and disposable-email policy
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
`migrations/20261019_auth_v1.sql` as DB owner. It creates only private auth tables
and the migration ledger entry. Remote SQL has **not** been applied here.

## HTTP contract

All routes below use `/auth` (or `/api/auth` through the existing proxy).
Mutation requests require an exact allowlisted `Origin`, including scripts/tests.
JSON bodies are limited to 16 KiB. Responses are `no-store`; errors do not echo
passwords, codes, provider responses or tokens. Cookie-based mutations additionally
require `X-CSRF-Token` from `/auth/session` or a successful sign-in response.

| Method/path | Body / behavior |
| --- | --- |
| `POST /signup` | `{email,password}` → 202 generic response, **no app account/session** |
| `POST /otp/resend` | `{email}` → 202 generic response |
| `POST /otp/verify` | `{email,token}` → confirmed account, opaque cookie, user + CSRF |
| `POST /login` | `{email,password}` → cookie only if this account previously completed OTP |
| `GET /session` | Bootstrap user/recovery session + CSRF; no cookie → 401 |
| `GET /me` | Verified reader session; recovery session rejected |
| `POST /refresh` | CSRF required; rotate provider tokens server-side, preserve absolute deadline |
| `POST /logout` | CSRF if session exists; revoke local session, clear cookie even on provider outage |
| `POST /logout-all` | Valid unexpired cookie + CSRF; revoke all local sessions; provider logout best-effort |
| `POST /password/forgot` | `{email}` → 202 generic response |
| `POST /password/verify` | `{email,token}` → **restricted recovery session**, not reader access |
| `POST /password/reset` | `{password}` + recovery cookie + CSRF; change password, revoke all local sessions, require new login |

Successful verification/login: `{authenticated,purpose,csrf_token,user:{id,email,
email_verified}}`. Session bootstrap also returns `expires_at`. There are no role
claims or memberships; registration never enrolls anyone in a library. Future
Vue integration must use `credentials:'include'`, keep CSRF in memory and never
store Supabase tokens in localStorage. Google remains outside this implementation.

Disabled auth returns 503. Invalid credentials/codes/session return 401; wrong
purpose/CSRF/Origin 403; invalid input 422; oversized input 413; non-JSON 415;
throttling 429 with `Retry-After`; provider outage 503. Eligibility-related email
responses are generic, but network timing is not guaranteed constant-time.

## Storage and safeguards

Separate `AuthBase` leaves the finalized Atlas metadata/migrations unchanged.
PostgreSQL uses private `libris_auth`; SQLite development uses prefixed tables:

- `auth_accounts`: Supabase UUID string, canonical email, actual OTP-verification
  time, active/suspended state, creation time. Created only after verified OTP;
  recovery verification also demonstrates inbox control. No password hashes here.
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

Email OTP proves inbox control, **not humanity**. No implementation can guarantee
zero spam accounts. Local quotas + upstream quotas + unverified-access restrictions
reduce abuse without CAPTCHA. Attackers can still register many real inboxes and
call Supabase's public auth endpoints directly; production provider limits and
monitoring are essential. Libris creates no usable local account until OTP succeeds.

## Operations / remaining work

- No live provider/SMTP/native PostgreSQL test has run. Test delivery, expired and
  replayed codes, recovery, provider policy mismatches and concurrent PostgreSQL
  refresh/logout before production. Unit tests use a mock provider, not evidence
  of actual mail delivery or Supabase token behavior.
- Authentication does **not** implement library-scoped owner/staff roles, tenant
  isolation, device credentials or protection of existing operational endpoints.
  Do not wrap hardware polling in browser cookies; it needs independent device auth.
- Auth UI integration, profile fields, staff invitations, QR borrowing and MFA are
  separate follow-up work. No fines/reviews/support/feedback backend was added.
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
