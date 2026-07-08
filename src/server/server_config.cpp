#include "server_config.hpp"
#include "../parser/config_file.hpp"
#include <cstdlib>

/* Erstellt eine ServerConfig mit neutralen Defaults, bevor Parserwerte gesetzt werden. */
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

/* Zerstört die Config; alle enthaltenen Daten werden von STL-Containern verwaltet. */
ServerConfig::~ServerConfig() {}

/* Entfernt das abschließende Semikolon eines Config-Tokens. */
std::string ServerConfig::cleanToken(const std::string &value) const
{
	if (!value.empty() && value[value.length() - 1] == ';')
		return (value.substr(0, value.length() - 1));
	return (value);
}

/* Speichert den TCP-Port, auf dem dieser Server später lauschen soll. */
void ServerConfig::setPort(const std::string &port)
{
	_port = std::atoi(cleanToken(port).c_str());
}

/* Speichert den Host und markiert, dass ein Host explizit konfiguriert wurde. */
void ServerConfig::setHost(const std::string &host)
{
	_host_string = cleanToken(host);
	_host = _host_string.empty() ? 0 : 1;
}

/* Speichert den Document Root und sorgt dafür, dass er mit '/' endet. */
void ServerConfig::setRoot(const std::string &root)
{
	_root = cleanToken(root);
	if (!_root.empty() && _root[_root.length() - 1] != '/')
		_root += "/";
}

/* Speichert die Index-Datei, die bei Verzeichnisanfragen ausgeliefert werden soll. */
void ServerConfig::setIndex(const std::string &index)
{
	_index = cleanToken(index);
}

/* Speichert den optionalen virtuellen Servernamen. */
void ServerConfig::setServerName(const std::string &name)
{
	_server_name = cleanToken(name);
}

/* Speichert die maximal erlaubte Body-Größe in Bytes. */
void ServerConfig::setClientMaxBodySize(const std::string &size)
{
	_client_max_body_size = static_cast<size_t>(std::atol(cleanToken(size).c_str()));
}

/* Aktiviert oder deaktiviert Autoindex auf Serverebene. */
void ServerConfig::setAutoindex(const std::string &value)
{
	const std::string clean = cleanToken(value);
	_autoindex = (clean == "on" || clean == "true" || clean == "1");
}

/* Erstellt aus einem location{}-Block ein Location-Objekt und übernimmt Root/Index als Defaults. */
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

/* Parst error_page-Direktiven der Form: error_page 404 /404.html;. */
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

/* Gibt den optionalen virtuellen Servernamen zurück. */
const std::string &ServerConfig::getServerName() const
{
	return (_server_name);
}

/* Gibt 0 zurück, wenn kein Host gesetzt wurde, sonst 1 für Parser-Duplikatprüfungen. */
int ServerConfig::getHost() const
{
	return (_host);
}

/* Gibt den konfigurierten Host-String zurück. */
const std::string &ServerConfig::getHostString() const
{
	return (_host_string);
}

/* Gibt den Document Root des Servers zurück. */
const std::string &ServerConfig::getRoot() const
{
	return (_root);
}

/* Gibt die konfigurierte Index-Datei zurück. */
const std::string &ServerConfig::getIndex() const
{
	return (_index);
}

/* Gibt den Port zurück, auf dem der Server lauschen soll. */
int ServerConfig::getPort() const
{
	return (_port);
}

/* Gibt die maximal erlaubte Body-Größe zurück. */
size_t ServerConfig::getClientMaxBodySize() const
{
	return (_client_max_body_size);
}

/* Gibt die Map aus Statuscode zu Error-Page-Pfad zurück. */
const std::map<short, std::string> &ServerConfig::getErrorPages() const
{
	return (_error_pages);
}

/* Gibt alle konfigurierten Locations unveränderbar zurück. */
const std::vector<Location> &ServerConfig::getLocations() const
{
	return (_locations);
}

/* Gibt alle Locations veränderbar zurück, z.B. für CGI-Testaufbau. */
std::vector<Location> &ServerConfig::getLocations()
{
	return (_locations);
}

/* Prüft, ob zwei location{}-Blöcke denselben Pfad benutzen. */
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

/* Prüft, ob Error-Page-Statuscodes im gültigen HTTP-Fehlerbereich liegen. */
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
