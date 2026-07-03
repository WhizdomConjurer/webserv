#include "location.hpp"

/* Creates an empty dummy route with safe defaults. */
Location::Location()
	: _path("/"),
	  _root("./www/"),
	  _index("index.html"),
	  _autoindex(false),
	  _upload_enabled(false)
{
}

/* Creates a dummy route for one path while keeping safe defaults. */
Location::Location(const std::string &path)
	: _path(cleanToken(path)),
	  _root("./www/"),
	  _index("index.html"),
	  _autoindex(false),
	  _upload_enabled(false)
{
}

/* Destroys the route; all owned data is STL-managed. */
Location::~Location() {}

/* Removes a trailing semicolon from one config token. */
std::string Location::cleanToken(const std::string &value) const
{
	if (!value.empty() && value[value.length() - 1] == ';')
		return (value.substr(0, value.length() - 1));
	return (value);
}

/* Stores the route path, for example /cgi-bin or /static. */
void Location::setPath(const std::string &path)
{
	_path = cleanToken(path);
}

/* Stores the filesystem root used to resolve requests in this route. */
void Location::setRootLocation(const std::string &root)
{
	_root = cleanToken(root);
	if (!_root.empty() && _root[_root.length() - 1] != '/')
		_root += "/";
}

/* Stores the default index file served for directory requests. */
void Location::setIndexLocation(const std::string &index)
{
	_index = cleanToken(index);
}

/* Enables or disables autoindex for directory requests. */
void Location::setAutoindex(const std::string &value)
{
	const std::string clean = cleanToken(value);
	_autoindex = (clean == "on" || clean == "true" || clean == "1");
}

/* Stores the redirection target configured for this route. */
void Location::setReturn(const std::string &value)
{
	_return = cleanToken(value);
}

/* Stores an alias path for this route. */
void Location::setAlias(const std::string &value)
{
	_alias = cleanToken(value);
}

/* Stores the directory where uploaded files should be written. */
void Location::setUploadPath(const std::string &path)
{
	_upload_path = cleanToken(path);
}

/* Enables or disables uploads for this route. */
void Location::setUploadEnabled(const std::string &value)
{
	const std::string clean = cleanToken(value);
	_upload_enabled = (clean == "on" || clean == "true" || clean == "1");
}

/* Adds one accepted HTTP method to the route. */
void Location::addMethod(const std::string &method)
{
	_methods.insert(cleanToken(method));
}

/* Adds one CGI executable/interpreter path. */
void Location::addCgiPath(const std::string &path)
{
	_cgi_path.push_back(cleanToken(path));
}

/* Adds one extension that should be handled as CGI. */
void Location::addCgiExtension(const std::string &extension)
{
	_cgi_extension.push_back(cleanToken(extension));
}

/* Adds an extension-to-executable mapping used by CgiHandler::initEnv(). */
void Location::addCgiMapping(const std::string &extension, const std::string &exec_path)
{
	const std::string clean_ext = cleanToken(extension);
	const std::string clean_exec = cleanToken(exec_path);
	_ext_path[clean_ext] = clean_exec;
	addCgiExtension(clean_ext);
	addCgiPath(clean_exec);
}

/* Parses the minimal route directives needed by the current parser and CGI code. */
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

/* Loads a whole location block token list into this dummy route object. */
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

/* Returns the configured route path. */
const std::string &Location::getPath() const
{
	return (_path);
}

/* Returns the filesystem root configured for this route. */
const std::string &Location::getRootLocation() const
{
	return (_root);
}

/* Returns the configured directory index file. */
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

/* Returns the configured upload storage path. */
const std::string &Location::getUploadPath() const
{
	return (_upload_path);
}

/* Returns whether directory listing is enabled. */
bool Location::getAutoindex() const
{
	return (_autoindex);
}

/* Returns whether uploads are enabled. */
bool Location::getUploadEnabled() const
{
	return (_upload_enabled);
}

/* Returns the configured CGI executable/interpreter paths. */
const std::vector<std::string> &Location::getCgiPath() const
{
	return (_cgi_path);
}

/* Returns the configured CGI extensions. */
const std::vector<std::string> &Location::getCgiExtension() const
{
	return (_cgi_extension);
}

/* Returns accepted methods as a printable comma-separated string. */
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

/* Returns true if the route accepts method or no explicit method list exists. */
bool Location::acceptsMethod(const std::string &method) const
{
	return (_methods.empty() || _methods.find(method) != _methods.end());
}
