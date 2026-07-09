#!/usr/bin/python3

from http import cookies
import os
import time
try:
    from urllib.parse import parse_qs
except ImportError:
    from urlparse import parse_qs

query = parse_qs(os.environ.get("QUERY_STRING", ""))
value = query.get("value", ["webserv-bonus"])[0]

cookie = cookies.SimpleCookie()
cookie["bonus_cookie"] = value
cookie["bonus_cookie"]["path"] = "/"
cookie["bonus_cookie"]["max-age"] = 600

print("Status: 200 OK")
print(cookie.output())
print("Content-Type: text/html\r\n")
print("<!doctype html>")
print("<html lang=\"en\">")
print("<head><meta charset=\"utf-8\"><title>Cookie Set</title></head>")
print("<body>")
print("<h1>Cookie set</h1>")
print("<p><code>bonus_cookie</code> was set to <code>%s</code>.</p>" % value)
print("<p>Timestamp: %s</p>" % int(time.time()))
print("<p><a href=\"/cgi-bin/bonus_read_cookie.py\">Read cookie</a></p>")
print("<p><a href=\"/bonus.html\">Back to bonus demo</a></p>")
print("</body>")
print("</html>")
