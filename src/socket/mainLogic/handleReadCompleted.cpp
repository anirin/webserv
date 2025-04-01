#include "Connection.hpp"

FileStatus Connection::processAfterReadCompleted(MainConf *mainConf) {
	// std::cout << "<============ rbuff_: ==============> " << std::endl
	// std::cout << std::string(rbuff_.begin(), rbuff_.end()) << std::endl;
	// std::cout << "<============ rbuff_: ==============> " << std::endl;

	// config と request の設定を行う
	try {
		setHttpRequest(mainConf);
	} catch(const std::exception &e) {
		// 400 Bad Request の処理を行う
		std::cerr << e.what() << std::endl;
		std::cerr << "[connection] Failed to parse request: " << e.what() << std::endl;
		setHttpResponse();
		buildBadRequestResponse();
		return SUCCESS_STATIC;
	}

	// client max body size check
	// std::cout << "[connection] rbuff_ size: " << rbuff_.size() << ", " << conf_value_._client_max_body_size <<
	if(rbuff_.size() > conf_value_._client_max_body_size) {
		std::cerr << "[connection] client max body size exceeded" << std::endl;
		setHttpResponse();
		setErrorFd(413);
		buildStaticFileResponse(413);
		return SUCCESS_STATIC;
	}

	// autoindex処理
	std::string autoindex_path = getAutoIndexPath();
	if(autoindex_path != "") {
		std::cout << "[connection] autoindex is set" << std::endl;
		setHttpResponse();
		buildAutoIndexContent(autoindex_path);
		buildStaticFileResponse(200);
		return SUCCESS_STATIC;
	}

	// redirect処理
	std::pair<int, std::string> redirect = conf_value_._return;
	if(redirect.first != 0 && redirect.second != "") {
		return buildRedirectResponse(redirect.first, redirect.second);
	}

	// エラーハンドリング(405, 404, 505)
	int status_code = request_->getStatusCode();
	if(status_code != 200) {
		setHttpResponse();
		setErrorFd(status_code);
		buildStaticFileResponse(status_code);
		return SUCCESS_STATIC;
	}

	Method method = request_->getMethod();
	// todo クエリパラメーターの処理を追加
	if(method == GET) {
		setCGI();
		if(cgi_ != NULL)
			return SUCCESS_CGI;
		else {
			std::string file_path = request_->getLocationPath();
			std::cout << "[connection] file path: " << file_path << std::endl;
			return readStaticFile(file_path);
		}
	} else if(method == POST) {
		setCGI();
		if(cgi_ != NULL) {
			return SUCCESS_CGI;
		} else if(isFileUpload()) {
			std::string upload_dir = request_->getLocationPath();
			return fileUpload(upload_dir);
		} else {
			setErrorFd(400);
			buildStaticFileResponse(400);
			return SUCCESS_STATIC;
		}
	} else if(method == DELETE) {
		deleteFile();
		buildStaticFileResponse(200);
		return SUCCESS_STATIC;
	}

	return SUCCESS;
}
