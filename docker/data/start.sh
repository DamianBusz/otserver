#!/bin/bash -e

CANARY_DB_HOST="${CANARY_DB_HOST:-db}"
CANARY_DB_PORT="${CANARY_DB_PORT:-5432}"
CANARY_DB_USER="${CANARY_DB_USER:-canary}"
CANARY_DB_PASSWORD="${CANARY_DB_PASSWORD:-canary}"
CANARY_DB_NAME="${CANARY_DB_NAME:-canary}"
CANARY_SERVER_NAME="${CANARY_SERVER_NAME:-OpenTibiaBR Canary}"
CANARY_SERVER_IP="${CANARY_SERVER_IP:-127.0.0.1}"
CANARY_LOGIN_PORT="${CANARY_LOGIN_PORT:-7171}"
CANARY_GAME_PORT="${CANARY_GAME_PORT:-7172}"
CANARY_LEGACY_1100_GAME_PORT="${CANARY_LEGACY_1100_GAME_PORT:-7174}"
CANARY_LEGACY_860_GAME_PORT="${CANARY_LEGACY_860_GAME_PORT:-7175}"
CANARY_STATUS_PORT="${CANARY_STATUS_PORT:-7173}"
CANARY_STATUS_TIMEOUT="${CANARY_STATUS_TIMEOUT:-5000}"
CANARY_TEST_ACCOUNTS="${CANARY_TEST_ACCOUNTS:-false}"
CANARY_DATA_PACK="${CANARY_DATA_PACK:-data-otservbr-global}"
CANARY_MAP_URL="${CANARY_MAP_URL:-https://github.com/opentibiabr/canary/releases/download/v3.6.1/otservbr.otbm}"

validate_identifier() {
	local name="$1"
	local value="$2"
	if [[ ! "$value" =~ ^[A-Za-z0-9_][A-Za-z0-9_-]*$ ]]; then
		echo "Invalid ${name}: '${value}'. Use only letters, numbers, underscores, and hyphens." >&2
		exit 1
	fi
}

require_uint() {
	local name="$1"
	local value="$2"
	if [[ ! "$value" =~ ^[0-9]+$ ]]; then
		echo "Invalid ${name}: '${value}'. Use only unsigned integer values." >&2
		exit 1
	fi
}

escape_lua_string() {
	printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'
}

set_lua_line() {
	local key="$1"
	local replacement="$2"
	local tmp_file
	local found=0

	tmp_file=$(mktemp)
	while IFS= read -r line || [ -n "$line" ]; do
		if [[ "$line" == "$key = "* ]]; then
			printf '%s\n' "$replacement"
			found=1
		else
			printf '%s\n' "$line"
		fi
	done <config.lua >"$tmp_file"

	if [ "$found" -eq 0 ]; then
		printf '%s\n' "$replacement" >>"$tmp_file"
	fi

	mv "$tmp_file" config.lua
}

set_lua_string() {
	local key="$1"
	local value="$2"
	set_lua_line "$key" "$key = \"$(escape_lua_string "$value")\""
}

set_lua_number() {
	local key="$1"
	local value="$2"
	set_lua_line "$key" "$key = $value"
}

psql_cmd() {
	PGPASSWORD="$CANARY_DB_PASSWORD" psql \
		--no-psqlrc \
		--set=ON_ERROR_STOP=1 \
		--no-password \
		--username="$CANARY_DB_USER" \
		--host="$CANARY_DB_HOST" \
		--port="$CANARY_DB_PORT" \
		--dbname="$CANARY_DB_NAME" \
		"$@"
}

pg_dump_cmd() {
	PGPASSWORD="$CANARY_DB_PASSWORD" pg_dump \
		--no-password \
		--username="$CANARY_DB_USER" \
		--host="$CANARY_DB_HOST" \
		--port="$CANARY_DB_PORT" \
		--dbname="$CANARY_DB_NAME" \
		"$@"
}

validate_identifier "CANARY_DB_NAME" "$CANARY_DB_NAME"
require_uint "CANARY_DB_PORT" "$CANARY_DB_PORT"
require_uint "CANARY_LOGIN_PORT" "$CANARY_LOGIN_PORT"
require_uint "CANARY_GAME_PORT" "$CANARY_GAME_PORT"
require_uint "CANARY_LEGACY_1100_GAME_PORT" "$CANARY_LEGACY_1100_GAME_PORT"
require_uint "CANARY_LEGACY_860_GAME_PORT" "$CANARY_LEGACY_860_GAME_PORT"
require_uint "CANARY_STATUS_PORT" "$CANARY_STATUS_PORT"
require_uint "CANARY_STATUS_TIMEOUT" "$CANARY_STATUS_TIMEOUT"

echo ""
echo "===== Canary Docker Configuration ====="
echo ""
echo "CANARY_DB_HOST:[$CANARY_DB_HOST]"
echo "CANARY_DB_PORT:[$CANARY_DB_PORT]"
echo "CANARY_DB_USER:[$CANARY_DB_USER]"
echo "CANARY_DB_PASSWORD:[********]"
echo "CANARY_DB_NAME:[$CANARY_DB_NAME]"
echo "CANARY_SERVER_NAME:[$CANARY_SERVER_NAME]"
echo "CANARY_SERVER_IP:[$CANARY_SERVER_IP]"
echo "CANARY_LOGIN_PORT:[$CANARY_LOGIN_PORT]"
echo "CANARY_GAME_PORT:[$CANARY_GAME_PORT]"
echo "CANARY_LEGACY_1100_GAME_PORT:[$CANARY_LEGACY_1100_GAME_PORT]"
echo "CANARY_LEGACY_860_GAME_PORT:[$CANARY_LEGACY_860_GAME_PORT]"
echo "CANARY_STATUS_PORT:[$CANARY_STATUS_PORT]"
echo "CANARY_STATUS_TIMEOUT:[$CANARY_STATUS_TIMEOUT]"
echo "CANARY_TEST_ACCOUNTS:[$CANARY_TEST_ACCOUNTS]"
echo "CANARY_DATA_PACK:[$CANARY_DATA_PACK]"
echo "CANARY_MAP_URL:[$CANARY_MAP_URL]"
echo ""
echo "======================================="
echo ""

echo ""
echo "===== OTBR Global Data Pack ====="
echo ""

if [ "$CANARY_DATA_PACK" = "data-otservbr-global" ] && [ ! -f data-otservbr-global/world/otservbr.otbm ]; then
	echo "Downloading OTBR map..."
	tmp_map="data-otservbr-global/world/otservbr.otbm.tmp"
	rm -f "$tmp_map"
	if ! curl --fail --show-error --location \
		--connect-timeout 5 --max-time 180 \
		"$CANARY_MAP_URL" -o "$tmp_map"; then
		rm -f "$tmp_map"
		exit 1
	fi
	mv "$tmp_map" data-otservbr-global/world/otservbr.otbm
	echo "Done"
else
	echo "Map download skipped"
fi

echo ""
echo "================================"
echo ""

echo ""
echo "===== Wait For The DB To Be Up ====="
echo ""

until psql_cmd --command="SELECT 1;" >/dev/null 2>&1; do
	echo "DB offline, trying again"
	sleep 5s
done

echo ""
echo "================================"
echo ""

echo ""
echo "===== Ensure Canary Schema ====="
echo ""

if psql_cmd --tuples-only --no-align --command="SELECT to_regclass('public.server_config') IS NOT NULL;" | grep -q '^t$'; then
	echo "Existing Canary schema detected"
	echo "Saving database backup to /data/${CANARY_DB_NAME}.sql"
	pg_dump_cmd --file="/data/${CANARY_DB_NAME}.sql.tmp"
	mv "/data/${CANARY_DB_NAME}.sql.tmp" "/data/${CANARY_DB_NAME}.sql"
else
	echo "Importing Canary schema"
	psql_cmd --file=schema.sql

	echo ""
	echo "===== Test Accounts ====="
	echo ""

	if [ "$CANARY_TEST_ACCOUNTS" = "true" ]; then
		echo "Creating test accounts..."
		psql_cmd --file=/canary/01-test_account.sql
		psql_cmd --file=/canary/02-test_account_players.sql
	else
		echo "Test account creation skipped"
	fi

	echo ""
	echo "================================"
	echo ""
fi

echo ""
echo "================================"
echo ""

echo ""
echo "===== Resolve Advertised Server IP ====="
echo ""

if [ "$CANARY_SERVER_IP" = "auto" ]; then
	echo "IP discovery enabled"
	CANARY_SERVER_IP=$(curl --fail --show-error --location \
		--connect-timeout 5 --max-time 15 \
		https://ifconfig.me/ip)
	echo "Discovered IP: $CANARY_SERVER_IP"
else
	echo "IP discovery disabled"
fi

echo ""
echo "================================"
echo ""

echo ""
echo "===== Apply Server Configuration ====="
echo ""

set_lua_string "postgresHost" "$CANARY_DB_HOST"
set_lua_string "postgresUser" "$CANARY_DB_USER"
set_lua_string "postgresPass" "$CANARY_DB_PASSWORD"
set_lua_number "postgresPort" "$CANARY_DB_PORT"
set_lua_string "postgresDatabase" "$CANARY_DB_NAME"
set_lua_string "serverName" "$CANARY_SERVER_NAME"
set_lua_string "ip" "$CANARY_SERVER_IP"
set_lua_number "loginProtocolPort" "$CANARY_LOGIN_PORT"
set_lua_number "gameProtocolPort" "$CANARY_GAME_PORT"
set_lua_number "legacy1100GameProtocolPort" "$CANARY_LEGACY_1100_GAME_PORT"
set_lua_number "legacy860GameProtocolPort" "$CANARY_LEGACY_860_GAME_PORT"
set_lua_number "statusProtocolPort" "$CANARY_STATUS_PORT"
set_lua_number "statusTimeout" "$CANARY_STATUS_TIMEOUT"
set_lua_string "dataPackDirectory" "$CANARY_DATA_PACK"
set_lua_string "mapDownloadUrl" "$CANARY_MAP_URL"

echo "config.lua updated"

echo ""
echo "================================"
echo ""

if [ -d "/data/server/" ]; then
	echo ""
	echo "===== Copy Server Configuration And Data Pack To Shared Folder ====="
	echo ""

	cp config.lua /data/server/
	cp -r data/ /data/server/
	cp -r "$CANARY_DATA_PACK"/ /data/server/

	echo ""
	echo "================================"
	echo ""
fi

echo ""
echo "===== Start Server ====="
echo ""

ulimit -c unlimited
exec canary
