*This project has been created as part of the 42 curriculum by <login1>[, <login2>[, <login3>[...]]].*

# webserv

## Description

`webserv` is a 42 project whose goal is to write an HTTP server in C++98.
The server must be usable with a real browser and must implement the core
behavior expected from a small web server: accepting network connections,
parsing HTTP requests, selecting the correct configuration, serving static
files, handling uploads, executing CGI programs, returning accurate status
codes, and staying available under stress.

This project is not about using an existing web server. The point is to
understand what a web server does internally and to implement the required
parts ourselves.

## What Is HTTP?

HTTP means **Hypertext Transfer Protocol**. It is the application protocol used
by browsers, crawlers, command-line tools like `curl`, and web servers to
exchange data on the web.

A normal HTTP exchange looks like this:

1. A client opens a TCP connection to the server.
2. The client sends an HTTP request.
3. The server reads and parses the request.
4. The server decides which resource or behavior matches the request.
5. The server sends an HTTP response.
6. The client reads the response and displays or processes it.

An HTTP request usually contains:

- a request line, for example `GET /index.html HTTP/1.1`
- headers, for example `Host`, `Content-Length`, or `Content-Type`
- an optional body, for example form data in a `POST` request

An HTTP response usually contains:

- a status line, for example `HTTP/1.1 200 OK`
- headers, for example `Content-Type` or `Content-Length`
- an optional body, for example HTML, JSON, an image, or an error page

HTTP is important because it gives both sides a shared format. The client knows
how to ask for something, and the server knows how to answer in a way that the
client can understand.

## What Is A Web Server?

A web server is a program that waits for network connections and answers HTTP
requests.

In this project, `webserv` must be able to:

- create and configure listening sockets
- listen on one or more interface:port pairs
- accept client connections
- keep sockets non-blocking
- monitor all client and server I/O through one polling mechanism
- read incoming HTTP requests
- parse request lines, headers, and bodies
- choose the correct server and route configuration
- serve static files
- generate directory listings when enabled
- use default index files for directories
- receive client uploads
- delete resources when allowed
- send redirects
- execute CGI programs
- generate default error pages
- return accurate HTTP status codes
- stay available during stress tests

## What Is CGI?

CGI means **Common Gateway Interface**. It is a standard way for a web server to
communicate with an external program or script.

Without CGI, a server mostly returns existing files:

- `index.html`
- `style.css`
- `image.png`

With CGI, the server can create dynamic responses. The server starts a program,
passes request information to it through environment variables and standard
input, then sends the program output back to the client as an HTTP response.

Example CGI flow:

1. The browser requests `/cgi-bin/script.py`.
2. The server detects that this file extension is configured as CGI.
3. The server prepares CGI environment variables.
4. For requests with a body, the server sends the body to the CGI process.
5. The CGI program writes its response to `stdout`.
6. The server reads this output.
7. The server forwards the CGI response to the client.

CGI is used for dynamic content such as:

- form handling
- generated HTML pages
- small API responses
- upload processing
- scripts written in Python, PHP, or another supported CGI language

Important CGI details from the subject:

- CGI execution must be based on file extension, for example `.php` or `.py`.
- The full request and its arguments must be available to the CGI program.
- CGI environment variables must be set correctly.
- For chunked requests, the server must unchunk the body before giving it to CGI.
- CGI expects EOF as the end of the body.
- If CGI output has no `Content-Length`, EOF marks the end of the CGI response.
- CGI must run in the correct directory so relative paths work.
- The project must support at least one CGI type.

## Instructions

### Build

```sh
make
```

The Makefile must provide at least these rules:

```sh
make
make all
make clean
make fclean
make re
```

The project must compile with:

```sh
c++ -Wall -Wextra -Werror
```

It must also remain compatible with C++98:

```sh
c++ -Wall -Wextra -Werror -std=c++98
```

### Run

The executable must be named:

```sh
webserv
```

It must run like this:

```sh
./webserv [configuration file]
```

If no configuration file is provided, the program must use a default path.

### Quick Manual Tests

```sh
curl -v http://localhost:8080/
curl -v -X POST http://localhost:8080/upload -d "hello"
curl -v -X DELETE http://localhost:8080/file.txt
curl -v -H "Host: example.com" http://localhost:8080/
curl --resolve example.com:8080:127.0.0.1 http://example.com:8080/
```

Browser tests are also required because the subject explicitly expects the
server to be compatible with a standard web browser of your choice.

## Subject Checklist

This section is a detailed checklist based on the `webserv` subject. It is meant
to help team members track what must be implemented, tested, and demonstrated
during evaluation.

### 1. Repository And Submitted Files

- [ ] The repository contains all required project files.
- [ ] Submitted files include a `Makefile`.
- [ ] Submitted files include C++ source files: `*.cpp`.
- [ ] Submitted files include headers where needed: `*.h` and/or `*.hpp`.
- [ ] Submitted files include template files where needed: `*.tpp` and/or `*.ipp`.
- [ ] Submitted files include configuration files.
- [ ] The executable produced by the Makefile is named `webserv`.
- [ ] Only repository content is required for evaluation.
- [ ] File names are double-checked before submission.

### 2. General Rules

- [ ] The program must not crash under any circumstances.
- [ ] The program must not terminate unexpectedly.
- [ ] The program must handle errors cleanly, including severe cases such as
      allocation failure as well as reasonably possible.
- [ ] The Makefile must not perform unnecessary relinking.
- [ ] The Makefile must contain the rules `$(NAME)`, `all`, `clean`, `fclean`,
      and `re`.
- [ ] The project must compile with `c++`.
- [ ] The project must compile with `-Wall -Wextra -Werror`.
- [ ] The project must comply with the C++98 standard.
- [ ] The project should still compile when `-std=c++98` is added.
- [ ] C++ features should be preferred over C-style alternatives where possible.
- [ ] Example: prefer `<cstring>` over `<string.h>`.
- [ ] External libraries are forbidden.
- [ ] Boost is forbidden.
- [ ] Libft is not authorized and not needed.

### 3. Program Interface

- [ ] Program name is exactly `webserv`.
- [ ] The program can be started with a configuration file argument:
      `./webserv path/to/config`.
- [ ] The program can also run using a default configuration path when no
      argument is provided.
- [ ] Invalid arguments are handled with a clear error message.
- [ ] Missing or invalid configuration files are handled without crashing.
- [ ] The server starts listening after configuration parsing succeeds.

### 4. Authorized Functions

Only the subject-authorized external functions should be used.

- [ ] `execve`
- [ ] `pipe`
- [ ] `strerror`
- [ ] `gai_strerror`
- [ ] `errno`
- [ ] `dup`
- [ ] `dup2`
- [ ] `fork`
- [ ] `socketpair`
- [ ] `htons`
- [ ] `htonl`
- [ ] `ntohs`
- [ ] `ntohl`
- [ ] `select`
- [ ] `poll`
- [ ] `epoll_create`
- [ ] `epoll_ctl`
- [ ] `epoll_wait`
- [ ] `kqueue`
- [ ] `kevent`
- [ ] `socket`
- [ ] `accept`
- [ ] `listen`
- [ ] `send`
- [ ] `recv`
- [ ] `chdir`
- [ ] `bind`
- [ ] `connect`
- [ ] `getaddrinfo`
- [ ] `freeaddrinfo`
- [ ] `setsockopt`
- [ ] `getsockname`
- [ ] `getprotobyname`
- [ ] `fcntl`
- [ ] `close`
- [ ] `read`
- [ ] `write`
- [ ] `waitpid`
- [ ] `kill`
- [ ] `signal`
- [ ] `access`
- [ ] `stat`
- [ ] `open`
- [ ] `opendir`
- [ ] `readdir`
- [ ] `closedir`

Implementation note:

- [ ] `fork()` must be used only for CGI.
- [ ] It is forbidden to `execve()` another web server.

### 5. Event Loop And Non-Blocking I/O

The non-blocking I/O model is one of the most important grading points.

- [ ] The server must remain non-blocking at all times.
- [ ] The server must properly handle client disconnections.
- [ ] The server must use one `poll()` or equivalent mechanism for all I/O
      between clients and the server.
- [ ] The listening sockets must also be monitored by this same polling
      mechanism.
- [ ] The chosen mechanism may be `poll()`, `select()`, `kqueue()`, or `epoll()`.
- [ ] Read readiness and write readiness must both be monitored.
- [ ] The server must never call `read()` or `recv()` on sockets, pipes, or FIFOs
      unless readiness was reported by the polling mechanism.
- [ ] The server must never call `write()` or `send()` on sockets, pipes, or
      FIFOs unless readiness was reported by the polling mechanism.
- [ ] Regular disk files are exempt from polling readiness requirements.
- [ ] I/O that can wait for data, such as sockets and pipes, must be non-blocking.
- [ ] A request must never hang forever.
- [ ] Timeouts or cleanup logic must remove stuck requests or dead clients.
- [ ] After `read()` or `write()`, the server must not check `errno` to decide
      normal server behavior.
- [ ] All file descriptors that can block should be set to non-blocking mode.
- [ ] Poll events must be updated depending on whether a client needs reading,
      writing, or both.

### 6. macOS-Specific Rules

Because macOS handles `write()` differently from other Unix systems, the subject
allows `fcntl()` with strict limitations.

- [ ] On macOS, file descriptors must be put in non-blocking mode.
- [ ] `fcntl()` may be used only with the allowed flags.
- [ ] Allowed `fcntl()` flags are `F_SETFL`, `O_NONBLOCK`, and `FD_CLOEXEC`.
- [ ] Any other `fcntl()` flag is forbidden by the subject.

### 7. HTTP Request Parsing

- [ ] Parse the request line.
- [ ] Extract the HTTP method.
- [ ] Extract the request target URI.
- [ ] Extract the HTTP version.
- [ ] Parse request headers.
- [ ] Support request bodies.
- [ ] Correctly handle `Content-Length`.
- [ ] Correctly handle body size limits.
- [ ] Correctly handle malformed requests.
- [ ] Correctly handle unsupported methods.
- [ ] Correctly handle unknown routes.
- [ ] Correctly handle URL paths and route matching.
- [ ] Decode or normalize paths as needed to safely map URLs to files.
- [ ] Prevent path traversal such as `../` escaping the configured root.
- [ ] Requests must not hang indefinitely, even if incomplete or malformed.

### 8. HTTP Response Generation

- [ ] Generate valid HTTP responses.
- [ ] Return accurate HTTP status codes.
- [ ] Include required headers.
- [ ] Include correct `Content-Length` when known.
- [ ] Include suitable `Content-Type` where possible.
- [ ] Send response bodies for successful file responses.
- [ ] Send response bodies for error pages.
- [ ] Provide default error pages when no custom error page is configured.
- [ ] Close or keep connections consistently according to implemented behavior.
- [ ] Compare behavior with NGINX when a specific HTTP behavior is unclear.

### 9. Required HTTP Methods

The subject requires at least these methods:

- [ ] `GET`
- [ ] `POST`
- [ ] `DELETE`

For each configured route:

- [ ] Check whether the method is allowed.
- [ ] Return `405 Method Not Allowed` or another accurate status when the method
      is not accepted.
- [ ] Ensure method restrictions are route-specific.

### 10. Static Website Serving

- [ ] The server must be able to serve a fully static website.
- [ ] Static HTML files can be served.
- [ ] CSS files can be served.
- [ ] JavaScript files can be served.
- [ ] Images and other assets can be served.
- [ ] Missing files return an accurate error status.
- [ ] Forbidden files or directories return an accurate error status.
- [ ] Directory requests use the configured default file when available.
- [ ] Directory listing is generated only when enabled.
- [ ] Directory listing is disabled when configured off.

### 11. Uploads

- [ ] Clients must be able to upload files.
- [ ] Uploads must only be allowed where the configuration permits them.
- [ ] The upload storage location must come from the configuration.
- [ ] Uploaded file data must be written safely.
- [ ] Body size limits must be enforced.
- [ ] Oversized upload requests must receive an accurate status code.
- [ ] Upload errors must not crash the server.
- [ ] Upload behavior must be demonstrable during evaluation.

### 12. DELETE Behavior

- [ ] `DELETE` requests must be supported.
- [ ] Deleting must obey route method restrictions.
- [ ] Deleting must not escape the configured root.
- [ ] Successful deletion returns an accurate success status.
- [ ] Missing resources return an accurate error status.
- [ ] Forbidden deletion returns an accurate error status.
- [ ] Delete errors must not crash the server.

### 13. Multiple Ports And Multiple Websites

- [ ] The server must be able to listen on multiple ports.
- [ ] Different ports can deliver different content.
- [ ] The configuration can define all interface:port pairs.
- [ ] Multiple listening sockets are managed by the same event loop.
- [ ] The listening sockets are monitored by the same `poll()` or equivalent call.
- [ ] The subject says virtual hosts are out of scope.
- [ ] Virtual hosts may be implemented optionally, but they are not required.

### 14. Configuration File

The configuration may be inspired by the `server` section of NGINX.

Required configuration capabilities:

- [ ] Define every interface:port pair on which the server listens.
- [ ] Define multiple websites served by the program.
- [ ] Configure default error pages.
- [ ] Configure the maximum allowed client request body size.
- [ ] Configure route-specific rules.
- [ ] Configure accepted HTTP methods per route.
- [ ] Configure HTTP redirection.
- [ ] Configure the root directory for requested files.
- [ ] Configure directory listing on or off.
- [ ] Configure the default file served for directory requests.
- [ ] Configure whether uploads are allowed.
- [ ] Configure the upload storage location.
- [ ] Configure CGI execution by file extension.
- [ ] Provide enough configuration files to demonstrate all required features.
- [ ] Handle syntax errors without crashing.
- [ ] Handle missing required values without crashing.
- [ ] Handle duplicate or conflicting settings in a defined way.

Route example from the subject:

- [ ] If URL `/kapouet` is rooted to `/tmp/www`, then request
      `/kapouet/pouic/toto/pouet` must map to
      `/tmp/www/pouic/toto/pouet`.

Optional configuration capability:

- [ ] A server name may be added if the team chooses to implement virtual hosts.

### 15. Redirections

- [ ] A route can be configured to redirect.
- [ ] Redirection returns an accurate 3xx status code.
- [ ] Redirection includes the correct `Location` header.
- [ ] Redirection does not try to serve a file from the route root.
- [ ] Redirection behavior is demonstrable with `curl -v`.

### 16. Directory Listing And Index Files

- [ ] Directory listing can be enabled per route.
- [ ] Directory listing can be disabled per route.
- [ ] If a directory is requested and an index/default file exists, serve it.
- [ ] If no index exists and directory listing is enabled, generate a listing.
- [ ] If no index exists and directory listing is disabled, return an accurate
      error status.
- [ ] Generated listings must not expose paths outside the configured root.

### 17. CGI Implementation

- [ ] CGI execution is selected by file extension.
- [ ] At least one CGI type is supported, for example Python or PHP-CGI.
- [ ] CGI is executed only for routes/files configured for CGI.
- [ ] `fork()` is used only for CGI.
- [ ] `execve()` starts the configured CGI interpreter or executable.
- [ ] Pipes or another authorized mechanism connect server and CGI input/output.
- [ ] The request body is passed to CGI when required.
- [ ] CGI output is read by the server.
- [ ] The CGI response is forwarded to the client.
- [ ] CGI receives the full request information and arguments.
- [ ] Required CGI environment variables are set.
- [ ] The CGI process runs in the correct directory.
- [ ] Relative file access from CGI works as expected.
- [ ] Chunked request bodies are unchunked before being passed to CGI.
- [ ] EOF marks the end of the CGI request body.
- [ ] If CGI output has no `Content-Length`, EOF marks the end of CGI output.
- [ ] CGI timeouts or process failures are handled.
- [ ] Dead CGI processes are cleaned up with `waitpid()`.
- [ ] CGI must not block the whole server.
- [ ] CGI errors return accurate HTTP errors.

Useful CGI environment variables to consider:

- [ ] `REQUEST_METHOD`
- [ ] `SCRIPT_NAME`
- [ ] `PATH_INFO`
- [ ] `QUERY_STRING`
- [ ] `CONTENT_TYPE`
- [ ] `CONTENT_LENGTH`
- [ ] `SERVER_PROTOCOL`
- [ ] `SERVER_NAME`
- [ ] `SERVER_PORT`
- [ ] `REMOTE_ADDR`
- [ ] HTTP headers converted to `HTTP_*` variables where needed

### 18. Error Handling

- [ ] Default error pages exist.
- [ ] Configured custom error pages override defaults.
- [ ] Bad requests return an accurate error status.
- [ ] Unknown routes return `404 Not Found`.
- [ ] Forbidden resources return `403 Forbidden`.
- [ ] Method violations return `405 Method Not Allowed`.
- [ ] Oversized bodies return an accurate status such as `413 Payload Too Large`.
- [ ] Internal failures return `500 Internal Server Error`.
- [ ] CGI failures return an accurate 5xx status.
- [ ] The server remains alive after errors.

### 19. Browser Compatibility

- [ ] The server works with at least one standard web browser.
- [ ] Static pages render correctly in the browser.
- [ ] Assets referenced by HTML load correctly.
- [ ] Forms can submit data to the server.
- [ ] Uploads can be tested from the browser.
- [ ] CGI output can be opened from the browser.
- [ ] Browser tests do not leave the server hanging.

### 20. NGINX And RFC Comparison

- [ ] The team has read the relevant HTTP RFC material before implementation.
- [ ] HTTP/1.0 may be used as a reference point, but it is not strictly enforced.
- [ ] NGINX can be used to compare headers and response behavior.
- [ ] Differences between HTTP versions are considered when comparing with NGINX.
- [ ] Ambiguous behavior is documented or decided consistently.

### 21. Stress Testing And Resilience

- [ ] The server is stress-tested.
- [ ] The server remains available during stress tests.
- [ ] Many simultaneous clients do not crash the server.
- [ ] Repeated connections do not leak file descriptors.
- [ ] Client disconnections are handled properly.
- [ ] Slow or incomplete clients do not hang forever.
- [ ] Large but valid requests are handled according to configuration.
- [ ] Invalid requests do not crash the server.
- [ ] CGI stress cases do not leave zombie processes.
- [ ] The subject recommends testing with more than one program.
- [ ] Tests may be written in Python, Go, C, C++, or another suitable language.
- [ ] The provided tester may be used, but it is not mandatory if browser and team
      tests cover the behavior.

### 22. Demonstration Files For Evaluation

The subject requires files that demonstrate every feature.

- [ ] Provide configuration files for evaluation.
- [ ] Provide default/static website files.
- [ ] Provide files that demonstrate custom error pages.
- [ ] Provide files that demonstrate directory listing.
- [ ] Provide files that demonstrate default index behavior.
- [ ] Provide files or scripts that demonstrate uploads.
- [ ] Provide files or scripts that demonstrate `GET`.
- [ ] Provide files or scripts that demonstrate `POST`.
- [ ] Provide files or scripts that demonstrate `DELETE`.
- [ ] Provide files or scripts that demonstrate CGI.
- [ ] Provide files or scripts that demonstrate redirects.
- [ ] Provide files or scripts that demonstrate multiple ports.
- [ ] Make sure demonstration paths match the provided configuration files.

### 23. README Requirements

The subject requires a README at the root of the Git repository.

- [ ] The README file is at the repository root.
- [ ] The README is written in English.
- [ ] The first line is italicized.
- [ ] The first line reads:
      `This project has been created as part of the 42 curriculum by <login1>[, <login2>[, <login3>[...]]].`
- [ ] The README contains a `Description` section.
- [ ] The `Description` section clearly presents the project.
- [ ] The `Description` section explains the goal and gives a short overview.
- [ ] The README contains an `Instructions` section.
- [ ] The `Instructions` section explains compilation.
- [ ] The `Instructions` section explains execution.
- [ ] The README contains a `Resources` section.
- [ ] The `Resources` section lists classic references.
- [ ] The `Resources` section describes how AI was used.
- [ ] Additional useful sections may be included.

### 24. AI Usage Responsibility

The subject includes AI usage guidance.

- [ ] AI-generated content must be understood by the team.
- [ ] The team must be able to take responsibility for AI-assisted content.
- [ ] AI output should be reviewed, questioned, and tested.
- [ ] Peers should review important AI-assisted decisions.
- [ ] No team member should submit code they cannot explain.
- [ ] Any AI usage should be described in the README resources section.

### 25. Bonus Features

Bonus is evaluated only if the mandatory part is fully complete.

- [ ] Support cookies.
- [ ] Support session management.
- [ ] Provide simple examples for cookies and sessions.
- [ ] Handle multiple CGI types.
- [ ] Do not rely on bonus features to compensate for missing mandatory features.

### 26. Peer Evaluation Preparation

- [ ] Be ready to explain the event loop.
- [ ] Be ready to explain why the server is non-blocking.
- [ ] Be ready to explain how `poll()` or equivalent is used.
- [ ] Be ready to explain request parsing.
- [ ] Be ready to explain response generation.
- [ ] Be ready to explain route matching.
- [ ] Be ready to explain config parsing.
- [ ] Be ready to explain CGI input, output, environment variables, and process
      cleanup.
- [ ] Be ready to explain upload handling.
- [ ] Be ready to explain DELETE behavior.
- [ ] Be ready to explain error handling.
- [ ] Be ready to make a small requested modification during evaluation.

## Suggested Team Task Breakdown

This project is easier if the work is split into clear modules. The exact
structure can be different, but every responsibility below must be covered.

### Configuration

- [ ] Define the configuration grammar.
- [ ] Parse server blocks or equivalent structures.
- [ ] Parse listen/interface:port values.
- [ ] Parse error pages.
- [ ] Parse client body size limit.
- [ ] Parse route/location blocks.
- [ ] Parse allowed methods.
- [ ] Parse route roots.
- [ ] Parse index files.
- [ ] Parse autoindex setting.
- [ ] Parse redirects.
- [ ] Parse upload settings.
- [ ] Parse CGI extension and interpreter settings.
- [ ] Validate the final configuration.
- [ ] Produce helpful error messages for invalid configuration.

### Networking

- [ ] Create listening sockets.
- [ ] Apply `setsockopt()` where needed, for example address reuse.
- [ ] Bind sockets to configured addresses and ports.
- [ ] Call `listen()` on each listening socket.
- [ ] Put sockets in non-blocking mode.
- [ ] Add listening sockets to the polling structure.
- [ ] Accept new clients only when the listening socket is ready.
- [ ] Track each client state.
- [ ] Close clients cleanly.

### Event Loop

- [ ] Maintain one central poll/select/kqueue/epoll loop.
- [ ] Register all relevant sockets.
- [ ] Register CGI pipes if they can block.
- [ ] Switch clients between read and write interest.
- [ ] Avoid blocking calls on sockets and pipes.
- [ ] Remove closed descriptors from the polling set.
- [ ] Handle timeouts.
- [ ] Keep the server alive after recoverable errors.

### HTTP Parser

- [ ] Accumulate incoming bytes per client.
- [ ] Detect complete headers.
- [ ] Parse request line.
- [ ] Parse headers case-insensitively where appropriate.
- [ ] Detect body length.
- [ ] Read complete body before handling routes that need it.
- [ ] Support chunked request handling when needed for CGI.
- [ ] Reject malformed requests.
- [ ] Protect against excessively large headers or bodies.

### Routing

- [ ] Match request host/port to the correct server configuration.
- [ ] Match request path to the correct route/location.
- [ ] Apply longest-prefix route behavior if that is the chosen design.
- [ ] Resolve filesystem paths safely.
- [ ] Apply method restrictions.
- [ ] Apply redirection before file serving.
- [ ] Decide between static file, directory, upload, delete, and CGI behavior.

### Static Files And Directories

- [ ] Open and read regular files.
- [ ] Determine file existence and permissions with `stat()`/`access()`.
- [ ] Return correct status for missing or forbidden resources.
- [ ] Serve directory index files.
- [ ] Generate autoindex output if enabled.
- [ ] Set content type where possible.

### Uploads And Body Handling

- [ ] Enforce configured body size limit.
- [ ] Store uploaded data in the configured directory.
- [ ] Prevent unsafe paths or overwrites unless explicitly intended.
- [ ] Return accurate success or error status.
- [ ] Test uploads with `curl` and a browser form.

### CGI

- [ ] Detect CGI by extension.
- [ ] Build the CGI environment.
- [ ] Create pipes for stdin/stdout.
- [ ] Fork the CGI process.
- [ ] Redirect descriptors with `dup2()`.
- [ ] Execute the CGI program with `execve()`.
- [ ] Feed request body to CGI.
- [ ] Read CGI output without blocking the server.
- [ ] Parse or forward CGI headers correctly.
- [ ] Handle missing CGI output length using EOF.
- [ ] Kill or timeout stuck CGI processes if needed.
- [ ] Reap child processes with `waitpid()`.

### Testing

- [ ] Test compilation with required flags.
- [ ] Test startup with default config.
- [ ] Test startup with explicit config.
- [ ] Test invalid config.
- [ ] Test `GET` static file.
- [ ] Test `GET` directory with index.
- [ ] Test `GET` directory with autoindex enabled.
- [ ] Test `GET` directory with autoindex disabled.
- [ ] Test `POST` upload.
- [ ] Test `DELETE` existing resource.
- [ ] Test `DELETE` missing resource.
- [ ] Test disallowed methods.
- [ ] Test oversized body.
- [ ] Test custom error pages.
- [ ] Test default error pages.
- [ ] Test redirect.
- [ ] Test CGI `GET`.
- [ ] Test CGI `POST`.
- [ ] Test CGI with query string.
- [ ] Test multiple ports.
- [ ] Test browser compatibility.
- [ ] Test many simultaneous clients.
- [ ] Test client disconnects.
- [ ] Test slow or incomplete requests.
- [ ] Test that the server remains running after each error case.

## Resources

Classic references for this project:

- RFC 2616: HTTP/1.1
- RFC 7230: HTTP/1.1 Message Syntax and Routing
- RFC 2396: URI Generic Syntax
- RFC 3875: CGI
- HTTP status code references
- Content-Type and MIME type references
- NGINX documentation about server and location selection
- Documentation for `socket()`, `bind()`, `listen()`, `accept()`, `poll()`,
  `select()`, `kqueue()`, `epoll()`, `fork()`, `execve()`, `pipe()`, `dup2()`,
  `fcntl()`, `read()`, `write()`, `send()`, and `recv()`

AI usage for this README:

- AI was used to expand the project README.
- AI was used to summarize the subject requirements into a detailed checklist.
- AI was used to explain HTTP, CGI, web server responsibilities, and testing
  expectations in clearer language.
- The team must review this README against the official subject and keep only
  content they understand and can explain during evaluation.




-----------------------------------------------------------------------------------------------------------------------------------------------

Core / Pflichtteil für deinen Partner
Server-Loop final machen
Aktuell wird nur _servers[0] genutzt und accept() blockiert nacheinander: [server_manager.cpp (line 36)](/Users/puzzlesanalytik/42Heilbronn/webserve/src/server/server_manager.cpp:36).
Noch offen:
mehrere server {} Blöcke unterstützen
mehrere Ports/Listener verwalten
alle Listener und Clients über einen zentralen poll()/select()/kqueue() Loop
Sockets non-blocking setzen
Clients parallel behandeln, nicht request-by-request blockierend

HTTP Request Handling härten
malformed requests korrekt mit 400
HTTP-Version prüfen
Header sauber validieren
Timeouts für Clients
client_max_body_size aus Config wirklich anwenden
413 Payload Too Large bei zu großem Body

Routen/Locations richtig anwenden
Parser kann einiges lesen, aber Response nutzt aktuell fast nur Server-Root.
Noch offen:
best matching location auswählen
pro Location root, index, methods, autoindex, return, upload anwenden
Method-Checks für normale Routen, nicht nur CGI
Redirects aus return wirklich ausführen

Static File Serving vervollständigen
Aktuell nur GET und einfache Datei/Index-Logik: [server_manager.cpp (line 405)](/Users/puzzlesanalytik/42Heilbronn/webserve/src/server/server_manager.cpp:405).
Noch offen:
403 bei forbidden files/directories
directory handling:index vorhanden -> ausliefern
kein index + autoindex on -> Listing generieren
kein index + autoindex off -> 403

Pfadnormalisierung gegen ../ traversal
Custom error pages aus Config nutzen

POST Upload implementieren
In Config/Location gibt es Upload-Felder: [location.cpp (line 127)](/Users/puzzlesanalytik/42Heilbronn/webserve/src/server/location.cpp:127).
Aber statische Requests erlauben aktuell praktisch nur GET.
Noch offen:
POST auf Upload-Location speichern
upload_path/upload_store aus Config benutzen
Statuscodes: 201, 400, 403, 413, 500
Browser-Form/Testdatei vorbereiten

DELETE implementieren
Config erlaubt DELETE: [default.conf (line 8)](/Users/puzzlesanalytik/42Heilbronn/webserve/configs/default.conf:8).
Aber buildStaticResponse() gibt für alles außer GET 405.
Noch offen:
DELETE vorhandene Datei
fehlende Datei -> 404
forbidden -> 403
Erfolg -> 204 oder 200

CGI Pflichtteil finalisieren
CGI ist schon vorhanden und wirkt am weitesten, aber noch prüfen:
CGI nur in erlaubter Location ausführen
Extension-Mapping korrekt .py, .sh, etc.
Query string, PATH_INFO, REQUEST_URI korrekt
POST Body an CGI
chunked Body wird entchunked
CGI Timeout/Zombie cleanup
CGI darf nicht den ganzen Server blockieren
Einstieg: [server_manager.cpp (line 430)](/Users/puzzlesanalytik/42Heilbronn/webserve/src/server/server_manager.cpp:430)

Config final validieren
mehrere Server mit gleichem Port aber unterschiedlichem server_name
Host Header zur Serverauswahl nutzen
ungültige Configs sauber ablehnen
vollständige Test-Configs für Evaluation anlegen:normal static
redirect
autoindex on/off
upload
CGI
mehrere Server/Ports


Pflicht-Tests schreiben/manual checklist
curl GET /
curl POST upload
curl DELETE file
CGI GET/POST/query
großer Body -> 413
falsche Methode -> 405
missing file -> 404
forbidden directory -> 403
Browser-Test
Stress-Test/Siege/ab oder eigener Tester



--------------------------------------------------------------------------------




Priorisierte Restaufgaben

Priorität 0 – vor jeder Evaluation
CGI vollständig in den zentralen Poll-Loop integrieren.
CGI-Pipes non-blocking machen.
Blockierendes select() und waitpid(..., 0) entfernen.
errno-/fcntl-/Funktionsliste exakt Subject-konform machen.
HTTP-Header und Content-Length strikt validieren.
Chunked Request Completion und CGI-Länge reparieren.
CGI-Skriptpfad und PATH_INFO sauber trennen.


Priorität 1 – Pflichtteilfunktionalität
Autoindex reparieren.
host beim Socket-Binding berücksichtigen.
Verhalten bei komplett fehlgeschlagenen Listenern korrigieren.
Half-Close/POLLHUP korrekt behandeln.
fehlendes CGI-Skript mit 404 beantworten.
Upload mit Binärdateien, Browserformular und Chunked testen.
längeren Stress-, Slow-Client- und Leak-Test ergänzen.


Priorität 2 – Evaluation und Dokumentation
Readme.md zu README.md umbenennen.
echte Logins eintragen.
deutschen/veralteten Abschnitt entfernen.
Startseite nicht mehr als „temporary blocking test server“ bezeichnen.
Bonus-Test für falsche Extension korrigieren.
portable Demo- und Evaluation-Anleitung erstellen.
Abschließende Antwort
Ist der Pflichtteil fertig? Nein.
Funktionieren viele Kernfeatures? Ja.
Funktioniert Upload und DELETE? Ja, in den getesteten Grundfällen.
Ist der Bonus funktional vorhanden? Ja, weitgehend.
Wird der Bonus aktuell bewertet? Nein, weil der Pflichtteil noch kritische Probleme hat.
Ist das Projekt abgabefertig? Nein.
Das Repository wurde durch den Audit nicht dauerhaft verändert. Alle temporären Testdateien, Uploads, Sessions und Buildänderungen wurden entfernt; der Git-Status ist sauber.