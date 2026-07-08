#include "http_request.hpp"
#include <cctype>

/* Erstellt eine minimale GET-Anfrage als Default, damit das Objekt sofort gültig ist. */
HttpRequest::HttpRequest()
	: _method(GET),
	  _method_str("GET"),
	  _path("/"),
	  _query(""),
	  _body("")
{
}

/* Zerstört die Anfrage; Strings und Header-Map räumen sich selbst auf. */
HttpRequest::~HttpRequest() {}

/* Speichert die HTTP-Methode als enum und hält den Textwert synchron. */
void HttpRequest::setMethod(HttpMethod method)
{
	_method = method;
	if (method == GET)
		_method_str = "GET";
	else if (method == POST)
		_method_str = "POST";
	else if (method == DELETE)
		_method_str = "DELETE";
	else
		_method_str = "UNKNOWN";
}

/* Speichert die Methode als Text und leitet daraus das passende enum ab. */
void HttpRequest::setMethodStr(const std::string &method)
{
	_method_str = method;
	if (method == "GET")
		_method = GET;
	else if (method == "POST")
		_method = POST;
	else if (method == "DELETE")
		_method = DELETE;
	else
		_method = UNKNOWN_METHOD;
}

/* Speichert den Anfragepfad ohne weitere Normalisierung. */
void HttpRequest::setPath(const std::string &path)
{
	_path = path;
}

/* Speichert den rohen Query-String ohne führendes Fragezeichen. */
void HttpRequest::setQuery(const std::string &query)
{
	_query = query;
}

/* Speichert den Request-Body, z.B. für POST-Anfragen an CGI. */
void HttpRequest::setBody(const std::string &body)
{
	_body = body;
}

/* Speichert Header mit kleingeschriebenem Namen, damit Lookups case-insensitive funktionieren. */
void HttpRequest::setHeader(const std::string &key, const std::string &value)
{
	std::string lower = key;
	for (std::string::iterator it = lower.begin(); it != lower.end(); ++it)
		*it = static_cast<char>(std::tolower(static_cast<unsigned char>(*it)));
	_headers[lower] = value;
}

/* Gibt die HTTP-Methode als enum zurück. */
HttpMethod HttpRequest::getMethod() const
{
	return (_method);
}

/* Gibt die HTTP-Methode als Text zurück. */
const std::string &HttpRequest::getMethodStr() const
{
	return (_method_str);
}

/* Gibt den Anfragepfad zurück. */
const std::string &HttpRequest::getPath() const
{
	return (_path);
}

/* Gibt den rohen Query-String zurück. */
std::string HttpRequest::getQuery() const
{
	return (_query);
}

/* Gibt den Request-Body zurück. */
const std::string &HttpRequest::getBody() const
{
	return (_body);
}

/* Sucht einen Header unabhängig von Groß-/Kleinschreibung und gibt sonst "" zurück. */
std::string HttpRequest::getHeader(const std::string &key) const
{
	std::string lower = key;
	for (std::string::iterator it = lower.begin(); it != lower.end(); ++it)
		*it = static_cast<char>(std::tolower(static_cast<unsigned char>(*it)));
	std::map<std::string, std::string>::const_iterator found = _headers.find(lower);
	if (found == _headers.end())
		return ("");
	return (found->second);
}

/* Gibt alle gespeicherten Header zurück, damit CGI daraus HTTP_*-Umgebungsvariablen bauen kann. */
const std::map<std::string, std::string> &HttpRequest::getHeaders() const
{
	return (_headers);
}
