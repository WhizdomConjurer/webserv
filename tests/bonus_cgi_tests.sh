#!/bin/sh

set -u

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SERVER_URL="http://localhost:8080"
COOKIE_JAR="/tmp/webserv_bonus_cookie_$$.txt"
SESSION_JAR="/tmp/webserv_bonus_session_$$.txt"
SERVER_LOG="/tmp/webserv_bonus_server_$$.log"
SERVER_PID=""
SESSION_FILE=""
FAILURES=0

cleanup()
{
	if [ -n "$SERVER_PID" ]; then
		kill "$SERVER_PID" 2>/dev/null
		wait "$SERVER_PID" 2>/dev/null
	fi
	rm -f "$COOKIE_JAR" "$SESSION_JAR" "$SERVER_LOG"
	if [ -n "$SESSION_FILE" ]; then
		rm -f "$SESSION_FILE"
	fi
}

trap cleanup EXIT INT TERM

pass()
{
	printf 'PASS: %s\n' "$1"
}

fail()
{
	printf 'FAIL: %s\n' "$1"
	FAILURES=$((FAILURES + 1))
}

assert_contains()
{
	label=$1
	haystack=$2
	needle=$3
	if printf '%s' "$haystack" | grep -F "$needle" >/dev/null; then
		pass "$label"
	else
		fail "$label"
		printf 'Expected to find: %s\n' "$needle"
	fi
}

assert_not_contains()
{
	label=$1
	haystack=$2
	needle=$3
	if printf '%s' "$haystack" | grep -F "$needle" >/dev/null; then
		fail "$label"
		printf 'Did not expect to find: %s\n' "$needle"
	else
		pass "$label"
	fi
}

cd "$ROOT_DIR" || exit 1

make >/dev/null || exit 1
./webserv configs/default.conf >"$SERVER_LOG" 2>&1 &
SERVER_PID=$!

sleep 1
if ! kill -0 "$SERVER_PID" 2>/dev/null; then
	printf 'Server did not start. Log:\n'
	cat "$SERVER_LOG"
	exit 1
fi

python_response=$(curl -sS -i "$SERVER_URL/cgi-bin/bonus_python.py?source=test")
assert_contains "Python CGI returns 200" "$python_response" "HTTP/1.1 200 OK"
assert_contains "Python CGI used Python script" "$python_response" "Python CGI works"
assert_contains "Python CGI received query string" "$python_response" "source=test"

shell_response=$(curl -sS -i "$SERVER_URL/cgi-bin/bonus_shell.sh?source=test")
assert_contains "Shell CGI returns 200" "$shell_response" "HTTP/1.1 200 OK"
assert_contains "Shell CGI used shell script" "$shell_response" "Shell CGI works"
assert_contains "Shell CGI received query string" "$shell_response" "source=test"

wrong_ext_response=$(curl -sS -i "$SERVER_URL/cgi-bin/not_cgi.txt")
assert_contains "Wrong extension is served as a static file" "$wrong_ext_response" "HTTP/1.1 200 OK"
assert_contains "Wrong extension is not executed as CGI" "$wrong_ext_response" "not configured as CGI"

set_cookie_response=$(curl -sS -i -c "$COOKIE_JAR" "$SERVER_URL/cgi-bin/bonus_set_cookie.py?value=from-test")
assert_contains "Set cookie returns 200" "$set_cookie_response" "HTTP/1.1 200 OK"
assert_contains "Set-Cookie header is forwarded" "$set_cookie_response" "Set-Cookie: bonus_cookie=from-test"

read_cookie_response=$(curl -sS -i -b "$COOKIE_JAR" "$SERVER_URL/cgi-bin/bonus_read_cookie.py")
assert_contains "Read cookie returns 200" "$read_cookie_response" "HTTP/1.1 200 OK"
assert_contains "Read cookie sees stored value" "$read_cookie_response" "from-test"

session_first=$(curl -sS -i -c "$SESSION_JAR" -b "$SESSION_JAR" "$SERVER_URL/cgi-bin/bonus_session.py")
assert_contains "Session first request returns 200" "$session_first" "HTTP/1.1 200 OK"
assert_contains "Session sets cookie" "$session_first" "Set-Cookie: bonus_sid="
assert_contains "Session counter starts at one" "$session_first" "1</strong> time"
session_id=$(printf '%s' "$session_first" | sed -n 's/.*Set-Cookie: bonus_sid=\([^;]*\).*/\1/p' | head -n 1)
if [ -n "$session_id" ]; then
	SESSION_FILE="$ROOT_DIR/cgi-bin/bonus_sessions/session_$session_id"
fi

session_second=$(curl -sS -i -c "$SESSION_JAR" -b "$SESSION_JAR" "$SERVER_URL/cgi-bin/bonus_session.py")
assert_contains "Session second request returns 200" "$session_second" "HTTP/1.1 200 OK"
assert_contains "Session counter increments" "$session_second" "2</strong> time"

if [ "$FAILURES" -ne 0 ]; then
	printf '\n%d bonus test(s) failed.\n' "$FAILURES"
	exit 1
fi

printf '\nAll bonus CGI tests passed.\n'
