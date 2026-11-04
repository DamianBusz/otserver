# Canary Docker quickstart

The quickstart runs PostgreSQL 16 and builds this checkout's PostgreSQL-enabled Canary image.

MyAAC is not part of this stack. Its legacy build files remain under
`docker/quickstart/myaac/` for reference. The upstream
`opentibiabr/login-server` is also excluded because it supports MySQL only.

## Start

From the `docker` directory:

```bash
cp .env.dist .env
docker compose up -d --build
```

Or use a guarded helper:

```bash
sh ./up.sh
```

```powershell
.\up.ps1
```

The helpers preserve the named database volume. Use `--lan` or `-Lan` to set
`CANARY_SERVER_IP` to the detected LAN address.

Follow startup logs:

```bash
docker compose logs -f server
```

Stop without deleting data:

```bash
docker compose down
```

## Configuration

Public Docker settings keep the `CANARY_*` prefix. Database defaults are:

```dotenv
CANARY_DB_HOST=db
CANARY_DB_PORT=5432
CANARY_DB_NAME=canary
CANARY_DB_USER=canary
CANARY_DB_PASSWORD=canary
```

Change the password outside local development. `.env.dist` also documents the
runtime image, advertised address, protocol ports, datapack, map URL, and test
account seeding.

The entrypoint imports `schema.sql` on an empty database and writes a backup to
`/data/<database>.sql` when an existing schema is detected. PostgreSQL data is
stored in `db-volume`; generated server data is stored in `server-data`.

Native Canary endpoints are login `7171`, game `7172`, status `7173`, legacy
11.00 game `7174`, and legacy 8.60 game `7175`.

No HTTP login endpoint is included. A modern client that requires HTTP login
needs a separate PostgreSQL-compatible login service.

## Test accounts

`CANARY_TEST_ACCOUNTS=true` imports the seed files in `docker/data/` on the
first schema import. Disable it for any non-local deployment.

## Troubleshooting

```bash
docker compose ps
docker compose logs db
docker compose logs server
docker compose config
```

Changing `.env` does not rewrite an existing PostgreSQL cluster. If credentials
or the database name change after initialization, restore the old values or
back up the database and deliberately recreate the volume.

The CI smoke test is `.github/scripts/docker-quickstart-smoke.sh`; it starts a
clean stack, waits for Canary, checks the generated PostgreSQL configuration,
and verifies seeded accounts.
