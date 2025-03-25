/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: atsu <atsu@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/13 11:25:14 by rmatsuba          #+#    #+#             */
<<<<<<< Updated upstream
/*   Updated: 2025/03/23 17:05:41 by rmatsuba         ###   ########.fr       */
=======
/*   Updated: 2025/03/25 15:01:54 by atsu             ###   ########.fr       */
>>>>>>> Stashed changes
/*                                                                            */
/* ************************************************************************** */

#include "Connection.hpp"

std::time_t Connection::timeout_ = 100000;
ssize_t buff_size = 1024; // todo 持たせ方の検討

// ==================================== constructor and destructor ====================================

Connection::Connection() : ASocket() {}

Connection::Connection(int listenerFd) : ASocket() { // throw
	/* make a new socket for the client */
	socklen_t len = sizeof(addr_);
	fd_ = accept(listenerFd, (struct sockaddr *)&addr_, &len);
	if(fd_ == -1)
		throw std::runtime_error("accept failed");
	if(fcntl(fd_, F_SETFL, O_NONBLOCK))
		throw std::runtime_error("Failed to set socket to non-blocking");
	request_ = NULL;
	response_ = NULL;
	lastActive_ = std::time(NULL);
	std::cout << "[connection] Accepted connection from sin_port = " << addr_.sin_port << std::endl;

	cgi_ = NULL;
}

Connection::Connection(const Connection &other) : ASocket(other) {
	if(this == &other)
		return;
	fd_ = other.fd_;
	cgi_ = other.cgi_;
	rbuff_ = other.rbuff_;
	wbuff_ = other.wbuff_;
	request_ = other.request_;
	response_ = other.response_;
	lastActive_ = other.lastActive_;
}

Connection::~Connection() {
	close(fd_);
}

// ==================================== getter ====================================

int Connection::getFd() const {
	return fd_;
}

CGI *Connection::getCGI() const {
	return cgi_;
}

FileTypes Connection::getFdType(int fd) const {
	if(cgi_ != NULL && fd == cgi_->getFd())
		return PIPE;
	else
		return SOCKET;
}

// ==================================== setter ====================================

void Connection::setHttpRequest(MainConf *mainConf) { // throw
	try {
		request_ = new HttpRequest(rbuff_, mainConf);
	} catch(const std::exception &e) {
		std::cerr << "[connection] Failed to parse request: " << e.what() << std::endl;
		throw std::runtime_error("Failed to parse request");
	}
	conf_value_ = mainConf->getConfValue(request_->getPort(), request_->getServerName(), request_->getRequestPath());
	std::cout << "[connection] request is set" << std::endl;
	std::cout << rbuff_ << std::endl;

	mainConf->debug_print_conf_value(conf_value_);
}

void Connection::setHttpResponse() {
	response_ = new HttpResponse();
}

void Connection::setCGI() { // throw
	std::string location_path = request_->getLocationPath();
	int path_size = location_path.size();

	if(path_size > 4 && location_path.substr(path_size - 4, 4) == ".php") {
		CGI *cgi = new CGI(location_path);
		if(cgi == NULL) {
			std::cerr << "[connection] Failed to create CGI object" << std::endl;
			throw std::runtime_error("Failed to create CGI object");
		}
		cgi_ = cgi;
		std::cout << "[connection] cgi is set" << std::endl;
		return;
	}
}

void Connection::clearValue() {
	response_ = NULL;
}

// ==================================== check ==============================================

bool Connection::isTimedOut(MainConf *mainConf) {
	std::time_t now = std::time(NULL);
	if(now - lastActive_ <= timeout_) {
		lastActive_ = now;
		return false;
	}
	std::cout << "[connection] time out" << std::endl;

	setHttpRequest(mainConf);
	setHttpResponse();
	response_->setStatusCode(504);

	setErrorFd(504);

	return true;
}

// ==================================== read and write ====================================

<<<<<<< Updated upstream
FileStatus Connection::processAfterReadCompleted(MainConf *mainConf) {
	try {
		setHttpRequest(mainConf);
	} catch (const std::exception &e) {
		// 400 Bad Request の処理を行う
		std::cerr << "[connection] Failed to parse request: " << e.what() << std::endl;
		setHttpResponse();
		setErrorFd(400);
		buildStaticFileResponse(400);
		return SUCCESS_STATIC;
	}
	setHttpResponse();

	// std::cout << "[connection] request" << rbuff_ << std::endl;
	// client max body size check ここで対処すべきか不明だが、とりあえずここで処理（もっと前に処理すべきな気がする）
	// 理由:confが欲しいのでここに配置する requset 設置後だと思われる
	std::cout << rbuff_.size() << " == " << conf_value_._client_max_body_size << std::endl;
	if(rbuff_.size() > conf_value_._client_max_body_size) {
		std::cerr << "[connection] client max body size exceeded" << std::endl;
		setErrorFd(413);
		buildStaticFileResponse(413);
		return SUCCESS_STATIC;
	}

	// config 設定
	std::string requestPath = request_->getRequestPath();
	std::string fsPath = getFilesystemPath(requestPath);

	// autoindex処理
	if(isAutoindexableDirectory(fsPath)) {
		std::cout << "[connection] autoindex is set" << std::endl;
		wbuff_ = buildAutoIndexContent(fsPath);
		buildStaticFileResponse(200);
		return SUCCESS_STATIC;
	}

	// redirect処理
	std::vector<std::string> redirect = conf_value_._return;
	if(!redirect.empty() && redirect.size() > 1) {
		std::string redirectPath = redirect[1];
		return buildRedirectResponse(redirectPath);
	}

	// error ハンドリング
	int status_code = request_->getStatusCode();
	if (status_code != 200) {
		setErrorFd(status_code);
		buildStaticFileResponse(status_code);
		return SUCCESS_STATIC;
	}

	Method method = request_->getMethod();
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
		// todo CGIの処理を追加する
		if(isFileUpload())
		{
			return fileUpload();
		}
		else
		{
			setErrorFd(400);
			buildStaticFileResponse(400);
			return SUCCESS_STATIC;
		}
	} else if(method == DELETE) {
		//すでにファイルが存在するか確認ずみ
		std::remove(request_->getLocationPath().c_str());
		std::cout << "delete file" << std::endl;
		std::string response_body = "<html>\r\n"
									"<head><title>200 OK</title></head>\r\n"
									"<body>\r\n"
									"<h1>200 OK</h1>\r\n"
									"<p>File deleted successfully</p>\r\n"
									"</body>\r\n"
									"</html>\r\n";
		response_->setBody(response_body);
		response_->setStartLine(200);
		response_->setHeader(request_->getHeader(), request_->getLocationPath(), request_->getServerName());
		return SUCCESS_STATIC;
	} else {
		// todo
	}

	return SUCCESS;
}

=======
>>>>>>> Stashed changes
FileStatus Connection::readSocket(MainConf *mainConf) {
	char buff[buff_size];
	ssize_t rlen = recv(fd_, buff, buff_size, 0);

	if(rlen < 0) {
		return ERROR;
	} else if(rlen == 0) {
		std::cout << "[connection] read socket closed by client" << std::endl;
		return CLOSED;
	} else if(rlen == buff_size) {
		rbuff_ += buff;
		return NOT_COMPLETED;
	}

	buff[rlen] = '\0';
	rbuff_ += buff;

	// std::cout << "rbuff: " << rbuff_ << std::endl;

	return processAfterReadCompleted(mainConf);
}

FileStatus Connection::readStaticFile(std::string file_path) {
	std::ifstream ifs(file_path.c_str(), std::ios::binary); // バイナリモード推奨
	if(!ifs) {
		std::cerr << "[connection] open file failed" << std::endl;
		return ERROR;
	}

	// ファイル全体を一度に読み込み
	wbuff_.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());

	if(ifs.bad()) {
		std::cerr << "[connection] read error" << std::endl;
		return ERROR;
	}

	ifs.close();
	return SUCCESS;
}

FileStatus Connection::readCGI() {
	char buff[buff_size];
	ssize_t rlen = read(cgi_->getFd(), buff, sizeof(buff) - 1);

	if(rlen < 0) {
		std::cerr << "[connection] read pipe failed" << std::endl;
		cgi_->killCGI();
		return ERROR;
	} else if(rlen == 1023) {
		wbuff_ += buff;
		return NOT_COMPLETED;
	}

	buff[rlen] = '\0';
	wbuff_ += buff;
	// todo chunked の場合はここで処理を行う 例） wbuff_ += "0\r\n\r\n";

	close(cgi_->getFd());
	delete cgi_;
	cgi_ = NULL;

	return SUCCESS;
}

FileStatus Connection::writeSocket() {
	char buff[buff_size];

	if(!request_) {
		std::cerr << "[connection] No request found" << std::endl;
		return ERROR;
	}

	if(wbuff_.empty()) {
		return NOT_COMPLETED;
	}

	// std::cout << wbuff_ << std::endl; // デバッグ用

	ssize_t copy_len = std::min(wbuff_.size(), static_cast<std::size_t>(buff_size));
	std::memcpy(buff, wbuff_.data(), copy_len);
	if(copy_len != buff_size)
		buff[copy_len] = '\0';
	wbuff_.erase(0, copy_len);
	ssize_t wlen = send(fd_, buff, copy_len, 0);
	if(wlen == -1)
		return ERROR;
	if(wlen == buff_size)
		return NOT_COMPLETED;

	delete response_;
	delete request_;
	response_ = NULL;
	request_ = NULL;
	std::cout << "[connection] write socket completed" << std::endl;
	return SUCCESS;
}

void Connection::cleanUp() {
	if(cgi_ != NULL && cgi_->getFd() != -1) {
		cgi_->killCGI();
		delete cgi_;
		cgi_ = NULL;
	}
}