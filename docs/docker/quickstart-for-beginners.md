# Docker quickstart for beginners

This starts a local Canary server and PostgreSQL database without compiling C++
locally.

MyAAC and the MySQL-only `opentibiabr/login-server` are not included. The server
still exposes native login and game protocols; clients that require HTTP login
need a separate PostgreSQL-compatible service.

## Requirements

- Docker Desktop on Windows/macOS, or Docker Engine with Compose v2 on Linux
- enough disk space for the runtime image, database, and map
- network access for the initial downloads

## Start

Open a terminal in the repository's `docker` directory:

```bash
cp .env.dist .env
```

On Linux or macOS:

```bash
sh ./up.sh
```

On Windows PowerShell:

```powershell
.\up.ps1
```

Direct Compose also works:

```bash
docker compose up -d
```

The first start can take several minutes while Docker downloads the image and
map. Watch progress with:

```bash
docker compose logs -f server
```

When the log says `Server Online!`, use login `127.0.0.1:7171`, game
`127.0.0.1:7172`, and status `127.0.0.1:7173`.

The default `.env` enables local test accounts. Do not expose this setup to the
public Internet with its default password or test data.

## LAN access

```bash
sh ./up.sh --lan
```

```powershell
.\up.ps1 -Lan
```

Allow the protocol ports through the host firewall if another computer cannot
connect.

## Stop and restart

```bash
docker compose down
docker compose up -d
```

Do not add `-v` unless you intentionally want to delete the PostgreSQL volume.

## Troubleshooting

```bash
docker compose ps
docker compose logs db
docker compose logs server
docker compose config
```

Changing database credentials after the first start does not alter the existing
volume. Restore the old `.env` values or back up the database and deliberately
recreate the volume.

For all settings and storage details, see
[`docker/DOCKER.md`](../../docker/DOCKER.md).
