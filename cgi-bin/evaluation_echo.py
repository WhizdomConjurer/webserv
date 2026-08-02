#!/usr/bin/python3

import json
import os
import sys

body = sys.stdin.read()
payload = {
    "body": body,
    "content_length": os.environ.get("CONTENT_LENGTH", ""),
    "content_type": os.environ.get("CONTENT_TYPE", ""),
    "cwd": os.getcwd(),
    "path_info": os.environ.get("PATH_INFO", ""),
    "query_string": os.environ.get("QUERY_STRING", ""),
    "request_method": os.environ.get("REQUEST_METHOD", ""),
    "request_uri": os.environ.get("REQUEST_URI", ""),
    "script_filename": os.environ.get("SCRIPT_FILENAME", ""),
    "script_name": os.environ.get("SCRIPT_NAME", ""),
}

print("Status: 200 OK")
print("Content-Type: application/json\r\n")
print(json.dumps(payload, sort_keys=True))
