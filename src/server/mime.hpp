#ifndef MIME_HPP
# define MIME_HPP

# include <string>

class Mime
{
	public:
		static std::string	getType(const std::string &path);
};

#endif
