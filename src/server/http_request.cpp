#include "http_request.hpp"
#include <cctype>

/* Creates a minimal GET request as a default so that the object is immediately valid. */
HttpRequest::HttpRequest()
	: _method(GET),
	  _method_str("GET"),
	  _path("/"),
	  _query(""),
	  _body(""),
	  _valid(true)
{
}

/* Destroys the request; strings and the header map clean themselves up. */
HttpRequest::~HttpRequest() {}

/* Stores the HTTP method as an enum and keeps the text value synchronized. */
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

/* Stores the method as text and derives the corresponding enum from it. */
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

/* Stores the request path without any further normalization. */
void HttpRequest::setPath(const std::string &path)
{
	_path = path;
}

/* Stores the raw query string without the leading question mark. */
void HttpRequest::setQuery(const std::string &query)
{
	_query = query;
}

/* Stores the request body, e.g. for POST requests to CGI. */
void HttpRequest::setBody(const std::string &body)
{
	_body = body;
}

/* Stores the header with a lowercase name so lookups work case-insensitively. */
void HttpRequest::setHeader(const std::string &key, const std::string &value)
{
	std::string lower = key;
	for (std::string::iterator it = lower.begin(); it != lower.end(); ++it)
		*it = static_cast<char>(std::tolower(static_cast<unsigned char>(*it)));
	_headers[lower] = value;
}

void HttpRequest::setValid(bool valid)
{
	_valid = valid;
}

void HttpRequest::setVersion(const std::string &version)
{
	_version = version;
}

const std::string &HttpRequest::getVersion() const
{
	return (_version);
}

bool HttpRequest::isValid() const
{
	return (_valid);
}

/* Returns the HTTP method as an enum. */
HttpMethod HttpRequest::getMethod() const
{
	return (_method);
}

/* Returns the HTTP method as text. */
const std::string &HttpRequest::getMethodStr() const
{
	return (_method_str);
}

/* Returns the http request path. */
const std::string &HttpRequest::getPath() const
{
	return (_path);
}

/* Returns the raw query string. */
std::string HttpRequest::getQuery() const
{
	return (_query);
}

/* Returns the request body. */
const std::string &HttpRequest::getBody() const
{
	return (_body);
}

/* Finds a header case-insensitively; returns "" otherwise. */
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

/* Returns all stored headers so CGI can build HTTP_* environment variables from them. */
const std::map<std::string, std::string> &HttpRequest::getHeaders() const
{
	return (_headers);
}
