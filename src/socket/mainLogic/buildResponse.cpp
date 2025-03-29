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
		std::string content = "<html>\r\n"
							  "<head><title>" +
							  status_code_phrase[status_code_str] +
							  "</title></head>\r\n"
							  "<body>\r\n"
							  "<h1>" +
							  status_code_str + " " + status_code_phrase[status_code_str] +
							  "</h1>\r\n"
							  "<p>error page is not set</p>\r\n"
							  "<p>please set error_page in config file</p>\r\n"
							  "</body>\r\n"
							  "</html>\r\n";
		wbuff_ = stringToVector(content);

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
	if(request_ == NULL) {
		std::cerr << "[connection] request is NULL" << std::endl;
		return;
	}
	std::map<std::string, std::string> r_header = request_->getHeader();
	std::string path = request_->getLocationPath();
	std::string server_name = request_->getServerName();

	response_ = new HttpResponse();
	response_->setBody(wbuff_);
	response_->setStartLine(status_code);
	response_->setHeader(r_header, path, server_name);

	wbuff_ = response_->buildResponse();
	std::cout << "[connection] response build" << std::endl;
}

void Connection::buildBadRequestResponse() {
	response_ = new HttpResponse();
	std::string body = "<html>\r\n"
					   "<head><title>400 Bad Request</title></head>\r\n"
					   "<body>\r\n"
					   "<h1>400 Bad Request</h1>\r\n"
					   "</body>\r\n"
					   "</html>\r\n";

	response_->setBody(stringToVector(body));
	response_->setStartLine(400);
	response_->setBadRequestHeader();
	wbuff_ = response_->buildResponse();
	std::cout << "[connection] bad request response build" << std::endl;
}
