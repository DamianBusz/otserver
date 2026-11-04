#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
DOCKER_DIR="${REPO_ROOT}/docker"
ENV_FILE="${DOCKER_DIR}/.env"

export COMPOSE_PROJECT_NAME="${COMPOSE_PROJECT_NAME:-otbr-smoke}"

cd "${DOCKER_DIR}"
cp .env.dist "${ENV_FILE}"

if [[ -n "${CANARY_IMAGE:-}" ]]; then
	sed -i "s|^CANARY_IMAGE=.*|CANARY_IMAGE=${CANARY_IMAGE}|" "${ENV_FILE}"
fi

if [[ -n "${CANARY_IMAGE_TAR:-}" ]]; then
	docker load --input "${REPO_ROOT}/${CANARY_IMAGE_TAR}"
fi

COMPOSE=(docker compose --env-file "${ENV_FILE}")

dump_debug() {
	echo "::group::Docker compose status"
	"${COMPOSE[@]}" ps || true
	echo "::endgroup::"

	echo "::group::Docker compose logs"
	"${COMPOSE[@]}" logs --no-color db server || true
	echo "::endgroup::"
}

cleanup() {
	"${COMPOSE[@]}" down -v --remove-orphans || true
}

on_exit() {
	local status=$?

	if [[ "${status}" -ne 0 ]]; then
		dump_debug
	fi

	cleanup
	exit "${status}"
}

wait_for_server() {
	for attempt in $(seq 1 120); do
		if "${COMPOSE[@]}" logs --no-color server 2>&1 | grep -qi "server online!"; then
			echo "Canary server is online"
			return 0
		fi

		echo "Waiting for Canary server (attempt ${attempt}/120)"
		sleep 5
	done

	echo "Canary server did not become online" >&2
	return 1
}

trap on_exit EXIT

cleanup

"${COMPOSE[@]}" config >/tmp/canary-docker-compose.yml
if [[ -n "${CANARY_IMAGE_TAR:-}" ]]; then
	"${COMPOSE[@]}" pull db
else
	"${COMPOSE[@]}" pull db server
fi
"${COMPOSE[@]}" up -d

wait_for_server

"${COMPOSE[@]}" exec -T server sh -lc '
	grep -q "postgresHost = \"db\"" config.lua &&
	grep -q "postgresDatabase = \"canary\"" config.lua &&
	grep -q "loginProtocolPort = 7171" config.lua &&
	grep -q "gameProtocolPort = 7172" config.lua &&
	grep -q "statusProtocolPort = 7173" config.lua
'

"${COMPOSE[@]}" exec -T db psql --no-psqlrc -v ON_ERROR_STOP=1 \
	-U canary -d canary -tAc "SELECT count(*) > 0 FROM accounts;" | grep -q '^t$'
