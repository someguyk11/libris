-- Reviewed optional metadata for ISBN registration, on the EXISTING books table.
-- Operator-applied migration only. Not executed automatically by this feature.
-- PostgreSQL; test on a disposable/local database before any deployment.
-- Operator-only, after backup and review on a disposable PostgreSQL database.
-- No production execution is authorized by the presence of this file.
BEGIN;
SET LOCAL lock_timeout = '5s';
SELECT pg_advisory_xact_lock(hashtext('20261024_book_bibliography'));
DO $$
BEGIN
    IF to_regclass('public.books') IS NULL THEN
        RAISE EXCEPTION 'books is missing; this upgrade does not initialize databases';
    END IF;
    IF NOT EXISTS (SELECT 1 FROM information_schema.columns
                   WHERE table_schema = 'public' AND table_name = 'books' AND column_name = 'bibliography') THEN
        IF to_regclass('public.schema_migrations') IS NOT NULL THEN
            IF EXISTS (SELECT 1 FROM public.schema_migrations WHERE version = '20261024_book_bibliography') THEN
                RAISE EXCEPTION 'Migration recorded but bibliography missing; review schema explicitly';
            END IF;
        END IF;
        ALTER TABLE public.books ADD COLUMN bibliography JSON;
    END IF;
    IF NOT EXISTS (SELECT 1 FROM information_schema.columns
                   WHERE table_schema = 'public' AND table_name = 'books' AND column_name = 'bibliography'
                     AND udt_name IN ('json', 'jsonb') AND is_nullable = 'YES') THEN
        RAISE EXCEPTION 'bibliography must be nullable JSON or JSONB; no type conversion performed';
    END IF;
END $$;
CREATE TABLE IF NOT EXISTS public.schema_migrations (version VARCHAR(100) PRIMARY KEY);
INSERT INTO public.schema_migrations(version) VALUES ('20261024_book_bibliography') ON CONFLICT DO NOTHING;
COMMIT;
