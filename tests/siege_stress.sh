#!/bin/sh

set -u

CONFIG=${1:-configs/default.conf}
URL=${WEBSERV_SIEGE_URL:-http://127.0.0.1:8080/}
CONCURRENCY=${WEBSERV_SIEGE_CONCURRENCY:-50}
DURATION=${WEBSERV_SIEGE_DURATION:-30S}
SIEGE_LOG=$(mktemp /tmp/webserv-siege.XXXXXX)
SERVER_LOG=$(mktemp /tmp/webserv-server.XXXXXX)
SERVER_PID=
SIEGE_PID=

cleanup()
{
	if [ -n "$SIEGE_PID" ] && kill -0 "$SIEGE_PID" 2>/dev/null; then
		kill "$SIEGE_PID" 2>/dev/null
		wait "$SIEGE_PID" 2>/dev/null
	fi
	if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
		kill "$SERVER_PID" 2>/dev/null
		wait "$SERVER_PID" 2>/dev/null
	fi
	rm -f "$SIEGE_LOG" "$SERVER_LOG"
}

trap cleanup 0 1 2 15

if ! command -v siege >/dev/null 2>&1; then
	printf 'ERROR: siege is not installed. On Debian/Ubuntu: sudo apt install siege\n' >&2
	exit 1
fi

./webserv "$CONFIG" >"$SERVER_LOG" 2>&1 &
SERVER_PID=$!
sleep 1
if ! kill -0 "$SERVER_PID" 2>/dev/null; then
	printf 'ERROR: webserv did not start:\n' >&2
	cat "$SERVER_LOG" >&2
	exit 1
fi

if ! siege -b -c 1 -r 1 "$URL" >/dev/null 2>&1; then
	printf 'ERROR: warm-up request failed:\n' >&2
	cat "$SERVER_LOG" >&2
	exit 1
fi

RSS_START=$(awk '/VmRSS:/ { print $2 }' "/proc/$SERVER_PID/status")
RSS_MAX=$RSS_START

siege -b -c "$CONCURRENCY" -t "$DURATION" "$URL" >"$SIEGE_LOG" 2>&1 &
SIEGE_PID=$!
while kill -0 "$SIEGE_PID" 2>/dev/null
do
	RSS_CURRENT=$(awk '/VmRSS:/ { print $2 }' "/proc/$SERVER_PID/status")
	if [ -n "$RSS_CURRENT" ] && [ "$RSS_CURRENT" -gt "$RSS_MAX" ]; then
		RSS_MAX=$RSS_CURRENT
	fi
	sleep 1
done

wait "$SIEGE_PID"
SIEGE_STATUS=$?
SIEGE_PID=
cat "$SIEGE_LOG"

RSS_END=$(awk '/VmRSS:/ { print $2 }' "/proc/$SERVER_PID/status")
if [ -n "$RSS_END" ] && [ "$RSS_END" -gt "$RSS_MAX" ]; then
	RSS_MAX=$RSS_END
fi
AVAILABILITY=$(awk '
	/Availability:/ { value=$2 }
	/"availability":/ { value=$2 }
	END { gsub(/[% ,]/, "", value); print value }
' "$SIEGE_LOG")
FAILED=$(awk '
	/Failed transactions:/ { value=$3 }
	/"failed_transactions":/ { value=$2 }
	END { gsub(/[,]/, "", value); print value }
' "$SIEGE_LOG")

printf 'webserv RSS: start=%s kB max=%s kB end=%s kB\n' "$RSS_START" "$RSS_MAX" "$RSS_END"

if [ "$SIEGE_STATUS" -ne 0 ] || [ -z "$AVAILABILITY" ]; then
	printf 'ERROR: Siege did not finish successfully.\n' >&2
	exit 1
fi
if ! awk -v value="$AVAILABILITY" 'BEGIN { exit !(value + 0 >= 99.5) }'; then
	printf 'ERROR: Availability %s%% is below 99.5%%.\n' "$AVAILABILITY" >&2
	exit 1
fi
if [ -n "$FAILED" ] && [ "$FAILED" -ne 0 ]; then
	printf 'ERROR: Siege reported %s failed transactions.\n' "$FAILED" >&2
	exit 1
fi

printf 'PASS: Siege availability=%s%% failed=%s\n' "$AVAILABILITY" "${FAILED:-unknown}"
