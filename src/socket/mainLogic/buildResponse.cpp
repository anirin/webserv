#include "Connection.hpp"

// todo 命名変更
void Connection::setErrorFd(int status_code) {
	if(conf_value_._error_page.empty() ||
	   (!conf_value_._error_page.empty() &&
		conf_value_._error_page.find(status_code) == conf_value_._error_page.end())) {
		std::map<std::string, std::string> status_code_phrase;

		status_code_phrase["301"] = "Moved Permanently";
		status_code_phrase["302"] = "Found";
		status_code_phrase["400"] = "Bad Request";
		status_code_phrase["404"] = "Not Found";
		status_code_phrase["405"] = "Method Not Allowed";
		status_code_phrase["413"] = "Content Too Long";
		status_code_phrase["500"] = "Internal Server Error";
		status_code_phrase["504"] = "Gateway Timeout";
		status_code_phrase["505"] = "HTTP Version Not Supported";

		std::stringstream ss;
		ss << status_code;
		std::string status_code_str = ss.str();

		std::cerr << "[connection] error_page is not set" << std::endl;
		wbuff_ = "<html>\r\n"
				 "<head><title>" +
				 status_code_phrase[status_code_str] +
				 "</title></head>\r\n"
				 "<body>\r\n"
				 "<h1>" +
				 status_code_str + " " + status_code_phrase[status_code_str] +
				 "</h1>\r\n"
				 "</body>\r\n"
				 "</html>\r\n";

		return;
	}

	// 本来は内部リダイレクトによって更新処理をおこなう
	// ヘッダーを書き換えて 再帰的にprocess after read
	// completedを呼び出すと思われる（無限ループに陥らないように注意する） error page は絶対パスで指定されることにする
	std::string error_page = conf_value_._error_page[status_code];
	std::cout << "[connection] error_page: " << error_page << " is set" << std::endl;
	readStaticFile(error_page);

	return;
}

void Connection::buildStaticFileResponse(int status_code) {
	std::cout << "reach 1 here in buildStaticFileResponse" << std::cout;
	std::map<std::string, std::string> r_header = request_->getHeader();
	std::cout << "reach 2 here in buildStaticFileResponse" << std::cout;
	std::string path = request_->getLocationPath();
	std::cout << "reach 3 here in buildStaticFileResponse" << std::cout;
	std::string server_name = request_->getServerName();

	std::cout << "reach 4 here in buildStaticFileResponse" << std::cout;
	response_ = new HttpResponse();
	response_->setBody(wbuff_);
	response_->setStartLine(status_code);
	response_->setHeader(r_header, path, server_name);

	wbuff_ = response_->buildResponse();
	std::cout << "[connection] response build" << std::endl;
}
