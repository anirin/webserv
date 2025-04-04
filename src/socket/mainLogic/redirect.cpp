#include "Connection.hpp"

// redirectの処理の場合
FileStatus Connection::buildRedirectResponse(int status_code, std::string redirect_path) {
	// std::cout << "[connection] building redirect response to: " << redirect_path << std::endl;

	std::map<std::string, std::string> r_header = request_->getHeader();

	response_ = new HttpResponse();
	setErrorFd(status_code); // error ではないが同じロジック
	response_->setBody(wbuff_);
	response_->setStartLine(status_code);
	r_header["Location"] = redirect_path;
	response_->setHeader(r_header, "", request_->getServerName());

	wbuff_ = response_->buildResponse();
	return SUCCESS_STATIC;
}