#include "location.hpp"

/* Erstellt eine Location mit sicheren Defaults für Root, Index und deaktivierte Extras. */
Location::Location()
	: _path("/"),
	  _root("./www/"),
	  _index("index.html"),
	  _autoindex(false),
	  _upload_enabled(false)
{
}

/* Erstellt eine Location für einen bestimmten URL-Pfad und übernimmt die Default-Werte. */
Location::Location(const std::string &path)
	: _path(cleanToken(path)),
	  _root("./www/"),
	  _index("index.html"),
	  _autoindex(false),
	  _upload_enabled(false)
{
}

/* Zerstört die Location; Strings und Container räumen sich selbst auf. */
Location::~Location() {}

/* Entfernt ein abschließendes Semikolon aus einem Config-Token. */
std::string Location::cleanToken(const std::string &value) const
{
	if (!value.empty() && value[value.length() - 1] == ';')
		return (value.substr(0, value.length() - 1));
	return (value);
}

/* Speichert den URL-Pfad dieser Location, z.B. /cgi-bin oder /static. */
void Location::setPath(const std::string &path)
{
	_path = cleanToken(path);
}

/* Speichert den Dateisystem-Root, aus dem diese Location Dateien auflöst. */
void Location::setRootLocation(const std::string &root)
{
	_root = cleanToken(root);
	if (!_root.empty() && _root[_root.length() - 1] != '/')
		_root += "/";
}

/* Speichert die Index-Datei für Verzeichnisanfragen in dieser Location. */
void Location::setIndexLocation(const std::string &index)
{
	_index = cleanToken(index);
}

/* Aktiviert oder deaktiviert Verzeichnislisting für diese Location. */
void Location::setAutoindex(const std::string &value)
{
	const std::string clean = cleanToken(value);
	_autoindex = (clean == "on" || clean == "true" || clean == "1");
}

/* Speichert das Weiterleitungsziel einer return-Direktive. */
void Location::setReturn(const std::string &value)
{
	_return = cleanToken(value);
}

/* Speichert einen Alias-Pfad, der den normalen Root ersetzen kann. */
void Location::setAlias(const std::string &value)
{
	_alias = cleanToken(value);
}

/* Speichert das Zielverzeichnis für hochgeladene Dateien. */
void Location::setUploadPath(const std::string &path)
{
	_upload_path = cleanToken(path);
}

/* Aktiviert oder deaktiviert Uploads in dieser Location. */
void Location::setUploadEnabled(const std::string &value)
{
	const std::string clean = cleanToken(value);
	_upload_enabled = (clean == "on" || clean == "true" || clean == "1");
}

/* Fügt eine erlaubte HTTP-Methode hinzu, z.B. GET oder POST. */
void Location::addMethod(const std::string &method)
{
	_methods.insert(cleanToken(method));
}

/* Fügt einen CGI-Interpreter oder ausführbaren Pfad hinzu. */
void Location::addCgiPath(const std::string &path)
{
	_cgi_path.push_back(cleanToken(path));
}

/* Fügt eine Dateiendung hinzu, die als CGI behandelt werden soll. */
void Location::addCgiExtension(const std::string &extension)
{
	_cgi_extension.push_back(cleanToken(extension));
}

/* Verknüpft eine CGI-Endung mit dem passenden Interpreter für CgiHandler::initEnv(). */
void Location::addCgiMapping(const std::string &extension, const std::string &exec_path)
{
	const std::string clean_ext = cleanToken(extension);
	const std::string clean_exec = cleanToken(exec_path);
	_ext_path[clean_ext] = clean_exec;
	addCgiExtension(clean_ext);
	addCgiPath(clean_exec);
}

/* Interpretiert einzelne Location-Direktiven aus der Tokenliste des Parsers. */
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

/* Lädt alle Tokens eines location{}-Blocks in dieses Location-Objekt. */
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

/* Gibt den URL-Pfad dieser Location zurück. */
const std::string &Location::getPath() const
{
	return (_path);
}

/* Gibt den Dateisystem-Root dieser Location zurück. */
const std::string &Location::getRootLocation() const
{
	return (_root);
}

/* Gibt die konfigurierte Index-Datei dieser Location zurück. */
const std::string &Location::getIndexLocation() const
{
	return (_index);
}

/* Gibt das konfigurierte Weiterleitungsziel zurück. */
const std::string &Location::getReturn() const
{
	return (_return);
}

/* Gibt den konfigurierten Alias-Pfad zurück. */
const std::string &Location::getAlias() const
{
	return (_alias);
}

/* Gibt den Upload-Speicherpfad zurück. */
const std::string &Location::getUploadPath() const
{
	return (_upload_path);
}

/* Gibt zurück, ob Verzeichnislisting aktiviert ist. */
bool Location::getAutoindex() const
{
	return (_autoindex);
}

/* Gibt zurück, ob Uploads aktiviert sind. */
bool Location::getUploadEnabled() const
{
	return (_upload_enabled);
}

/* Gibt die konfigurierten CGI-Interpreterpfade zurück. */
const std::vector<std::string> &Location::getCgiPath() const
{
	return (_cgi_path);
}

/* Gibt die Dateiendungen zurück, die als CGI gelten. */
const std::vector<std::string> &Location::getCgiExtension() const
{
	return (_cgi_extension);
}

/* Gibt die erlaubten Methoden als kommaseparierten Text für Debug-Ausgaben zurück. */
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

/* Prüft, ob diese Location die angefragte HTTP-Methode erlaubt. */
bool Location::acceptsMethod(const std::string &method) const
{
	return (_methods.empty() || _methods.find(method) != _methods.end());
}
