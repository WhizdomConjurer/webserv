#include "config_file.hpp"

/* Creates an empty config file helper. */
ConfigFile::ConfigFile() : _size(0) { }

/* Stores the config path that should later be checked and read. */
ConfigFile::ConfigFile(const std::string &path) : _path(path), _size(0) { }

/* Destroys the helper; no dynamic resources are owned. */
ConfigFile::~ConfigFile() { }


/* Returns 1 for regular file, 2 for directory, 3 for other, or -1 on stat failure. */
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

/* Wraps access() to check whether path satisfies the requested permission mode. */
int	ConfigFile::checkFile(const std::string &path, int mode)
{
	return (access(path.c_str(), mode));
}

/* Checks whether index is readable either as absolute path or relative to path. */
int ConfigFile::isFileExistAndReadable(const std::string &path, const std::string &index)
{
	if (getTypePath(index) == 1 && checkFile(index, 4) == 0)
		return (0);
	if (getTypePath(path + index) == 1 && checkFile(path + index, 4) == 0)
		return (0);
	return (-1);
}

/* Reads the whole file into a string; returns an empty string on failure. */
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

/* Returns the stored configuration file path. */
const std::string &ConfigFile::getPath() const
{
	return (_path);
}

/* Returns the cached file size value, currently unused and initialized to 0. */
size_t ConfigFile::getSize() const
{
	return (_size);
}
