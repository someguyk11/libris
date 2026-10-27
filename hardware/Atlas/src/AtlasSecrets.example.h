#pragma once
// Copy to AtlasSecrets.local.h (git-ignored). Provision OFFLINE, not in the portal.
// Must match this robot's >=32-character entry in backend ATLAS_DEVICE_KEYS.
#define ATLAS_DEVICE_KEY ""
// PEM CA certificate(s) for BackendBaseUrl; use the issuing root, not a leaf pin.
// Empty means backend HTTPS is disabled. NEVER use setInsecure().
#define ATLAS_TLS_CA R"PEM()PEM"
// Supabase anon (public) key for robot_commands table access.
// Empty means Supabase-backed command polling is disabled.
#define SUPABASE_ANON_KEY ""
