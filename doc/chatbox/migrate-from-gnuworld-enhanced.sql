-- ChatBox.nu: move a gnuworld-enhanced (Seven) cservice database to this GNUworld.
--
-- Run once, as the owner of the databases, on a fresh backup:
--   psql -h 127.0.0.1 -U gnuworld -d cservice -X -v ON_ERROR_STOP=1 -f migrate-from-gnuworld-enhanced.sql
--
-- Keeps users.nickname (nick protection) and the other gnuworld-enhanced columns that this
-- version does not use (users.hostname, users.created_ts, channels.welcome). Tables whose column
-- types change must be empty (checked below); on ChatBox.nu they were.

\connect cservice
BEGIN;

-- 1. Tables whose column types or layout change must be empty.
DO $$
BEGIN
  IF (SELECT count(*) FROM ip_restrict) > 0 THEN
    RAISE EXCEPTION 'ip_restrict has rows: convert them to the new layout by hand first';
  END IF;
  IF (SELECT count(*) FROM pending_traffic) > 0 THEN
    RAISE EXCEPTION 'pending_traffic has rows: ip_number changes from integer to inet';
  END IF;
END $$;

-- 2. User flags. users.flags becomes a 32-bit integer. gnuworld-enhanced used 0x800 for
--    AUTONICK and 0x1000 for POWER; here 0x800 is TOTP_REQ_IPR and 0x1000 is CERTONLY.
--    AUTONICK moves to 0x10000 and POWER (not used) is dropped.
ALTER TABLE users ALTER COLUMN flags TYPE integer;
ALTER TABLE users ALTER COLUMN flags SET DEFAULT 0;
UPDATE users SET flags = (flags & ~2048) | 65536 WHERE flags & 2048 <> 0;
UPDATE users SET flags = flags & ~4096 WHERE flags & 4096 <> 0;
ALTER TABLE users ADD COLUMN IF NOT EXISTS scram_record text;

-- 3. New columns.
ALTER TABLE channels ADD COLUMN IF NOT EXISTS limit_joinmax integer DEFAULT 3;
ALTER TABLE channels ADD COLUMN IF NOT EXISTS limit_joinsecs integer DEFAULT 1;
ALTER TABLE channels ADD COLUMN IF NOT EXISTS limit_joinperiod integer DEFAULT 180;
ALTER TABLE channels ADD COLUMN IF NOT EXISTS limit_joinmode varchar(255) DEFAULT '+rb *!~*@*';
ALTER TABLE pending ADD COLUMN IF NOT EXISTS first_init char(1) NOT NULL DEFAULT 'N';
ALTER TABLE supporters ADD COLUMN IF NOT EXISTS noticed char(1) NOT NULL DEFAULT 'N';
ALTER TABLE pending_emailchanges ADD COLUMN IF NOT EXISTS phase integer NOT NULL DEFAULT 1;

-- 4. Changed column types (tables are empty, see step 1).
ALTER TABLE pending_traffic ALTER COLUMN ip_number TYPE inet USING NULL;
ALTER TABLE whitelist ALTER COLUMN ip TYPE inet USING ip::inet;

-- 5. IP restrictions: new layout (value/expiry/description instead of allowmask/allowrange).
DROP TABLE ip_restrict;
CREATE TABLE public.ip_restrict (
    id integer NOT NULL,
    user_id integer NOT NULL,
    added integer NOT NULL,
    added_by integer NOT NULL,
    type integer DEFAULT 0 NOT NULL,
    value inet NOT NULL,
    last_updated integer DEFAULT (date_part('epoch'::text, CURRENT_TIMESTAMP))::integer NOT NULL,
    last_used integer DEFAULT 0 NOT NULL,
    expiry integer NOT NULL,
    description character varying(255)
);
CREATE SEQUENCE public.ip_restrict_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;
ALTER SEQUENCE public.ip_restrict_id_seq OWNED BY public.ip_restrict.id;
ALTER TABLE ONLY public.ip_restrict ALTER COLUMN id SET DEFAULT nextval('public.ip_restrict_id_seq'::regclass);
CREATE INDEX ip_restrict_idx ON public.ip_restrict USING btree (user_id, type);

-- 6. Tables that are new in this version.
CREATE TABLE public.user_sec_history (
    user_id integer NOT NULL,
    user_name text NOT NULL,
    command text NOT NULL,
    ip character varying(256) NOT NULL,
    ident text NOT NULL,
    hostmask character varying(256) NOT NULL,
    "timestamp" integer NOT NULL,
    deleted text DEFAULT 'N'::text NOT NULL
);
CREATE TABLE public.pending_chanfix_scores (
    channel_id integer,
    user_id text DEFAULT '0'::text NOT NULL,
    rank integer DEFAULT 0 NOT NULL,
    score integer DEFAULT 0 NOT NULL,
    account character varying(20) NOT NULL,
    first_opped character varying(10),
    last_opped character varying(20),
    last_updated integer DEFAULT (date_part('epoch'::text, CURRENT_TIMESTAMP))::integer NOT NULL,
    first character(1) DEFAULT 'Y'::bpchar NOT NULL
);
CREATE TABLE public.users_fingerprints (
    user_id integer NOT NULL,
    fingerprint character varying(128) NOT NULL,
    added_ts bigint NOT NULL,
    added_by character varying(128) NOT NULL,
    note text
);
CREATE INDEX idx_ip_ident_username ON public.user_sec_history USING btree (ip, ident, user_name);
CREATE INDEX idx_user_sec_history_deleted ON public.user_sec_history USING btree (deleted);
CREATE INDEX idx_user_sec_history_hostmask ON public.user_sec_history USING btree (hostmask);
CREATE INDEX idx_user_sec_history_ip_hostmask ON public.user_sec_history USING btree (ip, hostmask);
CREATE INDEX idx_user_sec_history_user_id ON public.user_sec_history USING btree (user_id);
CREATE INDEX pending_chanfix_scores_channel_id_idx ON public.pending_chanfix_scores USING btree (channel_id);
ALTER TABLE ONLY public.users_fingerprints
    ADD CONSTRAINT users_fingerprints_fingerprint_key UNIQUE (fingerprint);
ALTER TABLE ONLY public.pending_chanfix_scores
    ADD CONSTRAINT pending_chanfix_scores_channel_ref FOREIGN KEY (channel_id) REFERENCES public.channels(id);
ALTER TABLE ONLY public.users_fingerprints
    ADD CONSTRAINT users_fingerprints_user_id_fkey FOREIGN KEY (user_id) REFERENCES public.users(id) ON DELETE CASCADE;

-- 7. Trigger and helper functions as in this version.
CREATE OR REPLACE FUNCTION public.delete_ban()
 RETURNS trigger
 LANGUAGE plpgsql
AS $function$
BEGIN
	INSERT INTO deletion_transactions (tableID, key1, key2, key3, last_updated)
	VALUES(4, OLD.id, 0, 0, extract(epoch FROM now())::int);
	RETURN OLD;
END;
$function$
;

CREATE OR REPLACE FUNCTION public.delete_channel()
 RETURNS trigger
 LANGUAGE plpgsql
AS $function$
BEGIN
	INSERT INTO deletion_transactions (tableID, key1, key2, key3, last_updated)
	VALUES(2, OLD.id, 0, 0, extract(epoch FROM now())::int);
	RETURN OLD;
END;
$function$
;

CREATE OR REPLACE FUNCTION public.delete_level()
 RETURNS trigger
 LANGUAGE plpgsql
AS $function$
BEGIN
	INSERT INTO deletion_transactions (tableID, key1, key2, key3, last_updated)
	VALUES(3, OLD.channel_id, OLD.user_id, 0, extract(epoch FROM now())::int);
	RETURN OLD;
END;
$function$
;

CREATE OR REPLACE FUNCTION public.delete_user()
 RETURNS trigger
 LANGUAGE plpgsql
AS $function$
BEGIN
	INSERT INTO deletion_transactions (tableID, key1, key2, key3, last_updated)
	VALUES(1, OLD.id, 0, 0, extract(epoch FROM now())::int);
	RETURN OLD;
END;
$function$
;

CREATE OR REPLACE FUNCTION public.get_linked_users(user_id integer)
 RETURNS TABLE(total_usernames integer, all_usernames text[])
 LANGUAGE plpgsql
 STABLE
AS $function$
DECLARE
  uname TEXT;
BEGIN
  -- Get the most recent user_name for this user_id (in case of renames)
  SELECT ush.user_name INTO uname
  FROM user_sec_history ush
  WHERE ush.user_id = get_linked_users.user_id AND deleted = 'N'
  ORDER BY timestamp DESC
  LIMIT 1;

  IF uname IS NULL THEN
    RETURN;
  END IF;

  -- Recursively get all linked usernames
  RETURN QUERY
  WITH RECURSIVE link_graph(user_name) AS (
    SELECT user_name
    FROM multiusers_linked
    WHERE user_name = uname

    UNION

    SELECT unnest(linked_usernames)
    FROM multiusers_linked
    JOIN link_graph ON multiusers_linked.user_name = link_graph.user_name
  )
  SELECT
    COUNT(DISTINCT user_name)::INTEGER,
    array_agg(DISTINCT user_name ORDER BY user_name)
  FROM link_graph
  WHERE user_name <> uname;
END;
$function$
;

CREATE OR REPLACE FUNCTION public.new_user()
 RETURNS trigger
 LANGUAGE plpgsql
AS $function$
-- creates the users associated last_seen record
BEGIN
	INSERT INTO users_lastseen (user_id, last_seen, last_updated) VALUES(NEW.id, extract(epoch FROM now())::int, extract(epoch FROM now())::int);
	RETURN NEW;
END;
$function$
;

CREATE OR REPLACE FUNCTION public.update_bans()
 RETURNS trigger
 LANGUAGE plpgsql
AS $function$
BEGIN
	NOTIFY bans_u;
	RETURN NEW;
END;
$function$
;

CREATE OR REPLACE FUNCTION public.update_channels()
 RETURNS trigger
 LANGUAGE plpgsql
AS $function$
BEGIN
	NOTIFY channels_u;
	RETURN NEW;
END;
$function$
;

CREATE OR REPLACE FUNCTION public.update_levels()
 RETURNS trigger
 LANGUAGE plpgsql
AS $function$
BEGIN
	NOTIFY levels_u;
	RETURN NEW;
END;
$function$
;

CREATE OR REPLACE FUNCTION public.update_users()
 RETURNS trigger
 LANGUAGE plpgsql
AS $function$
BEGIN
	NOTIFY users_u;
	RETURN NEW;
END;
$function$
;

COMMIT;

\connect local_db
BEGIN;
-- The website's IP check for new accounts stores addresses as inet.
DO $$
BEGIN
  IF (SELECT count(*) FROM newu_ipcheck) > 0 THEN
    DELETE FROM newu_ipcheck WHERE expiration < extract(epoch FROM now());
  END IF;
END $$;
ALTER TABLE newu_ipcheck ALTER COLUMN ip DROP DEFAULT;
ALTER TABLE newu_ipcheck ALTER COLUMN ip TYPE inet USING ip::inet;
ALTER TABLE webcookies ADD COLUMN IF NOT EXISTS totp_cookie varchar(40);
COMMIT;

\connect ccontrol
BEGIN;
-- ccontrol changes from doc/ccontrol.update.sql (2016-2024) that gnuworld-enhanced did not have.
-- Single sign-on for opers (2016-04-17; defaults as changed on 2023-01-15).
ALTER TABLE opers ADD COLUMN IF NOT EXISTS autoop boolean NOT NULL DEFAULT 'n';
ALTER TABLE opers ADD COLUMN IF NOT EXISTS sso boolean NOT NULL DEFAULT 't';
ALTER TABLE opers ADD COLUMN IF NOT EXISTS ssooo boolean NOT NULL DEFAULT 't';
ALTER TABLE opers ADD COLUMN IF NOT EXISTS account varchar(128);
ALTER TABLE opers ADD COLUMN IF NOT EXISTS accountts integer NOT NULL DEFAULT 0;
-- Connection limits per ISP (2016-08-27, 2017-02-09, 2019-06-29, 2024-04-16). They replace
-- gnuworld-enhanced's shellcompanies/shellnetblocks, which stay in place untouched.
CREATE TABLE public.iplisps (
    id integer NOT NULL,
    name character varying(200) NOT NULL,
    email character varying(200) NOT NULL,
    clonecidr integer DEFAULT 0 NOT NULL,
    maxlimit integer DEFAULT 0 NOT NULL,
    maxidentlimit integer DEFAULT 0 NOT NULL,
    forcecount integer DEFAULT 0 NOT NULL,
    glunidented integer DEFAULT 0 NOT NULL,
    active integer DEFAULT 1 NOT NULL,
    nogline integer DEFAULT 0 NOT NULL,
    isgroup integer DEFAULT 0 NOT NULL,
    addedby character varying(200) NOT NULL,
    addedon integer NOT NULL,
    lastmodby character varying(200) NOT NULL,
    lastmodon integer NOT NULL
);
CREATE SEQUENCE public.iplisps_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;
ALTER SEQUENCE public.iplisps_id_seq OWNED BY public.iplisps.id;
CREATE TABLE public.iplnetblocks (
    ispid integer NOT NULL,
    cidr character varying(50) NOT NULL,
    addedby character varying(200) NOT NULL,
    addedon integer NOT NULL
);
ALTER TABLE ONLY public.iplisps ALTER COLUMN id SET DEFAULT nextval('public.iplisps_id_seq'::regclass);
ALTER TABLE ONLY public.iplisps
    ADD CONSTRAINT iplisps_name_key UNIQUE (name);
-- Defaults for PostgreSQL 13+ (2022-04-10).
ALTER TABLE glines ALTER COLUMN lastupdated SET DEFAULT date_part('epoch', CURRENT_TIMESTAMP)::int;
ALTER TABLE exceptions ALTER COLUMN addedon SET DEFAULT date_part('epoch', CURRENT_TIMESTAMP)::int;
COMMIT;
