# webserv

`webserv` ist ein 42-Projekt, in dem ein eigener HTTP-Server in C++ gebaut wird.
Der Server soll mit einem echten Browser getestet werden koennen und grundlegende
Webserver-Funktionen selbst umsetzen: Verbindungen annehmen, HTTP-Anfragen lesen,
Antworten erzeugen, statische Dateien ausliefern, Fehlerseiten senden und CGI
Programme starten.

Das Ziel ist nicht, einen fertigen Produktionsserver wie nginx nachzubauen,
sondern die wichtigsten Mechanismen hinter Webservern zu verstehen.

## Was ist HTTP?

HTTP steht fuer **Hypertext Transfer Protocol**. Es ist das Protokoll, mit dem
Browser und Webserver im Web miteinander sprechen.

Ein typischer Ablauf sieht so aus:

1. Ein Browser verbindet sich mit einem Server.
2. Der Browser sendet eine HTTP-Anfrage, zum Beispiel `GET /index.html HTTP/1.1`.
3. Der Server liest diese Anfrage.
4. Der Server entscheidet, was zur Anfrage passt.
5. Der Server sendet eine HTTP-Antwort zurueck.

Eine HTTP-Anfrage besteht meistens aus:

- einer Request-Line, zum Beispiel `GET / HTTP/1.1`
- Headern, zum Beispiel `Host`, `Content-Length` oder `Content-Type`
- optional einem Body, zum Beispiel bei `POST`

Eine HTTP-Antwort besteht meistens aus:

- einer Status-Line, zum Beispiel `HTTP/1.1 200 OK`
- Headern, zum Beispiel `Content-Type` oder `Content-Length`
- optional einem Body, zum Beispiel HTML, JSON, ein Bild oder eine Fehlerseite

HTTP ist wichtig, weil es festlegt, wie Client und Server Daten austauschen. Ohne
dieses gemeinsame Format wuesste der Browser nicht, wie er eine Antwort verstehen
soll, und der Server wuesste nicht, was der Browser eigentlich anfragt.

## HTTP-Methoden

HTTP-Methoden beschreiben, was der Client tun moechte.

- `GET`: Eine Ressource abrufen, zum Beispiel eine HTML-Datei oder ein Bild.
- `POST`: Daten an den Server schicken, zum Beispiel ein Formular oder Upload.
- `DELETE`: Eine Ressource loeschen, wenn der Server das erlaubt.

Im Projekt muss der Server diese Methoden korrekt erkennen und je nach
Konfiguration erlauben oder ablehnen.

## HTTP-Statuscodes

Statuscodes sagen dem Client, ob eine Anfrage erfolgreich war oder warum sie
fehlgeschlagen ist.

Beispiele:

- `200 OK`: Die Anfrage war erfolgreich.
- `201 Created`: Eine Ressource wurde erstellt.
- `301 Moved Permanently`: Die Ressource wurde dauerhaft verschoben.
- `400 Bad Request`: Die Anfrage ist fehlerhaft.
- `403 Forbidden`: Der Zugriff ist verboten.
- `404 Not Found`: Die Ressource wurde nicht gefunden.
- `405 Method Not Allowed`: Die Methode ist fuer diese Route nicht erlaubt.
- `413 Payload Too Large`: Der Request-Body ist groesser als erlaubt.
- `500 Internal Server Error`: Auf dem Server ist ein Fehler passiert.

Diese Codes sind wichtig, weil Browser, Tools wie `curl` und andere Programme
dadurch maschinenlesbar verstehen, was passiert ist.

## Was macht ein Webserver?

Ein Webserver ist ein Programm, das auf Netzwerkverbindungen wartet und
HTTP-Anfragen beantwortet.

In `webserv` muss der Server unter anderem:

- einen Socket erstellen
- an eine IP-Adresse und einen Port binden
- auf eingehende Verbindungen warten
- neue Clients akzeptieren
- Daten von Clients lesen
- HTTP-Anfragen parsen
- passende Server- und Location-Konfigurationen auswaehlen
- statische Dateien ausliefern
- Directory Listing erzeugen, falls aktiviert
- Uploads verarbeiten, falls erlaubt
- Weiterleitungen senden
- Fehlerseiten ausliefern
- CGI-Skripte ausfuehren
- mehrere Clients gleichzeitig verwalten

Dabei ist wichtig, dass der Server nicht fuer jeden Client blockiert. Deshalb
werden Mechanismen wie `poll()` verwendet, um viele File Descriptors gleichzeitig
zu beobachten.

## Was ist CGI?

CGI steht fuer **Common Gateway Interface**. Es ist eine Schnittstelle zwischen
einem Webserver und einem externen Programm oder Skript.

Ohne CGI liefert ein Webserver meistens nur vorhandene Dateien aus, zum Beispiel:

- `index.html`
- `style.css`
- `image.png`

Mit CGI kann der Server dynamische Inhalte erzeugen lassen. Das bedeutet: Der
Server startet ein Programm, gibt ihm Informationen ueber die Anfrage und schickt
die Ausgabe dieses Programms als HTTP-Antwort an den Client zurueck.

Beispiel:

1. Der Browser ruft `/cgi-bin/script.py` auf.
2. Der Server erkennt, dass diese Datei als CGI ausgefuehrt werden soll.
3. Der Server setzt CGI-Umgebungsvariablen wie `REQUEST_METHOD`, `PATH_INFO` oder
   `CONTENT_LENGTH`.
4. Bei `POST` gibt der Server den Request-Body an das CGI-Programm weiter.
5. Das CGI-Programm schreibt seine Antwort auf `stdout`.
6. Der Server liest diese Ausgabe und sendet sie an den Browser.

CGI ist also dafuer da, dynamische Antworten zu erzeugen, zum Beispiel:

- Formularverarbeitung
- Login- oder Session-Logik
- kleine API-Antworten
- generierte HTML-Seiten
- Verarbeitung von Uploads

## Wichtige CGI-Variablen

CGI-Programme bekommen Informationen ueber die Anfrage ueber
Umgebungsvariablen.

Typische Variablen sind:

- `REQUEST_METHOD`: Die HTTP-Methode, zum Beispiel `GET` oder `POST`.
- `SCRIPT_NAME`: Der Pfad zum ausgefuehrten Skript.
- `PATH_INFO`: Zusaetzliche Pfadinformation nach dem Skriptnamen.
- `QUERY_STRING`: Der Teil der URL nach `?`.
- `CONTENT_TYPE`: Der Typ des Request-Bodys.
- `CONTENT_LENGTH`: Die Groesse des Request-Bodys.
- `SERVER_PROTOCOL`: Die HTTP-Version, zum Beispiel `HTTP/1.1`.
- `HTTP_COOKIE`: Cookies, die der Client geschickt hat.

Diese Variablen sind der Weg, wie das CGI-Programm versteht, welche Anfrage
gestellt wurde.

## Warum braucht `webserv` Sockets?

Ein Socket ist ein Endpunkt fuer Netzwerkkommunikation. Der Server nutzt Sockets,
um TCP-Verbindungen von Browsern oder Tools wie `curl` anzunehmen.

Wichtige Funktionen:

- `socket()`: Erstellt einen neuen Socket.
- `bind()`: Bindet den Socket an eine Adresse und einen Port.
- `listen()`: Versetzt den Socket in den Zustand, in dem er Verbindungen annimmt.
- `accept()`: Nimmt eine neue Client-Verbindung an.
- `recv()` / `read()`: Liest Daten vom Client.
- `send()` / `write()`: Schreibt Daten zurueck an den Client.
- `close()`: Schliesst den Socket oder File Descriptor.

## Warum wird `poll()` benutzt?

Ein Webserver muss oft mehrere Clients gleichzeitig bedienen. Wenn der Server bei
einem Client blockiert, koennen andere Clients nicht mehr bearbeitet werden.

`poll()` hilft dabei, viele File Descriptors gleichzeitig zu ueberwachen. Der
Server kann dadurch erkennen:

- welcher Socket eine neue Verbindung hat
- welcher Client neue Daten geschickt hat
- welcher Client bereit ist, Daten zu empfangen
- ob ein Fehler oder Disconnect passiert ist

Dadurch kann ein einzelner Prozess mehrere Clients verwalten, ohne fuer jeden
Client einen eigenen blockierenden Ablauf zu starten.

## Konfiguration

Ein wichtiger Teil von `webserv` ist die Konfigurationsdatei. Sie beschreibt, wie
der Server sich verhalten soll.

Typische Einstellungen sind:

- Port und Host, auf denen der Server lauscht
- Servernamen
- Root-Verzeichnisse
- erlaubte HTTP-Methoden
- maximale Body-Groesse
- eigene Fehlerseiten
- Weiterleitungen
- CGI-Endungen und CGI-Interpreter
- Upload-Verzeichnisse
- Autoindex fuer Directory Listing

Die Konfiguration ist wichtig, weil ein Webserver je nach Route unterschiedlich
reagieren kann. Zum Beispiel kann `/uploads` Uploads erlauben, waehrend `/static`
nur Dateien ausliefert.

## Nuetzliche Tests

Der Server kann mit einem Browser getestet werden. Fuer gezieltere Tests ist
`curl` sehr hilfreich:

```sh
curl -v http://localhost:8080/
curl -X POST http://localhost:8080/upload -d "hello"
curl -H "Host: example.com" http://localhost:8080/
curl --resolve example.com:8080:127.0.0.1 http://example.com:8080/
```

Mit `curl -v` sieht man Header, Statuscode und Verbindungsdetails sehr gut.

## Tutorials

- Building a simple web server in C++
- C++ Web Programming: CGI program
- HTTP
- Build a simple HTTP server from scratch
- Manage a socket flow of events using `poll()`
- C++ programming applied to network
- Network programming

## HTTP documentation

- RFC 2616: HTTP 1.1 protocol
- RFC 7230: HTTP/1.1 Message Syntax and Routing
- List of HTTP status codes
- Content-Type
- Content-Type Stack List
- Content-Type Full List
- HTTP messages
- Redirections

## Useful RFCs

- RFC editor: official source for RFCs on the World Wide Web
- RFC 2396: Uniform Resource Identifiers (URI): Generic Syntax
- RFC 3875: CGI

## Useful C functions

- `socket()`: Creates an endpoint for communication and returns a file descriptor.
- `bind()`: Assigns an address and port to a socket.
- `listen()`: Marks a socket as passive, so it can accept incoming connections.
- `accept()`: Accepts a new connection on a listening socket.
- `poll()`: Waits until one or more file descriptors are ready for I/O.
- `fork()`: Creates a child process, useful for CGI execution.
- `execve()`: Replaces the current process image with another program.
- `pipe()`: Creates a communication channel between processes.
- `dup2()`: Redirects file descriptors, useful for CGI stdin/stdout handling.

## CGI documentation

- RFC 3875: CGI
- CGI environment variables
- CGI examples
- CGI tutorials with cookies

## Tools

- Browser for real-world testing
- `curl` for precise HTTP requests
- Header testing websites
- nginx documentation for understanding server and location selection

## Server model: nginx documentation

- Inside nginx architecture
- Understanding nginx server and location block selection algorithms


webserv

This project is here to make you write your HTTP server. You will be able to test it with a real browser. HTTP is one of the most used protocols on the internet. Knowing its arcane will be useful, even if you won’t be working on a website.

Tutorials

    Building a simple web serveur in c++
    C++ Web Programming: CGI program
    HTTP
    Build a simple HTTP server from scratch
    Manage a socket flow of events using poll()
    C++ programming applied to network (in French)
    Network programming

HTTP documentation

    RFC 2616: HTTP 1.1 protocol
    RFC 7230: HTTP/1.1 Message Syntax and Routing
    List of HTTP status codes
    Content-Type
    Content-Type Stack List
    Content-Type Full List
    Tres bon site aussi !
    HTTP messages
    Redirections

Useful RFCs

    RFC editor: official source for RFCs on the World Wide Web.
    RFC 2396: Uniform Resource Identifiers (URI): Generic Syntax: useful definitions of URI, port and host

Useful C functions

    socket: creates an endpoint for communication and returns a file descriptor that refers to that endpoint.
    listen: listens for connections on a socket.
    poll: waits for one of a set of file descriptors to become ready to perform I/O.
    accept: accepts a connection on a socket.

CGI Doc

    RFC 3875: CGI
    Best CGI Exemple: Programmation CGI in C++.
    CGI Environmnent Variables
    Good CGI Howto (and cookies)
    Some exemples

Tools

    Super Mega Site pour tester les Headers !
    cURL "--resolve"; curl -H "Host: ..." (for testing different server names).

Server model: nginx documentation

    Inside Nginx architecture
    Understanding Nginx Server and Location Block Selection Algorithms
