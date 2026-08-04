#include "location.hpp"

/* Creates a location with safe defaults for root, index, and disabled extras. */
Location::Location()
	: _path("/"),
	  _root("./www/"),
	  _index("index.html"),
	  _autoindex(false),
	  _upload_enabled(false)
{
}

/* Creates a location for a specific URL path and inherits the default values. */
Location::Location(const std::string &path)
	: _path(cleanToken(path)),
	  _root("./www/"),
	  _index("index.html"),
	  _autoindex(false),
	  _upload_enabled(false)
{
}

/* Destroys the location; strings and containers clean themselves up. */
Location::~Location() {}

/* Removes a trailing semicolon from a config token. */
std::string Location::cleanToken(const std::string &value)
{
	if (!value.empty() && value[value.length() - 1] == ';')
		return (value.substr(0, value.length() - 1));
	return (value);
}

/* Stores the URL path of this location, e.g. /cgi-bin or /static. */
void Location::setPath(const std::string &path)
{
	_path = cleanToken(path);
}

/* Stores the filesystem root from which this location resolves files. */
void Location::setRootLocation(const std::string &root)
{
	_root = cleanToken(root);
	if (!_root.empty() && _root[_root.length() - 1] != '/')
		_root += "/";
}

/* Stores the index file for directory requests in this location. */
void Location::setIndexLocation(const std::string &index)
{
	_index = cleanToken(index);
}

/* Enables or disables directory listing for this location. */
void Location::setAutoindex(const std::string &value)
{
	const std::string clean = cleanToken(value);
	_autoindex = (clean == "on" || clean == "true" || clean == "1");
}

/* Stores the redirect target of a return directive. */
void Location::setReturn(const std::string &value)
{
	_return = cleanToken(value);
}

/* Stores an alias path that can replace the normal root. */
void Location::setAlias(const std::string &value)
{
	_alias = cleanToken(value);
}

/* Stores the target directory for uploaded files. */
void Location::setUploadPath(const std::string &path)
{
	_upload_path = cleanToken(path);
}

/* Enables or disables uploads in this location. */
void Location::setUploadEnabled(const std::string &value)
{
	const std::string clean = cleanToken(value);
	_upload_enabled = (clean == "on" || clean == "true" || clean == "1");
}

/* Adds an allowed HTTP method, e.g. GET or POST. */
void Location::addMethod(const std::string &method)
{
	_methods.insert(cleanToken(method));
}

/* Adds a CGI interpreter or executable path. */
void Location::addCgiPath(const std::string &path)
{
	_cgi_path.push_back(cleanToken(path));
}

/* Adds a file extension that should be treated as CGI. */
void Location::addCgiExtension(const std::string &extension)
{
	_cgi_extension.push_back(cleanToken(extension));
}

/* Associates a CGI extension with the matching interpreter for CgiHandler::initEnv(). */
void Location::addCgiMapping(const std::string &extension, const std::string &exec_path)
{
	const std::string clean_ext = cleanToken(extension);
	const std::string clean_exec = cleanToken(exec_path);
	_ext_path[clean_ext] = clean_exec;
	addCgiExtension(clean_ext);
	addCgiPath(clean_exec);
}

/* Interprets individual location directives from the parser's token list. */
void Location::parseDirective(const std::vector<std::string> &tokens, size_t &i)
{
	if (tokens[i] == "root" && i + 1 < tokens.size())
		setRootLocation(tokens[++i]);
	else if (tokens[i] == "index" && i + 1 < tokens.size())
		setIndexLocation(tokens[++i]);
	else if (tokens[i] == "autoindex" && i + 1 < tokens.size())
		setAutoindex(tokens[++i]);
	else if (tokens[i] == "return" && i + 1 < tokens.size())
		setReturn(tokens[++i]);
	else if (tokens[i] == "alias" && i + 1 < tokens.size())
		setAlias(tokens[++i]);
	else if ((tokens[i] == "upload_path" || tokens[i] == "upload_store") && i + 1 < tokens.size())
		setUploadPath(tokens[++i]);
	else if ((tokens[i] == "upload" || tokens[i] == "upload_enable") && i + 1 < tokens.size())
		setUploadEnabled(tokens[++i]);
	else if ((tokens[i] == "methods" || tokens[i] == "allow_methods") && i + 1 < tokens.size())
	{
		while (i + 1 < tokens.size() && tokens[i + 1].find(';') == std::string::npos)
			addMethod(tokens[++i]);
		if (i + 1 < tokens.size())
			addMethod(tokens[++i]);
	}
	else if ((tokens[i] == "cgi" || tokens[i] == "cgi_pass") && i + 2 < tokens.size())
	{
		const std::string extension = tokens[++i];
		const std::string exec_path = tokens[++i];
		addCgiMapping(extension, exec_path);
	}
}

/* Loads all tokens of a location{} block into this Location object. */
void Location::loadDirectives(const std::vector<std::string> &tokens)
{
	for (size_t i = 0; i < tokens.size(); ++i)
		parseDirective(tokens, i);
	if (_methods.empty())
	{
		addMethod("GET");
		addMethod("POST");
		addMethod("DELETE");
	}
}

/* Returns the URL path of this location. */
const std::string &Location::getPath() const
{
	return (_path);
}

/* Returns the filesystem root of this location. */
const std::string &Location::getRootLocation() const
{
	return (_root);
}

/* Returns the configured index file of this location. */
const std::string &Location::getIndexLocation() const
{
	return (_index);
}

/* Returns the configured redirect target. */
const std::string &Location::getReturn() const
{
	return (_return);
}

/* Returns the configured alias path. */
const std::string &Location::getAlias() const
{
	return (_alias);
}

/* Returns the upload storage path. */
const std::string &Location::getUploadPath() const
{
	return (_upload_path);
}

/* Returns whether directory listing is enabled. */
bool Location::getAutoindex() const
{
	return (_autoindex);
}

/* Returns whether uploads are enabled or not. */
bool Location::getUploadEnabled() const
{
	return (_upload_enabled);
}

/* Returns the configured CGI interpreter paths. */
const std::vector<std::string> &Location::getCgiPath() const
{
	return (_cgi_path);
}

/* Returns the file extensions that are treated as CGI. */
const std::vector<std::string> &Location::getCgiExtension() const
{
	return (_cgi_extension);
}

/* Returns the allowed methods as comma-separated text for debug output. */
std::string Location::getPrintMethods() const
{
	std::stringstream out;
	for (std::set<std::string>::const_iterator it = _methods.begin(); it != _methods.end(); ++it)
	{
		if (it != _methods.begin())
			out << ", ";
		out << *it;
	}
	return (out.str());
}

/* Checks whether this location allows the requested HTTP method. */
bool Location::acceptsMethod(const std::string &method) const
{
	return (_methods.empty() || _methods.find(method) != _methods.end());
}
