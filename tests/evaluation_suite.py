#!/usr/bin/env python3
"""Subject-oriented integration tests for the local 42 webserv project."""

import argparse
import concurrent.futures
import json
import os
import re
import socket
import subprocess
import sys
import threading
import time


ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BINARY = os.path.join(ROOT, "webserv")
ALL_SECTIONS = ("source", "config", "core", "cgi", "bonus")


class ResultBook:
    def __init__(self):
        self.results = []
        self.section = "general"

    def begin(self, section):
        self.section = section
        print("\n=== %s ===" % section.upper())

    def check(self, name, condition, actual=None, expected=None, note=""):
        passed = bool(condition)
        self.results.append((self.section, name, passed))
        label = "PASS" if passed else "FAIL"
        line = "%s: %s" % (label, name)
        if actual is not None or expected is not None:
            line += " | actual=%r expected=%r" % (actual, expected)
        if note:
            line += " | %s" % note
        print(line)
        return passed

    def error(self, name, exc):
        return self.check(name, False, actual=str(exc), expected="no exception")

    def summary(self):
        print("\n=== SUMMARY ===")
        total_passed = 0
        total = 0
        for section in ALL_SECTIONS:
            selected = [item for item in self.results if item[0] == section]
            if not selected:
                continue
            passed = sum(1 for item in selected if item[2])
            total_passed += passed
            total += len(selected)
            print("%-8s %3d/%-3d passed" % (section, passed, len(selected)))
        print("TOTAL    %3d/%-3d passed, %d failed" % (total_passed, total, total - total_passed))
        return total - total_passed


BOOK = ResultBook()


class HttpResponse:
    def __init__(self, raw):
        self.raw = raw
        self.status = None
        self.headers = {}
        self.body = b""
        head, separator, body = raw.partition(b"\r\n\r\n")
        self.body = body if separator else b""
        lines = head.split(b"\r\n") if head else []
        if lines:
            parts = lines[0].split()
            if len(parts) >= 2:
                try:
                    self.status = int(parts[1])
                except ValueError:
                    pass
        for line in lines[1:]:
            if b":" not in line:
                continue
            key, value = line.split(b":", 1)
            self.headers[key.decode("latin1").lower()] = value.strip().decode("latin1")

    def json_body(self):
        try:
            return json.loads(self.body.decode("utf-8"))
        except Exception:
            return None


def raw_request(host, port, first, second=None, delay=0.0, timeout=12.0, half_close=False):
    sock = socket.create_connection((host, port), timeout=timeout)
    sock.settimeout(timeout)
    sock.sendall(first)
    if second is not None:
        time.sleep(delay)
        try:
            sock.sendall(second)
        except OSError:
            pass
    if half_close:
        try:
            sock.shutdown(socket.SHUT_WR)
        except OSError:
            pass
    chunks = []
    while True:
        try:
            chunk = sock.recv(65536)
        except (socket.timeout, ConnectionResetError):
            break
        if not chunk:
            break
        chunks.append(chunk)
    sock.close()
    return HttpResponse(b"".join(chunks))


def http_request(port, method, path, headers=None, body=b"", host="127.0.0.1", timeout=12.0):
    values = dict(headers or {})
    values.setdefault("Host", "localhost:%d" % port)
    values.setdefault("Connection", "close")
    if body and "Content-Length" not in values:
        values["Content-Length"] = str(len(body))
    header_text = "\r\n".join("%s: %s" % item for item in values.items())
    payload = ("%s %s HTTP/1.1\r\n%s\r\n\r\n" % (method, path, header_text)).encode("ascii") + body
    return raw_request(host, port, payload, timeout=timeout)


def port_is_free(host, port):
    probe = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    probe.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        probe.bind((host, port))
        return True
    except OSError:
        return False
    finally:
        probe.close()


def non_loopback_ipv4_addresses():
    addresses = set()
    try:
        records = socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET, socket.SOCK_STREAM)
        for record in records:
            address = record[4][0]
            if not address.startswith("127.") and address != "0.0.0.0":
                addresses.add(address)
    except socket.gaierror:
        pass
    return sorted(addresses)


class ManagedServer:
    def __init__(self, config, endpoints):
        self.config = config
        self.endpoints = endpoints
        self.process = None
        self.log_path = "/tmp/webserv-evaluation-%d.log" % os.getpid()
        self.log_file = None

    def __enter__(self):
        if not os.path.exists(BINARY):
            raise RuntimeError("webserv binary not found; run make re first")
        for host, port in self.endpoints:
            if not port_is_free(host, port):
                raise RuntimeError("test port is already in use: %s:%d" % (host, port))
        self.log_file = open(self.log_path, "wb")
        self.process = subprocess.Popen(
            [BINARY, self.config], cwd=ROOT, stdout=self.log_file, stderr=subprocess.STDOUT
        )
        deadline = time.time() + 5.0
        for host, port in self.endpoints:
            while time.time() < deadline:
                if self.process.poll() is not None:
                    raise RuntimeError("server exited while starting; see %s" % self.log_path)
                try:
                    probe = socket.create_connection((host, port), timeout=0.2)
                    probe.close()
                    break
                except OSError:
                    time.sleep(0.05)
            else:
                raise RuntimeError("server did not listen on %s:%d; see %s" % (host, port, self.log_path))
        return self

    def __exit__(self, exc_type, exc, tb):
        if self.process is not None and self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
        if self.log_file is not None:
            self.log_file.close()
        if exc_type is None:
            try:
                os.unlink(self.log_path)
            except OSError:
                pass


def run_source_checks():
    BOOK.begin("source")
    manager_path = os.path.join(ROOT, "src/server/server_manager.cpp")
    cgi_path = os.path.join(ROOT, "src/cgi/cgi_handler.cpp")
    manager = open(manager_path, encoding="utf-8").read()
    cgi = open(cgi_path, encoding="utf-8").read()

    exact_readme = os.path.exists(os.path.join(ROOT, "README.md"))
    BOOK.check("root file is named README.md exactly", exact_readme, exact_readme, True)
    readme_path = os.path.join(ROOT, "README.md" if exact_readme else "Readme.md")
    readme = open(readme_path, encoding="utf-8").read() if os.path.exists(readme_path) else ""
    BOOK.check("README contains real login names", "<login" not in readme, "<login" in readme, False)
    BOOK.check("README is fully English", "Core / Pflichtteil" not in readme, "Core / Pflichtteil" in readme, False)
    BOOK.check("README no longer describes a temporary blocking server", "temporary blocking" not in readme.lower(), "temporary blocking" in readme.lower(), False)

    BOOK.check("only one poll-equivalent loop", manager.count("::poll(") == 1 and "::select(" not in manager,
               "poll=%d select=%d" % (manager.count("::poll("), manager.count("::select(")), "poll=1 select=0")
    BOOK.check("macOS fcntl flags are subject-safe", "F_GETFL" not in manager, "F_GETFL" in manager, False)
    BOOK.check("listener uses the configured interface", "INADDR_ANY" not in manager,
               "INADDR_ANY" in manager, False)
    prohibited = [token for token in ("::getcwd", "::strdup", "::remove") if token in manager + cgi]
    BOOK.check("no unlisted POSIX helpers in server/CGI", not prohibited, prohibited, [])
    errno_after_io = re.search(
        r"::(?:recv|send|read|write)\s*\([^;]*;(?:(?!::(?:recv|send|read|write)\s*\().){0,400}\berrno\b",
        manager, re.DOTALL
    ) is not None
    BOOK.check("no errno-based control after socket/pipe I/O", not errno_after_io, errno_after_io, False)
    cgi_write_start = manager.find("void ServerManager::handleCgiInputWritable")
    cgi_write_end = manager.find("void ServerManager::handleCgiOutputReadable", cgi_write_start)
    cgi_write = manager[cgi_write_start:cgi_write_end]
    BOOK.check("CGI pipe write checks zero and negative results", "if (n <= 0)" in cgi_write,
               "if (n <= 0)" in cgi_write, True)


def invalid_config_rejected(path):
    process = subprocess.Popen([BINARY, path], cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        process.communicate(timeout=1)
        return process.returncode != 0, "exit=%s" % process.returncode
    except subprocess.TimeoutExpired:
        process.terminate()
        try:
            process.wait(timeout=1)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        return False, "server kept running"


def run_config_checks():
    BOOK.begin("config")
    cases = [
        ("missing configuration file", "tests/configs/invalid/does-not-exist.conf"),
        ("directory instead of configuration file", "tests/configs"),
        ("missing listen directive", "tests/configs/invalid/missing_listen.conf"),
        ("unknown directive", "tests/configs/invalid/unknown_directive.conf"),
        ("duplicate location", "tests/configs/invalid/duplicate_location.conf"),
        ("duplicate listen identity", "tests/configs/invalid/duplicate_server.conf"),
        ("invalid error-page status", "tests/configs/invalid/invalid_error_status.conf"),
        ("unclosed scope", "tests/configs/invalid/unclosed_scope.conf"),
    ]
    for name, path in cases:
        rejected, detail = invalid_config_rejected(path)
        BOOK.check(name + " is rejected", rejected, detail, "non-zero exit")


def run_default_core_checks(quick):
    with ManagedServer("configs/default.conf", [("127.0.0.1", 8080)]):
        root = http_request(8080, "GET", "/")
        BOOK.check("static GET /", root.status == 200, root.status, 200)
        declared = int(root.headers.get("content-length", "-1"))
        BOOK.check("static Content-Length is accurate", declared == len(root.body), declared, len(root.body))
        BOOK.check("static Content-Type is present", bool(root.headers.get("content-type")), root.headers.get("content-type"), "non-empty")

        missing = http_request(8080, "GET", "/evaluation-file-does-not-exist")
        BOOK.check("missing resource returns 404", missing.status == 404, missing.status, 404)
        BOOK.check("configured 404 page is served", b"Bad news" in missing.body, b"Bad news" in missing.body, True)
        unsupported = http_request(8080, "PUT", "/")
        BOOK.check("unsupported PUT returns 405", unsupported.status == 405,
                   unsupported.status, 405)
        traversal = http_request(8080, "GET", "/%2e%2e/Makefile")
        BOOK.check("encoded path traversal returns 403", traversal.status == 403,
                   traversal.status, 403)

        no_host = raw_request("127.0.0.1", 8080, b"GET / HTTP/1.1\r\nConnection: close\r\n\r\n")
        BOOK.check("HTTP/1.1 without Host returns 400", no_host.status == 400, no_host.status, 400)
        http10 = raw_request("127.0.0.1", 8080, b"GET / HTTP/1.0\r\nConnection: close\r\n\r\n")
        BOOK.check("HTTP/1.0 request works", http10.status == 200, http10.status, 200)
        malformed = raw_request("127.0.0.1", 8080, b"GET / HTTP/1.1\r\nHost: x\r\nBroken-Header\r\n\r\n")
        BOOK.check("malformed header line returns 400", malformed.status == 400, malformed.status, 400)
        unsupported_version = raw_request(
            "127.0.0.1", 8080, b"GET / HTTP/9.9\r\nHost: localhost\r\nConnection: close\r\n\r\n"
        )
        BOOK.check("unsupported HTTP version returns 505", unsupported_version.status == 505,
                   unsupported_version.status, 505)

        default_upload = os.path.join(ROOT, "uploads/evaluation_roundtrip.bin")
        try:
            os.unlink(default_upload)
        except OSError:
            pass
        roundtrip_body = b"roundtrip\x00binary\xffbody"
        roundtrip_post = http_request(8080, "POST", "/upload/evaluation_roundtrip.bin",
                                      {"Content-Type": "application/octet-stream"}, roundtrip_body)
        roundtrip_get = http_request(8080, "GET", "/upload/evaluation_roundtrip.bin")
        roundtrip_delete = http_request(8080, "DELETE", "/upload/evaluation_roundtrip.bin")
        BOOK.check("default upload route accepts POST", roundtrip_post.status == 201,
                   roundtrip_post.status, 201)
        BOOK.check("uploaded file can be retrieved byte-exactly",
                   roundtrip_get.status == 200 and roundtrip_get.body == roundtrip_body,
                   (roundtrip_get.status, roundtrip_get.body), (200, roundtrip_body))
        BOOK.check("default upload route accepts DELETE", roundtrip_delete.status == 204,
                   roundtrip_delete.status, 204)
        try:
            os.unlink(default_upload)
        except OSError:
            pass

        invalid_upload = os.path.join(ROOT, "uploads/evaluation_invalid_length.txt")
        try:
            os.unlink(invalid_upload)
        except OSError:
            pass
        invalid_length = raw_request(
            "127.0.0.1", 8080,
            b"POST /upload/evaluation_invalid_length.txt HTTP/1.1\r\nHost: x\r\nContent-Length: abc\r\n\r\n",
        )
        BOOK.check("invalid Content-Length returns 400", invalid_length.status == 400, invalid_length.status, 400)
        try:
            os.unlink(invalid_upload)
        except OSError:
            pass

        def half_closed_get(_):
            return raw_request(
                "127.0.0.1", 8080,
                b"GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n",
                timeout=3.0, half_close=True,
            ).status
        with concurrent.futures.ThreadPoolExecutor(max_workers=10) as pool:
            half_close_codes = list(pool.map(half_closed_get, range(20)))
        BOOK.check("20 complete requests survive TCP half-close",
                   all(code == 200 for code in half_close_codes),
                   "%d/20 HTTP 200" % half_close_codes.count(200), "20/20 HTTP 200")

        dropped = socket.create_connection(("127.0.0.1", 8080), timeout=2)
        dropped.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n")
        dropped.close()
        time.sleep(0.1)
        after_drop = http_request(8080, "GET", "/")
        BOOK.check("server survives abrupt client disconnect", after_drop.status == 200, after_drop.status, 200)

        if not quick:
            def parallel_get(_):
                return http_request(8080, "GET", "/", timeout=15).status
            with concurrent.futures.ThreadPoolExecutor(max_workers=20) as pool:
                codes = list(pool.map(parallel_get, range(100)))
            BOOK.check("100 parallel static requests", all(code == 200 for code in codes),
                       "%d/100 HTTP 200" % codes.count(200), "100/100 HTTP 200")


def run_static_checks():
    with ManagedServer("tests/configs/evaluation_static.conf", [("127.0.0.1", 18082)]):
        redirect = http_request(18082, "GET", "/old")
        BOOK.check("configured redirect returns 302", redirect.status == 302, redirect.status, 302)
        BOOK.check("redirect Location is accurate", redirect.headers.get("location") == "https://example.com/evaluation",
                   redirect.headers.get("location"), "https://example.com/evaluation")
        autoindex = http_request(18082, "GET", "/gallery/")
        BOOK.check("autoindex route returns 200", autoindex.status == 200, autoindex.status, 200)
        BOOK.check("autoindex lists directory contents", b"listed.txt" in autoindex.body,
                   b"listed.txt" in autoindex.body, True)
        private = http_request(18082, "GET", "/private/")
        BOOK.check("directory without index and autoindex off returns 403", private.status == 403, private.status, 403)
        post = http_request(18082, "POST", "/", body=b"not-allowed")
        BOOK.check("location method restriction returns 405", post.status == 405, post.status, 405)


def run_upload_delete_checks():
    probe = os.path.join(ROOT, "uploads/evaluation_probe.bin")
    try:
        os.unlink(probe)
    except OSError:
        pass
    try:
        with ManagedServer("tests/configs/evaluation_upload_delete.conf", [("127.0.0.1", 18083)]):
            body = b"evaluation\x00binary\xffbody"
            uploaded = http_request(18083, "POST", "/upload/evaluation_probe.bin",
                                    {"Content-Type": "application/octet-stream"}, body)
            BOOK.check("binary upload returns 201", uploaded.status == 201, uploaded.status, 201)
            stored = open(probe, "rb").read() if os.path.exists(probe) else None
            BOOK.check("binary upload content is exact", stored == body, stored, body)
            get_upload = http_request(18083, "GET", "/upload/evaluation_probe.bin")
            BOOK.check("disallowed GET on upload route returns 405", get_upload.status == 405, get_upload.status, 405)
            deleted = http_request(18083, "DELETE", "/upload/evaluation_probe.bin")
            BOOK.check("DELETE existing file returns 204", deleted.status == 204, deleted.status, 204)
            BOOK.check("DELETE removes the file", not os.path.exists(probe), os.path.exists(probe), False)
            missing = http_request(18083, "DELETE", "/upload/evaluation_probe.bin")
            BOOK.check("DELETE missing file returns 404", missing.status == 404, missing.status, 404)
            traversal = http_request(18083, "DELETE", "/upload/%2e%2e/testfile.txt")
            BOOK.check("DELETE traversal returns 403", traversal.status == 403, traversal.status, 403)
            directory = http_request(18083, "DELETE", "/upload/")
            BOOK.check("DELETE directory returns 403", directory.status == 403, directory.status, 403)
            oversized = raw_request(
                "127.0.0.1", 18083,
                b"POST /upload/oversized HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1000001\r\n\r\n",
            )
            BOOK.check("oversized body returns 413", oversized.status == 413, oversized.status, 413)
    finally:
        try:
            os.unlink(probe)
        except OSError:
            pass


def run_listener_checks():
    with ManagedServer("tests/configs/evaluation_multi_port.conf",
                       [("127.0.0.1", 18080), ("127.0.0.1", 18081)]):
        first = http_request(18080, "GET", "/")
        second = http_request(18081, "GET", "/")
        BOOK.check("first configured port responds", first.status == 200, first.status, 200)
        BOOK.check("second configured port responds", second.status == 200, second.status, 200)
        BOOK.check("ports can deliver different content",
                   b"webserv evaluation fixture" in first.body and b"second evaluation port" in second.body,
                   (b"webserv evaluation fixture" in first.body, b"second evaluation port" in second.body),
                   (True, True))

    with ManagedServer("tests/configs/evaluation_virtual_hosts.conf", [("127.0.0.1", 18088)]):
        alpha = http_request(18088, "GET", "/", {"Host": "alpha.local"})
        beta = http_request(18088, "GET", "/", {"Host": "beta.local"})
        BOOK.check("Host header selects the matching server block",
                   alpha.status == 200 and beta.status == 200
                   and b"webserv evaluation fixture" in alpha.body
                   and b"second evaluation port" in beta.body,
                   (alpha.status, beta.status,
                    b"webserv evaluation fixture" in alpha.body,
                    b"second evaluation port" in beta.body),
                   (200, 200, True, True))

    with ManagedServer("tests/configs/evaluation_interface.conf", [("127.0.0.1", 18085)]):
        configured = http_request(18085, "GET", "/", host="127.0.0.1")
        BOOK.check("configured interface responds", configured.status == 200, configured.status, 200)
        alternate_addresses = non_loopback_ipv4_addresses()
        if alternate_addresses:
            try:
                unexpected = http_request(18085, "GET", "/", host=alternate_addresses[0])
                reachable = unexpected.status is not None
            except OSError:
                reachable = False
            BOOK.check("unconfigured interface does not accept connections", not reachable, reachable, False,
                       "tested %s" % alternate_addresses[0])
        else:
            BOOK.check("unconfigured interface runtime probe", True,
                       "no non-loopback IPv4 address available", "probe skipped",
                       "source audit still checks for INADDR_ANY")

    blocker = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    blocker.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    blocker.bind(("127.0.0.1", 18086))
    blocker.listen(1)
    log = open("/tmp/webserv-evaluation-bind-failure.log", "wb")
    process = subprocess.Popen([BINARY, "tests/configs/evaluation_bind_failure.conf"], cwd=ROOT,
                               stdout=log, stderr=subprocess.STDOUT)
    time.sleep(0.5)
    exited = process.poll() is not None and process.returncode != 0
    BOOK.check("server exits when no listener can be created", exited,
               "exit=%s" % process.poll() if process.poll() is not None else "still running", "non-zero exit")
    if process.poll() is None:
        process.terminate()
        process.wait(timeout=2)
    log.close()
    blocker.close()
    try:
        os.unlink("/tmp/webserv-evaluation-bind-failure.log")
    except OSError:
        pass


def run_core_checks(quick):
    BOOK.begin("core")
    run_default_core_checks(quick)
    run_static_checks()
    run_upload_delete_checks()
    run_listener_checks()


def run_cgi_checks(quick):
    BOOK.begin("cgi")
    with ManagedServer("tests/configs/evaluation_cgi.conf", [("127.0.0.1", 18084)]):
        get_response = http_request(18084, "GET", "/cgi-bin/evaluation_echo.py?source=evaluation")
        get_json = get_response.json_body()
        BOOK.check("CGI GET returns 200", get_response.status == 200, get_response.status, 200)
        BOOK.check("CGI query string is propagated", get_json and get_json.get("query_string") == "source=evaluation",
                   get_json.get("query_string") if get_json else None, "source=evaluation")
        BOOK.check("CGI runs in the script directory", get_json and get_json.get("cwd", "").endswith("/cgi-bin"),
                   get_json.get("cwd") if get_json else None, ".../cgi-bin")

        post_response = http_request(18084, "POST", "/cgi-bin/evaluation_echo.py",
                                     {"Content-Type": "text/plain"}, b"hello")
        post_json = post_response.json_body()
        BOOK.check("CGI POST returns 200", post_response.status == 200, post_response.status, 200)
        BOOK.check("CGI receives the POST body", post_json and post_json.get("body") == "hello",
                   post_json.get("body") if post_json else None, "hello")
        BOOK.check("CGI CONTENT_LENGTH is accurate", post_json and post_json.get("content_length") == "5",
                   post_json.get("content_length") if post_json else None, "5")

        chunk_payload = (
            b"POST /cgi-bin/evaluation_echo.py HTTP/1.1\r\n"
            b"Host: localhost\r\nTransfer-Encoding: chunked\r\n"
            b"Content-Type: text/plain\r\n\r\n5\r\nhello\r\n0\r\n\r\n"
        )
        chunked = raw_request("127.0.0.1", 18084, chunk_payload)
        chunk_json = chunked.json_body()
        BOOK.check("single-packet chunked CGI returns 200", chunked.status == 200, chunked.status, 200)
        BOOK.check("single-packet chunked body is unchunked", chunk_json and chunk_json.get("body") == "hello",
                   chunk_json.get("body") if chunk_json else None, "hello")
        BOOK.check("chunked CGI receives decoded CONTENT_LENGTH",
                   chunk_json and chunk_json.get("content_length") == "5",
                   chunk_json.get("content_length") if chunk_json else None, "5")

        fragmented = raw_request(
            "127.0.0.1", 18084,
            b"POST /cgi-bin/evaluation_echo.py HTTP/1.1\r\nHost: localhost\r\n"
            b"Transfer-Encoding: chunked\r\nContent-Type: text/plain\r\n\r\n",
            b"5\r\nhello\r\n0\r\n\r\n", delay=0.25,
        )
        fragmented_json = fragmented.json_body()
        BOOK.check("fragmented chunked CGI returns 200", fragmented.status == 200, fragmented.status, 200)
        BOOK.check("fragmented chunked body is preserved",
                   fragmented_json and fragmented_json.get("body") == "hello",
                   fragmented_json.get("body") if fragmented_json else None, "hello")

        path_info = http_request(18084, "GET", "/cgi-bin/evaluation_echo.py/extra/path")
        path_json = path_info.json_body()
        BOOK.check("CGI PATH_INFO request returns 200", path_info.status == 200, path_info.status, 200)
        BOOK.check("CGI PATH_INFO is accurate", path_json and path_json.get("path_info") == "/extra/path",
                   path_json.get("path_info") if path_json else None, "/extra/path")
        missing = http_request(18084, "GET", "/cgi-bin/evaluation-does-not-exist.py")
        BOOK.check("missing CGI script returns 404", missing.status == 404, missing.status, 404)
        failed = http_request(18084, "GET", "/cgi-bin/evaluation_error.py")
        BOOK.check("failing CGI returns 502", failed.status == 502, failed.status, 502)
        alive_after_error = http_request(18084, "GET", "/")
        BOOK.check("server survives failing CGI", alive_after_error.status == 200,
                   alive_after_error.status, 200)
        wrong_extension = http_request(18084, "GET", "/cgi-bin/not_cgi.txt")
        BOOK.check("non-CGI extension is served statically", wrong_extension.status == 200 and b"not configured as CGI" in wrong_extension.body,
                   (wrong_extension.status, b"not configured as CGI" in wrong_extension.body), (200, True))

        if not quick:
            slow_response = []
            thread = threading.Thread(
                target=lambda: slow_response.append(http_request(18084, "GET", "/cgi-bin/evaluation_slow.py", timeout=10))
            )
            thread.start()
            time.sleep(0.2)
            started = time.time()
            static_during_cgi = http_request(18084, "GET", "/", timeout=10)
            elapsed = time.time() - started
            thread.join()
            BOOK.check("slow CGI returns 200", slow_response and slow_response[0].status == 200,
                       slow_response[0].status if slow_response else None, 200)
            BOOK.check("slow CGI does not block static clients", static_during_cgi.status == 200 and elapsed < 0.5,
                       "status=%s time=%.3fs" % (static_during_cgi.status, elapsed), "status=200 time<0.5s")
            timeout_response = http_request(18084, "GET", "/cgi-bin/evaluation_timeout.py", timeout=12)
            BOOK.check("hanging CGI returns 504", timeout_response.status == 504, timeout_response.status, 504)
            alive = http_request(18084, "GET", "/")
            BOOK.check("server survives CGI timeout", alive.status == 200, alive.status, 200)


def new_files(directory, before):
    if not os.path.isdir(directory):
        return []
    return [os.path.join(directory, name) for name in os.listdir(directory) if name not in before]


def run_bonus_checks():
    BOOK.begin("bonus")
    session_dir = os.path.join(ROOT, "cgi-bin/bonus_sessions")
    directory_existed = os.path.isdir(session_dir)
    before_sessions = set(os.listdir(session_dir)) if directory_existed else set()
    try:
        with ManagedServer("configs/default.conf", [("127.0.0.1", 8080)]):
            page = http_request(8080, "GET", "/bonus.html")
            BOOK.check("bonus browser page is available", page.status == 200 and b"webserv bonus demo" in page.body,
                       (page.status, b"webserv bonus demo" in page.body), (200, True))
            python_cgi = http_request(8080, "GET", "/cgi-bin/bonus_python.py?source=evaluation")
            BOOK.check("Python CGI bonus works", python_cgi.status == 200 and b"Python CGI works" in python_cgi.body,
                       (python_cgi.status, b"Python CGI works" in python_cgi.body), (200, True))
            BOOK.check("Python CGI bonus receives query", b"source=evaluation" in python_cgi.body,
                       b"source=evaluation" in python_cgi.body, True)
            shell_cgi = http_request(8080, "GET", "/cgi-bin/bonus_shell.sh?source=evaluation")
            BOOK.check("shell CGI bonus works", shell_cgi.status == 200 and b"Shell CGI works" in shell_cgi.body,
                       (shell_cgi.status, b"Shell CGI works" in shell_cgi.body), (200, True))
            BOOK.check("shell CGI bonus receives query", b"source=evaluation" in shell_cgi.body,
                       b"source=evaluation" in shell_cgi.body, True)
            wrong_extension = http_request(8080, "GET", "/cgi-bin/not_cgi.txt")
            BOOK.check("wrong CGI extension is not executed", wrong_extension.status == 200 and b"not configured as CGI" in wrong_extension.body,
                       (wrong_extension.status, b"not configured as CGI" in wrong_extension.body), (200, True))

            set_cookie = http_request(8080, "GET", "/cgi-bin/bonus_set_cookie.py?value=from-evaluation")
            cookie_header = set_cookie.headers.get("set-cookie", "")
            BOOK.check("cookie CGI forwards Set-Cookie", set_cookie.status == 200 and "bonus_cookie=from-evaluation" in cookie_header,
                       (set_cookie.status, cookie_header), "200 and bonus_cookie=from-evaluation")
            read_cookie = http_request(8080, "GET", "/cgi-bin/bonus_read_cookie.py",
                                       {"Cookie": "bonus_cookie=from-evaluation"})
            BOOK.check("cookie CGI reads client cookie", read_cookie.status == 200 and b"from-evaluation" in read_cookie.body,
                       (read_cookie.status, b"from-evaluation" in read_cookie.body), (200, True))

            session_one = http_request(8080, "GET", "/cgi-bin/bonus_session.py")
            session_cookie = session_one.headers.get("set-cookie", "").split(";", 1)[0]
            BOOK.check("session CGI sets a session cookie", session_one.status == 200 and session_cookie.startswith("bonus_sid="),
                       (session_one.status, session_cookie), "200 and bonus_sid=...")
            BOOK.check("session counter starts at one", b"<strong>1</strong>" in session_one.body,
                       b"<strong>1</strong>" in session_one.body, True)
            session_two = http_request(8080, "GET", "/cgi-bin/bonus_session.py", {"Cookie": session_cookie})
            BOOK.check("session counter increments", session_two.status == 200 and b"<strong>2</strong>" in session_two.body,
                       (session_two.status, b"<strong>2</strong>" in session_two.body), (200, True))
    finally:
        for path in new_files(session_dir, before_sessions):
            try:
                os.unlink(path)
            except OSError:
                pass
        if not directory_existed and os.path.isdir(session_dir):
            try:
                os.rmdir(session_dir)
            except OSError:
                pass


def parse_args():
    parser = argparse.ArgumentParser(description="Run the webserv subject/evaluation test suite")
    parser.add_argument("--section", action="append", choices=ALL_SECTIONS,
                        help="run only this section; may be supplied multiple times")
    parser.add_argument("--quick", action="store_true",
                        help="skip slow CGI timeout and 100-client concurrency checks")
    return parser.parse_args()


def main():
    args = parse_args()
    sections = tuple(args.section) if args.section else ALL_SECTIONS
    if not os.path.exists(BINARY):
        print("ERROR: webserv binary is missing. Run: make re", file=sys.stderr)
        return 2
    runners = {
        "source": run_source_checks,
        "config": run_config_checks,
        "core": lambda: run_core_checks(args.quick),
        "cgi": lambda: run_cgi_checks(args.quick),
        "bonus": run_bonus_checks,
    }
    for section in sections:
        try:
            runners[section]()
        except Exception as exc:
            BOOK.begin(section)
            BOOK.error("section completed without infrastructure error", exc)
    failures = BOOK.summary()
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
