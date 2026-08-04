*This project has been created as part of the 42 curriculum by WhizdomConjurer, Tjkruger and mpoplow.*

# webserv

## Description

`webserv` is an HTTP server written in C++98. It accepts multiple listening
sockets, parses HTTP/1.0 and HTTP/1.1 requests, applies server and route
configuration, and returns static or CGI-generated responses.

The server uses one central `poll()` loop for listener sockets, client sockets,
and CGI pipes. Network and pipe descriptors are non-blocking. Each client moves
through explicit request-reading, CGI-processing, and response-writing states,
so a slow client or CGI process does not block the rest of the server.

Mandatory features include:

- multiple configured interface and port pairs;
- server selection by `Host` when server names share a listener;
- static file serving and MIME types;
- custom and default error pages;
- route-specific methods, roots, indexes, redirects, and autoindex;
- binary uploads with configured storage locations and body-size limits;
- `DELETE` for regular files;
- extension-based CGI with query strings, request bodies, `PATH_INFO`, and
  CGI environment variables;
- chunked request decoding before CGI execution;
- non-blocking CGI input/output, process cleanup, and timeouts;
- malformed-request, traversal, disconnect, and stress handling.

## Instructions

### Build

```sh
make
```

The Makefile compiles with:

```text
c++ -Wall -Wextra -Werror -std=c++98
```

Available rules are `all`, `clean`, `fclean`, `re`, `test-eval`, `test-core`,
`test-bonus`, `test-quick`, and `test-siege`.

### Linux / 42 setup

Linux is the reference environment. The project only uses C++98 and POSIX APIs
available on the 42 Linux machines (`poll`, sockets, pipes, `fcntl`, `fork`,
`execve`, and `waitpid`). It contains no macOS-only framework or compiler flag.

On Debian or Ubuntu, the complete evaluation toolset can be installed with:

```sh
sudo apt update
sudo apt install -y build-essential python3 curl siege
```

Then build from a clean tree and run the mandatory tests:

```sh
make fclean
make
make test-core
```

If Docker is available, `Dockerfile.linux` provides a reproducible Debian Linux
compile check from any host:

```sh
docker build --no-cache --platform linux/amd64 \
  -f Dockerfile.linux -t webserv-linux-check .
```

The image is intentionally only a Linux/GCC compile check; submission and
evaluation use the native Makefile. On macOS, the Siege command from the
evaluation sheet can instead be installed with `brew install siege`.

### Run

Start with the default configuration:

```sh
./webserv
```

Or provide another configuration file:

```sh
./webserv configs/default.conf
```

The default site is then available at:

```text
http://127.0.0.1:8080/
```

Stop the server with `Ctrl-C`.

### Configuration

The configuration syntax is inspired by an NGINX `server` block:

```nginx
server {
    listen 8080;
    host 127.0.0.1;
    root www;
    index index.html;
    client_max_body_size 1000000;

    error_page 404 /error_pages/404.html;

    location / {
        methods GET;
        root www;
        index index.html;
        autoindex off;
    }

    location /upload {
        methods GET POST DELETE;
        root uploads;
        upload_enable on;
        upload_path uploads;
    }

    location /cgi-bin {
        methods GET POST;
        root cgi-bin;
        cgi .py /usr/bin/python3;
    }
}
```

Additional focused examples are provided under `configs/` and
`tests/configs/` for static routes, redirects, autoindex, uploads, deletion,
CGI, multiple ports, interface binding, and server-name selection.

## Testing

Run the complete subject-oriented suite:

```sh
make test-eval
```

Run only the mandatory sections:

```sh
make test-core
```

Run a shorter mandatory regression suite:

```sh
make test-quick
```

The mandatory suite checks source restrictions, invalid configurations, HTTP
parsing, static responses, redirects, autoindex, uploads, deletion, multiple
listeners, interface binding, server selection, 100 parallel clients, CGI,
chunked request bodies, slow CGI concurrency, and CGI timeouts.

Useful manual checks:

```sh
curl -i http://127.0.0.1:8080/
curl -i http://127.0.0.1:8080/not-found
curl -i -X POST --data-binary 'hello' \
  http://127.0.0.1:8080/upload/manual.txt
curl -i http://127.0.0.1:8080/upload/manual.txt
curl -i -X DELETE http://127.0.0.1:8080/upload/manual.txt
curl -i 'http://127.0.0.1:8080/cgi-bin/env.py?source=manual'
curl -i -X POST --data-binary 'hello' \
  http://127.0.0.1:8080/cgi-bin/evaluation_echo.py
```

Before submission, also open the site in a standard browser and run a leak
check on the target 42 machine.

## Architecture

`ServerManager::eventLoop()` rebuilds one `pollfd` collection on every loop:

- listeners request `POLLIN` and accept all currently pending clients;
- clients request `POLLIN` while receiving a request;
- CGI stdin pipes request `POLLOUT` while request-body bytes remain;
- CGI stdout pipes request `POLLIN` until EOF;
- clients request `POLLOUT` while a response remains to be sent.

The server performs socket and pipe reads or writes only after the corresponding
poll event. Partial sends and CGI writes retain their byte offsets for the next
poll cycle. CGI children are checked and reaped with non-blocking `waitpid()`;
hung children are killed after the CGI timeout.

## Evaluation defense Q&A

The answers below describe the current implementation, not an intended future
design. Function names are included so each answer can be demonstrated directly
in the source.

### General and networking

**What is an HTTP server?**

An HTTP server listens on one or more TCP sockets, accepts client connections,
parses HTTP requests, chooses a configured resource or handler, and serializes
an HTTP response. This project implements that flow itself; it does not call an
external web server.

**Which I/O multiplexing function is used?**

Only `poll()` is used. `ServerManager::eventLoop()` in
`src/server/server_manager.cpp` builds the central descriptor set. There is no
`select()` call. Listener sockets, client sockets, CGI stdin pipes, and CGI
stdout pipes are all registered in that same loop.

**How does `poll()` work?**

The program passes an array of `pollfd` entries to `poll()`. Every entry holds a
file descriptor and the events the program currently wants, such as `POLLIN` or
`POLLOUT`. `poll()` sleeps until at least one descriptor is ready or the timeout
expires, then places the observed events in `revents`. The server performs only
the matching non-blocking operation and returns to the loop. Unlike `select()`,
`poll()` does not use fixed-size descriptor bitsets and the descriptor sets do
not have to be rebuilt after the call modifies them.

**How are accept, read, and write handled with one `poll()`?**

- A listener is registered for `POLLIN`; readiness calls
  `ServerManager::acceptNewClients()`.
- A client receiving a request is registered for `POLLIN`; readiness calls
  `ServerManager::handleClientReadable()` once for that client in that loop
  iteration.
- A client with a completed response is registered for `POLLOUT`; readiness
  calls `ServerManager::handleClientWritable()` once and retains the offset
  after a partial send.
- CGI stdin and stdout use the same rule through
  `handleCgiInputWritable()` and `handleCgiOutputReadable()`.

The main loop never registers the same client socket for request reading and
response writing at the same time: the client's state determines the requested
event. It does, however, inspect both `POLLIN` and `POLLOUT` results for the
whole descriptor set in every iteration.

**Are socket and CGI-pipe operations non-blocking and poll-driven?**

Yes. Listener sockets, accepted client sockets, and both parent-side CGI pipe
ends are set to `O_NONBLOCK`. Socket and pipe `recv`, `send`, `read`, and
`write` calls occur only after the corresponding readiness event. Regular disk
file operations are exempt from this subject rule.

**What happens when `recv`, `send`, `read`, or `write` returns an error?**

Client `recv` or `send` results of zero or less close and remove that client.
For CGI pipes, EOF closes the relevant pipe and finalizes the CGI response;
other failed operations close/fail the CGI request and clean up its child and
descriptors. The implementation does not inspect `errno` after these I/O calls.

**Does the Makefile relink unnecessarily?**

No. Object files are rebuilt only when their source or generated header
dependency changes. Running `make` twice leaves `webserv` unchanged. `re` is an
explicit clean rebuild.

### Configuration

**How are multiple ports and virtual hosts handled?**

Each unique configured interface/port pair gets one listening socket. Different
ports can run simultaneously. Server blocks with the same interface and port
share that listener and are selected from the request's `Host` header by
`ServerManager::selectServer()`; the first block is the fallback.

```sh
./webserv tests/configs/evaluation_multi_port.conf
curl -i http://127.0.0.1:18080/
curl -i http://127.0.0.1:18081/

./webserv tests/configs/evaluation_virtual_hosts.conf
curl --resolve alpha.local:18088:127.0.0.1 http://alpha.local:18088/
curl --resolve beta.local:18088:127.0.0.1 http://beta.local:18088/
```

Distinct names on one port are deliberately valid virtual hosts and share one
listener. The operating system still permits only one listening socket for the
same interface/port without special reuse semantics: a second webserv process
trying to bind the occupied interface/port fails and exits if it has no usable
listener. This is the distinction to explain if the evaluator asks why two
named server blocks can share a port while two independent processes cannot.

**Which HTTP status codes can the server return?**

The current server-generated response paths use `200 OK`, `201 Created`, `204
No Content`, `302 Found`, `400 Bad Request`, `403 Forbidden`, `404 Not Found`,
`405 Method Not Allowed`, `413 Content Too Large`, `500 Internal Server Error`,
`502 Bad Gateway`, `504 Gateway Timeout`, and `505 HTTP Version Not Supported`.
A CGI program may also provide a valid `Status` header. `src/utils.cpp` contains
reason phrases for a wider set of standard codes, but that mapping alone does
not mean every code is generated by a current request path. Route handlers
select the actual status in
`src/server/server_manager.cpp`.

**Are custom error pages supported?**

Yes. `error_page <status> <path>;` maps an error status to a file. If that file
cannot be read, a built-in error body is generated so the server can still
answer.

**How is the maximum request body size enforced?**

`client_max_body_size` is configured per server. The server rejects a declared
oversized `Content-Length` before reading the full body and also checks the
decoded body while receiving fixed-length or chunked input. The response is
`413 Content Too Large`.

**How do routes work?**

The matching location can define its own root, index, allowed methods,
autoindex, redirect, upload directory, and CGI extension/interpreter mapping.
Thus different URLs can map to different directories. A directory request uses
the configured index; without an index it returns a generated listing only when
`autoindex on` is set, otherwise `403`.

**How are methods and deletion handled?**

Each location has an explicit allowed-method list. A known but disallowed method
returns `405`; malformed or unsupported request forms do not crash the server.
`DELETE` removes a regular file only when the route permits it and filesystem
permissions allow it. Directories and traversal attempts are rejected.

### Basic HTTP checks

The mandatory suite exercises `GET`, `POST`, and `DELETE`, malformed methods,
correct status codes, binary upload, body limits, abrupt disconnects, and 100
parallel clients. Run it with:

```sh
make test-core
```

With the default configuration, this live sequence uploads, retrieves, and
deletes the same bytes:

```sh
printf '\000\001hello\377' > /tmp/webserv-upload.bin
curl -i --data-binary @/tmp/webserv-upload.bin \
  http://127.0.0.1:8080/upload/webserv-upload.bin
curl -o /tmp/webserv-download.bin \
  http://127.0.0.1:8080/upload/webserv-upload.bin
cmp /tmp/webserv-upload.bin /tmp/webserv-download.bin
curl -i -X DELETE \
  http://127.0.0.1:8080/upload/webserv-upload.bin
```

### CGI

CGI is selected only for configured file extensions in the configured CGI
location. The server prepares CGI environment variables, passes a POST body to
CGI stdin, reads CGI stdout, and converts its headers/body into an HTTP
response. GET query strings, POST bodies, `PATH_INFO`, Python CGI, and shell CGI
are covered by the tests.

CGI is a state machine inside the central `poll()` loop. A slow CGI does not
block static clients. Child state is observed with `waitpid(..., WNOHANG)`;
invalid output or execution failure returns `502`, and a hanging CGI is killed
after the timeout and returns `504`. Missing scripts return `404`. These errors
do not terminate the server.

### Browser and Siege checks

In a browser, use the Network panel to verify the status line, response headers,
body, missing-page response, redirect, and autoindex listing. The default page
at `http://127.0.0.1:8080/` links to the available demonstrations.

After installing Siege, start the server in one terminal and run in another:

```sh
siege --version
siege -b -c 50 -t 30S http://127.0.0.1:8080/
```

Or let the repository start the server, enforce the 99.5% threshold, and sample
Linux RSS memory automatically:

```sh
make test-siege
```

The evaluation target is more than 99.5% successful requests with no hangs and
no indefinitely increasing memory use. Keep a longer run active while checking
the process with the leak tool available on the evaluation Linux machine.

## Cookies and sessions (bonus)

A cookie is a small `name=value` item associated with a website. The server
sends it in a `Set-Cookie` response header; the browser stores it and sends it
back in the `Cookie` request header on later matching requests. Cookies let a
stateless HTTP exchange recognize a returning browser. Common uses are a
session identifier, login state, language/theme preferences, and shopping-cart
identity.

Sensitive data such as passwords should not be stored directly in a cookie. A
typical session cookie contains only a hard-to-guess session ID while the actual
session data remains on the server. Useful attributes include `Path`,
`Max-Age`/`Expires`, `HttpOnly`, `Secure`, and `SameSite`.

In this project, `CgiHandler::initEnv()` forwards the incoming `Cookie` header
as `HTTP_COOKIE`, and CGI response normalization forwards `Set-Cookie` back to
the client. The bonus demonstrations are:

- `cgi-bin/bonus_set_cookie.py`: creates a cookie;
- `cgi-bin/bonus_read_cookie.py`: reads it on a later request;
- `cgi-bin/bonus_session.py`: stores a counter server-side under a random
  session ID;
- `www/bonus.html`: browser interface for the cookie/session demonstrations.

Run `make test-bonus` to verify cookies, sessions, Python CGI, and shell CGI.

## Resources

- [RFC 9110 - HTTP Semantics](https://www.rfc-editor.org/rfc/rfc9110)
- [RFC 9112 - HTTP/1.1](https://www.rfc-editor.org/rfc/rfc9112)
- [RFC 3875 - CGI/1.1](https://www.rfc-editor.org/rfc/rfc3875)
- [IANA HTTP Status Code Registry](https://www.iana.org/assignments/http-status-codes/http-status-codes.xhtml)
- [poll(2)](https://man7.org/linux/man-pages/man2/poll.2.html)
- [NGINX request processing](https://nginx.org/en/docs/http/request_processing.html)

AI was used to help organize subject requirements, review event-loop and CGI
state transitions, generate edge-case test ideas, and improve documentation.
All generated suggestions were reviewed against the local code, the official
subject, and the repository's executable test suite before being retained.
