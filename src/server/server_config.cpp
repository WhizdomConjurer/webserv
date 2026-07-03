#include "server_config.hpp"
#include "../parser/config_file.hpp"
#include <cstdlib>

/* Creates a dummy server configuration with conservative defaults. */
ServerConfig::ServerConfig()
	: _server_name(""),
	  _root(""),
	  _index(""),
	  _host_string(""),
	  _host(0),
	  _port(0),
	  _client_max_body_size(1000000),
	  _autoindex(false)
{
}

/* Destroys the server config; STL members own all data. */
ServerConfig::~ServerConfig() {}

/* Removes the parser's trailing semicolon from a directive value. */
std::string ServerConfig::cleanToken(const std::string &value) const
{
	if (!value.empty() && value[value.length() - 1] == ';')
		return (value.substr(0, value.length() - 1));
	return (value);
}

/* Stores the TCP port on which this server should listen. */
void ServerConfig::setPort(const std::string &port)
{
	_port = std::atoi(cleanToken(port).c_str());
}

/* Stores the host string and marks host as configured for parser duplicate checks. */
void ServerConfig::setHost(const std::string &host)
{
	_host_string = cleanToken(host);
	_host = _host_string.empty() ? 0 : 1;
}

/* Stores the filesystem root and normalizes it to end with slash. */
void ServerConfig::setRoot(const std::string &root)
{
	_root = cleanToken(root);
	if (!_root.empty() && _root[_root.length() - 1] != '/')
		_root += "/";
}

/* Stores the default index file name. */
void ServerConfig::setIndex(const std::string &index)
{
	_index = cleanToken(index);
}

/* Stores the optional server_name value. */
void ServerConfig::setServerName(const std::string &name)
{
	_server_name = cleanToken(name);
}

/* Stores the maximum accepted request body size in bytes. */
void ServerConfig::setClientMaxBodySize(const std::string &size)
{
	_client_max_body_size = static_cast<size_t>(std::atol(cleanToken(size).c_str()));
}

/* Enables or disables autoindex at server level for future real routing logic. */
void ServerConfig::setAutoindex(const std::string &value)
{
	const std::string clean = cleanToken(value);
	_autoindex = (clean == "on" || clean == "true" || clean == "1");
}

/* Creates a Location dummy from parser tokens and inherits server root/index defaults. */
void ServerConfig::setLocation(const std::string &path, const std::vector<std::string> &tokens)
{
	Location location(path);
	if (!_root.empty())
		location.setRootLocation(_root);
	if (!_index.empty())
		location.setIndexLocation(_index);
	location.loadDirectives(tokens);
	_locations.push_back(location);
}

/* Parses error_page pairs in the simple form: error_page 404 /404.html;. */
void ServerConfig::setErrorPages(const std::vector<std::string> &tokens)
{
	if (tokens.size() < 2)
		return;
	const std::string path = cleanToken(tokens[tokens.size() - 1]);
	for (size_t i = 0; i + 1 < tokens.size(); ++i)
	{
		const short code = static_cast<short>(std::atoi(cleanToken(tokens[i]).c_str()));
		if (code > 0)
			_error_pages[code] = path;
	}
}

/* Returns the optional virtual host name. */
const std::string &ServerConfig::getServerName() const
{
	return (_server_name);
}

/* Returns 0 when unset, 1 when a host string was configured. */
int ServerConfig::getHost() const
{
	return (_host);
}

/* Returns the configured host string. */
const std::string &ServerConfig::getHostString() const
{
	return (_host_string);
}

/* Returns the filesystem root. */
const std::string &ServerConfig::getRoot() const
{
	return (_root);
}

/* Returns the default index file. */
const std::string &ServerConfig::getIndex() const
{
	return (_index);
}

/* Returns the configured listen port. */
int ServerConfig::getPort() const
{
	return (_port);
}

/* Returns the configured maximum request body size. */
size_t ServerConfig::getClientMaxBodySize() const
{
	return (_client_max_body_size);
}

/* Returns the configured error page map. */
const std::map<short, std::string> &ServerConfig::getErrorPages() const
{
	return (_error_pages);
}

/* Returns the configured locations. */
const std::vector<Location> &ServerConfig::getLocations() const
{
	return (_locations);
}

/* Returns mutable locations for tests that need to feed the CGI handler. */
std::vector<Location> &ServerConfig::getLocations()
{
	return (_locations);
}

/* Checks whether two location blocks use the same path. */
bool ServerConfig::checkLocaitons() const
{
	for (size_t i = 0; i < _locations.size(); ++i)
	{
		for (size_t j = i + 1; j < _locations.size(); ++j)
		{
			if (_locations[i].getPath() == _locations[j].getPath())
				return (true);
		}
	}
	return (false);
}

/* Validates that configured error pages are readable when they exist in the dummy tree. */
bool ServerConfig::isValidErrorPages() const
{
	for (std::map<short, std::string>::const_iterator it = _error_pages.begin();
		it != _error_pages.end(); ++it)
	{
		if (it->first < 300 || it->first > 599)
			return (false);
	}
	return (true);
}
