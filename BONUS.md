# webserv Bonus

Bonus is evaluated only after the mandatory part is complete.

## Implemented Bonus Points

- Multiple CGI types through extension mapping:
  - `.py` -> `/usr/bin/python3`
  - `.sh` -> `/bin/bash`
- Cookie demo:
  - set a cookie through CGI
  - read the cookie through CGI
  - verify that `Set-Cookie` is forwarded by the server
- Session demo:
  - CGI creates a session id cookie
  - CGI persists a small per-session counter in `cgi-bin/bonus_sessions`
- Browser demo page:
  - `http://localhost:8080/bonus.html`

## Browser Demo URLs

- Bonus overview: `http://localhost:8080/bonus.html`
- Python CGI: `http://localhost:8080/cgi-bin/bonus_python.py?source=browser`
- Shell CGI: `http://localhost:8080/cgi-bin/bonus_shell.sh?source=browser`
- Wrong extension test: `http://localhost:8080/cgi-bin/not_cgi.txt`
- Set cookie: `http://localhost:8080/cgi-bin/bonus_set_cookie.py?value=webserv-bonus`
- Read cookie: `http://localhost:8080/cgi-bin/bonus_read_cookie.py`
- Session counter: `http://localhost:8080/cgi-bin/bonus_session.py`

## Curl Tests

Start the server:

```sh
make
./webserv configs/default.conf
```

Run individual checks:

```sh
curl -i 'http://localhost:8080/cgi-bin/bonus_python.py?source=curl'
curl -i 'http://localhost:8080/cgi-bin/bonus_shell.sh?source=curl'
curl -i 'http://localhost:8080/cgi-bin/not_cgi.txt'
curl -i -c /tmp/webserv_bonus_cookie.txt 'http://localhost:8080/cgi-bin/bonus_set_cookie.py?value=webserv-bonus'
curl -i -b /tmp/webserv_bonus_cookie.txt 'http://localhost:8080/cgi-bin/bonus_read_cookie.py'
curl -i -c /tmp/webserv_bonus_session.txt -b /tmp/webserv_bonus_session.txt 'http://localhost:8080/cgi-bin/bonus_session.py'
curl -i -c /tmp/webserv_bonus_session.txt -b /tmp/webserv_bonus_session.txt 'http://localhost:8080/cgi-bin/bonus_session.py'
```

Or run the bundled bonus smoke test:

```sh
bash tests/bonus_cgi_tests.sh
```
