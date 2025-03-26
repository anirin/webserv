#include "Connection.hpp"

FileStatus Connection::processAfterReadCompleted(MainConf *mainConf) {
	std::cout << "[connection] request" << rbuff_ << std::endl;

	// config と request の設定を行う
	try {
		setHttpRequest(mainConf);
	} catch(const std::exception &e) {
		// 400 Bad Request の処理を行う
		std::cerr << "[connection] Failed to parse request: " << e.what() << std::endl;
		setHttpResponse();
		setErrorFd(400);
		buildStaticFileResponse(400);
		return SUCCESS_STATIC;
	}

	// client max body size check
	// todo client max body size check
	// ここで対処すべきか不明だが、とりあえずここで処理（もっと前に処理すべきな気がする）
	if(rbuff_.size() > conf_value_._client_max_body_size) {
		std::cerr << "[connection] client max body size exceeded" << std::endl;
		setHttpResponse();
		setErrorFd(413);
		buildStaticFileResponse(413);
		return SUCCESS_STATIC;
	}

	// chunked処理
	if(isChunked()) {
		std::cout << "[connection] chunked body" << std::endl;
		try {
			setChunkedBody();
		} catch(const std::exception &e) {
			std::cerr << "[connection] Failed to parse chunked body: " << e.what() << std::endl;
			setHttpResponse();
			setErrorFd(400);
			buildStaticFileResponse(400);
			return SUCCESS_STATIC;
		}
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
			readStaticFile(file_path);
			buildStaticFileResponse(200);
			return SUCCESS_STATIC;
		}
	} else if(method == POST) {
		setCGI();
		if(cgi_ != NULL) {
			return SUCCESS_CGI;
		} else if(isFileUpload()) {
			return fileUpload();
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
