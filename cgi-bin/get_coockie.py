#! /usr/bin/python3

import os
from http import cookies
import cgi

# Create instance of FieldStorage 
form = cgi.FieldStorage() 

# Get data from fields
key = form.getvalue('key')
cookie = cookies.SimpleCookie()
if 'HTTP_COOKIE' in os.environ:
    cookie.load(os.environ["HTTP_COOKIE"])
if key is None:
    print("Status: 400 Bad Request")
    print("Content-Type: text/plain\r\n")
    print("Missing cookie key")
elif key in cookie:
    print("Status: 200 OK")
    print("Content-Type: text/plain\r\n")
    print("The Value of Cookie", key, "is", cookie[key].value)
else:
    print("Status: 404 Not Found")
    print("Content-Type: text/plain\r\n")
    print("Cookie was not found!")
