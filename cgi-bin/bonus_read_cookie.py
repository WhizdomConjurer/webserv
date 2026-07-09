#!/usr/bin/python3

from http import cookies
import html
import os

request_cookie = cookies.SimpleCookie()
if os.environ.get("HTTP_COOKIE"):
    request_cookie.load(os.environ["HTTP_COOKIE"])

print("Status: 200 OK")
print("Content-Type: text/html\r\n")
print("<!doctype html>")
print("<html lang=\"en\">")
print("<head><meta charset=\"utf-8\"><title>Cookie Read</title></head>")
print("<body>")
print("<h1>Cookie read</h1>")
if "bonus_cookie" in request_cookie:
    value = request_cookie["bonus_cookie"].value
    print("<p><code>bonus_cookie</code> is <code>%s</code>.</p>" % html.escape(value))
else:
    print("<p>No <code>bonus_cookie</code> was sent by the browser.</p>")
print("<p><a href=\"/cgi-bin/bonus_set_cookie.py?value=webserv-bonus\">Set cookie</a></p>")
print("<p><a href=\"/bonus.html\">Back to bonus demo</a></p>")
print("</body>")
print("</html>")
