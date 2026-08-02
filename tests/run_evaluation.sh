#!/bin/sh

set -u

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
WEBSERV_PYTHON=${WEBSERV_PYTHON:-python3}

cd "$ROOT_DIR" || exit 1

printf 'Rebuilding webserv with the mandatory flags...\n'
make re || exit 1

printf '\nRunning the evaluation suite...\n'
exec "$WEBSERV_PYTHON" tests/evaluation_suite.py "$@"
