#include "config_file.hpp"

/* Erstellt einen leeren Dateihelfer ohne gespeicherten Pfad. */
ConfigFile::ConfigFile() : _size(0) { }

/* Speichert den Config-Pfad, der später geprüft und gelesen wird. */
ConfigFile::ConfigFile(const std::string &path) : _path(path), _size(0) { }

/* Zerstört den Dateihelfer; dynamische Ressourcen werden nicht gehalten. */
ConfigFile::~ConfigFile() { }


/* Klassifiziert einen Pfad: 1 Datei, 2 Ordner, 3 sonstiger Typ, -1 bei stat()-Fehler. */
int ConfigFile::getTypePath(const std::string &path)
{
	struct stat	buffer;
	int			result;
	
	result = stat(path.c_str(), &buffer);
	if (result == 0)
	{
		if (buffer.st_mode & S_IFREG)
			return (1);
		else if (buffer.st_mode & S_IFDIR)
			return (2);
		else
			return (3);
	}
	else
		return (-1);
}

/* Prüft mit access(), ob ein Pfad die gewünschten Rechte erfüllt. */
int	ConfigFile::checkFile(const std::string &path, int mode)
{
	return (access(path.c_str(), mode));
}

/* Prüft, ob index direkt oder relativ zu path als lesbare Datei existiert. */
int ConfigFile::isFileExistAndReadable(const std::string &path, const std::string &index)
{
	if (getTypePath(index) == 1 && checkFile(index, 4) == 0)
		return (0);
	if (getTypePath(path + index) == 1 && checkFile(path + index, 4) == 0)
		return (0);
	return (-1);
}

/* Liest eine komplette Datei in einen String und gibt bei Fehler einen leeren String zurück. */
std::string	ConfigFile::readFile(const std::string &path)
{
	if (path.empty())
		return ("");
	std::ifstream config_file(path.c_str());
	if (!config_file || !config_file.is_open())
		return ("");

	std::stringstream stream_binding;
	stream_binding << config_file.rdbuf();
	return (stream_binding.str());
}

/* Gibt den gespeicherten Config-Dateipfad zurück. */
const std::string &ConfigFile::getPath() const
{
	return (_path);
}

/* Gibt die gespeicherte Dateigröße zurück; aktuell bleibt sie ungenutzt bei 0. */
size_t ConfigFile::getSize() const
{
	return (_size);
}
