#!/usr/bin/python3

from http import cookies
import hashlib
import html
import os
import time

SESSION_DIR = "bonus_sessions"

def new_sid():
    seed = "%s:%s:%s" % (time.time(), os.getpid(), os.environ.get("REMOTE_ADDR", ""))
    return hashlib.sha1(seed.encode("utf-8")).hexdigest()

def read_count(path):
    try:
        with open(path, "r") as session_file:
            return int(session_file.read().strip())
    except Exception:
        return 0

def write_count(path, count):
    if not os.path.isdir(SESSION_DIR):
        os.mkdir(SESSION_DIR)
    with open(path, "w") as session_file:
        session_file.write(str(count))

request_cookie = cookies.SimpleCookie()
if os.environ.get("HTTP_COOKIE"):
    request_cookie.load(os.environ["HTTP_COOKIE"])

sid = request_cookie["bonus_sid"].value if "bonus_sid" in request_cookie else new_sid()
session_path = os.path.join(SESSION_DIR, "session_" + sid)
count = read_count(session_path) + 1
write_count(session_path, count)

response_cookie = cookies.SimpleCookie()
response_cookie["bonus_sid"] = sid
response_cookie["bonus_sid"]["path"] = "/"
response_cookie["bonus_sid"]["max-age"] = 600

print("Status: 200 OK")
print(response_cookie.output())
print("Content-Type: text/html\r\n")
print("<!doctype html>")
print("<html lang=\"en\">")
print("<head><meta charset=\"utf-8\"><title>Session Demo</title></head>")
print("<body>")
print("<h1>Session counter</h1>")
print("<p>Session id: <code>%s</code></p>" % html.escape(sid))
print("<p>This session has opened this CGI page <strong>%d</strong> time(s).</p>" % count)
print("<p><a href=\"/cgi-bin/bonus_session.py\">Reload session demo</a></p>")
print("<p><a href=\"/bonus.html\">Back to bonus demo</a></p>")
print("</body>")
print("</html>")
