#include "http_request.hpp"
#include <cctype>

/* Creates a minimal request object that can be passed into the CGI handler. */
HttpRequest::HttpRequest()
	: _method(GET),
	  _method_str("GET"),
	  _path("/"),
	  _query(""),
	  _body("")
{
}

/* Destroys the dummy request; STL members own all data. */
HttpRequest::~HttpRequest() {}

/* Stores the enum method and keeps the method string in sync. */
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

/* Stores a method string and derives the enum used by existing CGI code. */
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

/* Stores the request path without parsing or normalization. */
void HttpRequest::setPath(const std::string &path)
{
	_path = path;
}

/* Stores the raw query string without the leading question mark. */
void HttpRequest::setQuery(const std::string &query)
{
	_query = query;
}

/* Stores the request body used for POST CGI tests. */
void HttpRequest::setBody(const std::string &body)
{
	_body = body;
}

/* Stores a header using a lower-case key for case-insensitive dummy lookup. */
void HttpRequest::setHeader(const std::string &key, const std::string &value)
{
	std::string lower = key;
	for (std::string::iterator it = lower.begin(); it != lower.end(); ++it)
		*it = static_cast<char>(std::tolower(static_cast<unsigned char>(*it)));
	_headers[lower] = value;
}

/* Returns the enum HTTP method. */
HttpMethod HttpRequest::getMethod() const
{
	return (_method);
}

/* Returns the HTTP method as text. */
const std::string &HttpRequest::getMethodStr() const
{
	return (_method_str);
}

/* Returns the request path. */
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

/* Returns a header value or an empty string when the header is absent. */
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

/* Returns all stored headers for CGI HTTP_* environment generation. */
const std::map<std::string, std::string> &HttpRequest::getHeaders() const
{
	return (_headers);
}
