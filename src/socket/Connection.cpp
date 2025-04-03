/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: atsu <atsu@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/13 11:25:14 by rmatsuba          #+#    #+#             */
/*   Updated: 2025/04/03 20:06:45 by atsu             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Connection.hpp"

std::time_t Connection::timeout_ = 10;

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

	is_timeout_ = false;
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
	// if (cgi_ != NULL) {
	// 	delete cgi_;
	// 	cgi_ = NULL;
	// }
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

bool Connection::getIsTimeout() const {
	return is_timeout_;
}

// ==================================== setter ====================================

void Connection::setHttpRequest(MainConf *mainConf) { // throw
	try {
		request_ = new HttpRequest(rbuff_, mainConf);
	} catch(const std::exception &e) {
		// ここでエラーが発生している, telnetを使っている時
		throw std::runtime_error(e.what());
	}

	try {
		conf_value_ =
			mainConf->getConfValue(request_->getPort(), request_->getServerName(), request_->getRequestPath());
	} catch(const std::exception &e) { throw std::runtime_error(e.what()); }
	std::cout << "[connection] request is set" << std::endl;
	//  std::cout << rbuff_ << std::endl;

	// mainConf->debug_print_conf_value(conf_value_);
}

void Connection::setHttpResponse() {
	response_ = new HttpResponse();
}

bool Connection::isCGI() {
	std::string location_path = request_->getLocationPath();
	int path_size = location_path.size();

	if(path_size <= 4 || location_path.substr(path_size - 4, 4) != ".php") {
		return false;
	}
	return true;
}

void Connection::executeCGI() { // throw
	CGI *cgi = NULL;

	std::string script_path = request_->getLocationPath();
	std::string query_string = request_->getQueryString();
	std::map<std::string, std::string> headers = request_->getHeader();
	std::vector<char> body = request_->getBody();

	Method method = request_->getMethod();
	if(method == POST) {
		cgi = new CGI(script_path, query_string, body, headers);
	} else {
		cgi = new CGI(script_path, query_string, headers);
	}

	if(cgi == NULL) {
		std::cerr << "[connection] Failed to create CGI object" << std::endl;
		throw std::runtime_error("Failed to create CGI object");
	}

	// CGIを実行し、パイプFDを設定
	try {
		cgi->execute();
	} catch(const std::exception &e) {
		delete cgi;
		throw std::runtime_error(std::string("[connection] CGI execution failed: ") + e.what());
	}

	cgi_ = cgi;
	std::cout << "[connection] CGI is set, fd=" << cgi_->getFd() << std::endl;
	return;
}

void Connection::clearValue() {
	delete response_;
	delete request_;
	response_ = NULL;
	request_ = NULL;

	rbuff_.clear();
	wbuff_.clear();

	lastActive_ = std::time(NULL);
}

// ==================================== check ==============================================

int Connection::isTimedOut() {
	if(is_timeout_) {
		std::cout << "[connection] already timed out" << std::endl;
		return 0;
	}

	std::time_t now = std::time(NULL);
	std::time_t diff = now - lastActive_;
	if(diff <= timeout_) {
		return 0;
	}

	std::cout << "[connection] time out" << std::endl;

	// ケース1: keep alive の待ち時間でtime outする場合
	// 単に切断する（status code は出さない）
	if(request_ == NULL && rbuff_.empty()) {
		std::cout << "[connection] keep-alive timeout - closing connection" << std::endl;
		is_timeout_ = true;
		return 1;
	}

	// ケース2: request が長時間かけて送られる場合 408 Request Timeout
	if(request_ == NULL && !rbuff_.empty()) {
		std::cout << "[connection] request timeout - sending 408" << std::endl;
		setHttpResponse();
		response_->setStatusCode(408);
		setErrorFd(408);
		buildStaticFileResponse(408);
		is_timeout_ = true;
		return 2;
	}

	// ケース3: EPOLLOUTの設定の場合（requestは正常に処理できたが対応できない場合）504
	if(cgi_ != NULL) {
		delete cgi_;
		cgi_ = NULL;
	}
	std::cout << "[connection] gateway timeout - sending 504" << std::endl;
	setHttpResponse();
	response_->setStatusCode(504);
	setErrorFd(504);
	buildStaticFileResponse(504);
	is_timeout_ = true;
	return 2;
}

// ==================================== read and write ====================================

void Connection::cleanUp() {
	if(cgi_ != NULL && cgi_->getFd() != -1) {
		delete cgi_;
		cgi_ = NULL;
	}
}

// ==================================== utils ====================================
std::vector<char> stringToVector(std::string str) {
	std::vector<char> vec(str.begin(), str.end());
	return vec;
}

std::string vectorToString(std::vector<char> vec) {
	std::string str(vec.begin(), vec.end());
	return str;
}
