#include "Connection.hpp"

// autoindexの場合の処理
// todo location path と被る部分がある
std::string getFilesystemPath(std::string request_path, std::string root_path) {
	std::string ret;

	ret = "." + root_path;
	if(!ret.empty() && ret[ret.length() - 1] != '/')
		ret += '/';

	if(request_path[0] == '/')
		request_path = request_path.substr(1);
	ret += request_path;

	return ret;
}

std::string Connection::getAutoIndexPath() { // throw
	if(conf_value_._autoindex == false) {
		return "";
	}

	std::string request_path = request_->getRequestPath();
	if(request_path[request_path.length() - 1] != '/')
		return "";

	std::string location_path = getFilesystemPath(request_path, conf_value_._root);
	struct stat path_stat;

	if(!stat(location_path.c_str(), &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
		throw std::runtime_error("Failed to get file status");
	}

	return location_path;
}

void Connection::buildAutoIndexContent(std::string path) { // throw
	DIR *dir;
	struct dirent *entry;
	struct stat file_stat;
	std::stringstream html;

	dir = opendir(path.c_str());
	if(!dir) {
		std::cerr << "[autoindex] Cannot open directory: " << path << std::endl;
		throw std::runtime_error("Cannot open directory");
	}

	html << "<html>\r\n"
		 << "<head><title>Index of " << path << "</title></head>\r\n"
		 << "<body>\r\n"
		 << "<h1>Index of " << path << "</h1>\r\n"
		 << "<hr>\r\n"
		 << "<pre>\r\n";

	html << "<a href=\"../\">../</a>\r\n";

	while((entry = readdir(dir)) != NULL) {
		std::string name = entry->d_name;

		if(name == "." || name == "..")
			continue;

		std::string full_path = path;
		if(path[path.length() - 1] != '/')
			full_path += "/";
		full_path += name;

		if(stat(full_path.c_str(), &file_stat) != 0)
			continue;

		bool is_dir = S_ISDIR(file_stat.st_mode);

		if(is_dir) {
			name += "/";
		}
		html << "<a href=\"" << name << "\">" << std::left << std::setw(50) << name << "</a>";
		html << "\r\n";
	}

	html << "</pre>\r\n"
		 << "<hr>\r\n"
		 << "</body>\r\n"
		 << "</html>\r\n";

	closedir(dir);

	std::string content;
	content = html.str();
	wbuff_ = stringToVector(content);

	return;
}
