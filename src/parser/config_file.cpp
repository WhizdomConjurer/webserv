#include "config_file.hpp"

/* Creates an empty file helper with no stored path. */
ConfigFile::ConfigFile() : _size(0) { }

/* Stores the config path to be validated and loaded later. */
ConfigFile::ConfigFile(const std::string &path) : _path(path), _size(0) { }

/* Destroys the file helper; no dynamic resources need to be released. */
ConfigFile::~ConfigFile() { }


/* Classifies a path: 1 for file, 2 for directory, 3 for other types, and -1 on stat() failure. */
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

/* Uses access() to check whether a path has the required permissions. */
int	ConfigFile::checkFile(const std::string &path, int mode)
{
	return (access(path.c_str(), mode));
}

/* Checks whether index exists as a readable file, either directly or relative to path. */
int ConfigFile::isFileExistAndReadable(const std::string &path, const std::string &index)
{
	if (getTypePath(index) == 1 && checkFile(index, 4) == 0)
		return (0);
	if (getTypePath(path + index) == 1 && checkFile(path + index, 4) == 0)
		return (0);
	return (-1);
}

/* Reads the entire file into a string and returns an empty string if an error occurs. */
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

/* Returns the stored path to the config file. */
const std::string &ConfigFile::getPath() const
{
	return (_path);
}

/* Returns the stored file size; currently unused and always remains 0. */
size_t ConfigFile::getSize() const
{
	return (_size);
}
