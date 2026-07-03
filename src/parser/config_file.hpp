#ifndef CONFIGFILE_HPP
#define CONFIGFILE_HPP

#include <cstddef>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

class ConfigFile {
	private:
		std::string		_path;
		size_t			_size;	// проверить нужен ли

	public:
		ConfigFile();
		explicit ConfigFile(const std::string &path);
		~ConfigFile();

		static int getTypePath(const std::string &path);
		static int checkFile(const std::string &path, int mode);
		std::string	readFile(const std::string &path);
		static int isFileExistAndReadable(const std::string &path, const std::string &index);

		const std::string &getPath() const;
		size_t getSize() const;
};

#endif
