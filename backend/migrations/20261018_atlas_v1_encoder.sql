-- Atlas V1 only. Run as the trusted backend/migration owner, not anon.
-- Legacy Atlas data is not converted: encoder measurements cannot be derived from durations.
BEGIN;
DO $$
DECLARE atlas_table TEXT; row_count BIGINT;
BEGIN
  IF EXISTS (SELECT 1 FROM information_schema.columns WHERE table_schema='public' AND table_name='robots' AND column_name='robot_id')
     OR EXISTS (SELECT 1 FROM information_schema.columns WHERE table_schema='public' AND table_name='routes' AND column_name='shelf_id')
     OR to_regclass('public.route_steps') IS NOT NULL THEN
    FOREACH atlas_table IN ARRAY ARRAY['route_steps','routes','robots'] LOOP
      IF to_regclass('public.' || atlas_table) IS NOT NULL THEN
        EXECUTE format('SELECT count(*) FROM public.%I', atlas_table) INTO row_count;
        IF row_count > 0 THEN
          RAISE EXCEPTION 'Legacy Atlas tables are not empty. Back up and explicitly clear obsolete Atlas data first.';
        END IF;
      END IF;
    END LOOP;
    DROP TABLE IF EXISTS public.route_steps;
    DROP TABLE IF EXISTS public.routes;
    DROP TABLE IF EXISTS public.robots;
  END IF;
END $$;

CREATE TABLE IF NOT EXISTS public.checkpoints (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  name TEXT NOT NULL CHECK (length(trim(name)) BETWEEN 1 AND 100),
  location_id UUID NULL,
  connected_checkpoint_ids UUID[] NOT NULL DEFAULT '{}',
  is_active BOOLEAN NOT NULL DEFAULT TRUE,
  created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
-- No existing Libris locations table was found; location_id is nullable metadata,
-- not a claim that a location entity or location FK has been implemented.
CREATE TABLE IF NOT EXISTS public.robots (
  id TEXT PRIMARY KEY CHECK (id ~ '^[A-Za-z0-9-]{1,80}$'),
  name TEXT NOT NULL CHECK (length(trim(name)) BETWEEN 1 AND 100),
  residing_checkpoint_id UUID NULL REFERENCES public.checkpoints(id) ON DELETE SET NULL,
  last_sequence_number INTEGER NULL CHECK (last_sequence_number >= 0),
  firmware_version TEXT NULL,
  created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE TABLE IF NOT EXISTS public.routes (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  robot_id TEXT NOT NULL REFERENCES public.robots(id) ON DELETE RESTRICT,
  name TEXT NOT NULL CHECK (length(trim(name)) BETWEEN 1 AND 100),
  start_checkpoint_id UUID NOT NULL REFERENCES public.checkpoints(id) ON DELETE RESTRICT,
  destination_checkpoint_id UUID NOT NULL REFERENCES public.checkpoints(id) ON DELETE RESTRICT,
  status TEXT NOT NULL DEFAULT 'draft' CHECK (status IN ('draft','active','disabled')),
  recording_version INTEGER NOT NULL DEFAULT 1 CHECK (recording_version = 1),
  route_segments JSONB NOT NULL DEFAULT '[]'::jsonb CHECK (jsonb_typeof(route_segments) = 'array'),
  last_verified_at TIMESTAMPTZ NULL,
  created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  CONSTRAINT uq_routes_robot_name UNIQUE (robot_id, name)
);
CREATE INDEX IF NOT EXISTS ix_routes_robot_id ON public.routes(robot_id);
CREATE INDEX IF NOT EXISTS ix_routes_start_checkpoint_id ON public.routes(start_checkpoint_id);
CREATE INDEX IF NOT EXISTS ix_routes_destination_checkpoint_id ON public.routes(destination_checkpoint_id);
CREATE INDEX IF NOT EXISTS ix_robots_residing_checkpoint_id ON public.robots(residing_checkpoint_id);

CREATE OR REPLACE FUNCTION public.atlas_v1_touch_updated_at() RETURNS TRIGGER
LANGUAGE plpgsql AS $$ BEGIN NEW.updated_at = now(); RETURN NEW; END $$;
DROP TRIGGER IF EXISTS atlas_v1_robot_updated ON public.robots;
CREATE TRIGGER atlas_v1_robot_updated BEFORE UPDATE ON public.robots FOR EACH ROW EXECUTE FUNCTION public.atlas_v1_touch_updated_at();
DROP TRIGGER IF EXISTS atlas_v1_checkpoint_updated ON public.checkpoints;
CREATE TRIGGER atlas_v1_checkpoint_updated BEFORE UPDATE ON public.checkpoints FOR EACH ROW EXECUTE FUNCTION public.atlas_v1_touch_updated_at();
DROP TRIGGER IF EXISTS atlas_v1_route_updated ON public.routes;
CREATE TRIGGER atlas_v1_route_updated BEFORE UPDATE ON public.routes FOR EACH ROW EXECUTE FUNCTION public.atlas_v1_touch_updated_at();

-- FastAPI validates UUID array elements, directed transitions, sequence order,
-- checkpoint observations, and encoder JSON using the same service contract.
-- Do not expose unrestricted writes via Supabase's public REST interface.
ALTER TABLE public.robots ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.checkpoints ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.routes ENABLE ROW LEVEL SECURITY;
CREATE TABLE IF NOT EXISTS public.schema_migrations (version VARCHAR(100) PRIMARY KEY);
INSERT INTO public.schema_migrations(version) VALUES ('20261018_atlas_v1_encoder_v2') ON CONFLICT DO NOTHING;
COMMIT;
