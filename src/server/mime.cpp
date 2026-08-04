#include "mime.hpp"

/* Determines the Content-Type for static responses based on the file extension. */
std::string Mime::getType(const std::string &path)
{
	const size_t dot = path.rfind('.');
	if (dot == std::string::npos)
		return ("application/octet-stream");
	const std::string ext = path.substr(dot);
	if (ext == ".html" || ext == ".htm")
		return ("text/html");
	if (ext == ".css")
		return ("text/css");
	if (ext == ".js")
		return ("application/javascript");
	if (ext == ".json")
		return ("application/json");
	if (ext == ".png")
		return ("image/png");
	if (ext == ".jpg" || ext == ".jpeg")
		return ("image/jpeg");
	if (ext == ".txt")
		return ("text/plain");
	return ("application/octet-stream");
}
