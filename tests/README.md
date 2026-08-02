# webserv subject and evaluation tests

This directory contains a repeatable audit suite for the mandatory and bonus
requirements of the 42 `webserv` subject. The suite deliberately exits with a
non-zero status while requirements are still failing.

## One-command runs

Run the complete evaluation, including the slow CGI and 100-client stress
checks:

```sh
make test-eval
```

Run mandatory checks only:

```sh
make test-core
```

Run bonus checks only:

```sh
make test-bonus
```

Run a shorter development loop without the CGI timeout and concurrency checks:

```sh
make test-quick
```

Individual sections can also be selected after building:

```sh
make re
python3 tests/evaluation_suite.py --section config
python3 tests/evaluation_suite.py --section core
python3 tests/evaluation_suite.py --section cgi
python3 tests/evaluation_suite.py --section bonus
```

The suite uses ports `8080` and `18080` through `18088`. Stop another service
using one of these ports before starting the tests. Temporary uploads, session
files, server processes, and logs are cleaned up automatically. If a test runner
itself crashes, diagnostic server logs remain in `/tmp/webserv-evaluation-*.log`.

## What each section proves

| Section | Main subject coverage |
| --- | --- |
| `source` | exact README name/content, one poll-equivalent, macOS-safe `fcntl`, allowed-function risks, forbidden `errno` control after I/O |
| `config` | missing/bad files, missing directives, unknown directives, duplicates, invalid status codes, malformed scopes |
| `core` | static GET, error pages, methods, malformed requests, body limit, upload, DELETE, redirect, autoindex, parallel clients, multiple ports, configured interfaces, bind failures |
| `cgi` | GET/POST, body and environment propagation, working directory, chunk decoding and fragmentation, PATH_INFO, missing scripts, timeout, non-blocking behavior |
| `bonus` | two CGI interpreter types, CGI extension matching, cookies, persistent session state, browser demonstration page |

## Evaluation demonstration order

1. Run `make re` and show that the project compiles with
   `-Wall -Wextra -Werror -std=c++98`.
2. Run `make test-core`. Any `FAIL` is an unresolved mandatory requirement and
   should be fixed before claiming the bonus.
3. Start `./webserv configs/default.conf` in a separate terminal and open
   `http://localhost:8080/` in a browser. Demonstrate a static page, a missing
   page, an upload, the uploaded file, and DELETE.
4. Open `http://localhost:8080/bonus.html` and demonstrate both CGI languages,
   cookie forwarding, and the session counter.
5. Stop the manually started server and run `make test-bonus`.
6. Keep `tests/configs/` open during the evaluation so every tested server,
   route, port, method, redirect, autoindex, upload, and CGI rule is visible.

Do not describe the project as subject-complete merely because a browser smoke
test succeeds. The mandatory part must pass before bonus points can be assessed.

## Adding a new regression test

1. Add a minimal fixture under `tests/fixtures/`, a CGI under `cgi-bin/`, or a
   dedicated configuration under `tests/configs/`.
2. Start that configuration with `ManagedServer` in
   `tests/evaluation_suite.py`. Use a unique port in the `18080` range.
3. Send a request with `http_request()` for normal HTTP or `raw_request()` for
   malformed, chunked, or fragmented traffic.
4. Record the exact expected status, header, body, timing, or filesystem effect
   with `BOOK.check()`.
5. Remove created files in a `finally` block so a failing assertion cannot dirty
   the repository.
6. First run only the relevant section, then run `make test-eval` to check for
   interference with other server configurations.

Example:

```python
response = http_request(18082, "GET", "/missing")
BOOK.check("missing file returns 404", response.status == 404,
           response.status, 404)
```

Tests should assert observable behavior, not internal implementation details,
except for explicit source-level subject rules such as the single
poll-equivalent requirement and the allowed function list.
