#! /usr/bin/python3

import os
from http import cookies
import cgi

# Create instance of FieldStorage 
form = cgi.FieldStorage() 

# Get data from fields
key = form.getvalue('key')
value  = form.getvalue('value')
if key is None or value is None:
    print("Status: 400 Bad Request")
    print("Content-Type: text/plain\r\n")
    print("Missing cookie key or value")
else:
    cookie = cookies.SimpleCookie()
    cookie[key] = value
    print("Status: 204 No Content")
    print(cookie.output())
    print("\r\n")
