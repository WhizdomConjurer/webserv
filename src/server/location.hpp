#ifndef LOCATION_HPP
# define LOCATION_HPP

# include <map>
# include <set>
# include <sstream>
# include <string>
# include <vector>

class Location
{
	private:
		std::string					_path;
		std::string					_root;
		std::string					_index;
		std::string					_return;
		std::string					_alias;
		bool						_autoindex;
		bool						_upload_enabled;
		std::string					_upload_path;
		std::set<std::string>		_methods;
		std::vector<std::string>		_cgi_path;
		std::vector<std::string>		_cgi_extension;

		static std::string	cleanToken(const std::string &value);
		void		parseDirective(const std::vector<std::string> &tokens, size_t &i);

	public:
		std::map<std::string, std::string>	_ext_path;

		Location();
		explicit Location(const std::string &path);
		~Location();

		void	setPath(const std::string &path);
		void	setRootLocation(const std::string &root);
		void	setIndexLocation(const std::string &index);
		void	setAutoindex(const std::string &value);
		void	setReturn(const std::string &value);
		void	setAlias(const std::string &value);
		void	setUploadPath(const std::string &path);
		void	setUploadEnabled(const std::string &value);
		void	addMethod(const std::string &method);
		void	addCgiPath(const std::string &path);
		void	addCgiExtension(const std::string &extension);
		void	addCgiMapping(const std::string &extension, const std::string &exec_path);
		void	loadDirectives(const std::vector<std::string> &tokens);

		const std::string			&getPath() const;
		const std::string			&getRootLocation() const;
		const std::string			&getIndexLocation() const;
		const std::string			&getReturn() const;
		const std::string			&getAlias() const;
		const std::string			&getUploadPath() const;
		bool						getAutoindex() const;
		bool						getUploadEnabled() const;
		const std::vector<std::string>	&getCgiPath() const;
		const std::vector<std::string>	&getCgiExtension() const;
		std::string					getPrintMethods() const;
		bool						acceptsMethod(const std::string &method) const;
};

#endif
