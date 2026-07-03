#ifndef HTTP_REQUEST_HPP
# define HTTP_REQUEST_HPP

# include <map>
# include <string>

enum HttpMethod
{
	GET,
	POST,
	DELETE,
	UNKNOWN_METHOD
};

class HttpRequest
{
	private:
		HttpMethod							_method;
		std::string							_method_str;
		std::string							_path;
		std::string							_query;
		std::string							_body;
		std::map<std::string, std::string>	_headers;

	public:
		HttpRequest();
		~HttpRequest();

		void	setMethod(HttpMethod method);
		void	setMethodStr(const std::string &method);
		void	setPath(const std::string &path);
		void	setQuery(const std::string &query);
		void	setBody(const std::string &body);
		void	setHeader(const std::string &key, const std::string &value);

		HttpMethod							getMethod() const;
		const std::string					&getMethodStr() const;
		const std::string					&getPath() const;
		std::string							getQuery() const;
		const std::string					&getBody() const;
		std::string							getHeader(const std::string &key) const;
		const std::map<std::string, std::string>	&getHeaders() const;
};

#endif
