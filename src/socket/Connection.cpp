/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: atsu <atsu@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/13 11:25:14 by rmatsuba          #+#    #+#             */
/*   Updated: 2025/04/01 13:27:09 by atsu             ###   ########.fr       */
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
		throw std::runtime_error(e.what());
	}

	try {
		conf_value_ =
			mainConf->getConfValue(request_->getPort(), request_->getServerName(), request_->getRequestPath());
	} catch(const std::exception &e) {
		throw std::runtime_error(e.what());
	}
	std::cout << "[connection] request is set" << std::endl;
	// std::cout << rbuff_ << std::endl;

	// mainConf->debug_print_conf_value(conf_value_);
}

void Connection::setHttpResponse() {
	response_ = new HttpResponse();
}

void Connection::setCGI() { // throw
	std::string location_path = request_->getLocationPath();
	int path_size = location_path.size();

	if(path_size > 4 && location_path.substr(path_size - 4, 4) == ".php") {
		CGI *cgi = NULL;

		Method method = request_->getMethod();
		if(method == POST) {
			std::vector<char> body_content = request_->getBody();
			std::string body = vectorToString(body_content);
			std::map<std::string, std::string> headers = request_->getHeader();
			cgi = new CGI(location_path, body, headers);
		} else {
			cgi = new CGI(location_path);
		}

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
	delete response_;
	delete request_;
	response_ = NULL;
	request_ = NULL;

	rbuff_.clear();
	wbuff_.clear();
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

FileStatus Connection::readSocket(MainConf *mainConf) {
	std::cout << "[connection] started to read socket" << std::endl;
	char buff[buff_size];
	ssize_t rlen = recv(fd_, buff, buff_size, 0);

	if(rlen < 0) {
		return ERROR;
	} else if(rlen == 0) {
		std::cout << "[connection] read socket closed by client" << std::endl;
		return CLOSED;
	} else if(rlen == buff_size) {
		for(ssize_t i = 0; i < buff_size; i++) {
			rbuff_.push_back(buff[i]);
		}
		return NOT_COMPLETED;
	}

	for(ssize_t i = 0; i < rlen; i++) {
		rbuff_.push_back(buff[i]);
	}

	return processAfterReadCompleted(mainConf);
}

FileStatus Connection::readStaticFile(std::string file_path) {
	struct stat path_stat;
	if (stat(file_path.c_str(), &path_stat) == 0) {
		if (S_ISDIR(path_stat.st_mode)) {
			std::cerr << "[connection] path is a directory" << std::endl;
			setErrorFd(404);
			buildStaticFileResponse(404);
			return SUCCESS_STATIC;
		}
	}

	std::ifstream ifs(file_path.c_str(), std::ios::binary); // バイナリモード推奨
	if(!ifs) {
		std::cerr << "[connection] open file failed" << std::endl;
		setErrorFd(500);
		buildStaticFileResponse(500);
		return SUCCESS_STATIC;
	}

	// ファイル全体を一度に読み込み
	wbuff_.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());

	if(ifs.bad()) {
		std::cerr << "[connection] read error" << std::endl;
		setErrorFd(500);
		buildStaticFileResponse(500);
		return SUCCESS_STATIC;
	}

	ifs.close();
	buildStaticFileResponse(200);

	return SUCCESS_STATIC;
}

FileStatus Connection::readCGI() {
	char buff[buff_size];
	ssize_t rlen = read(cgi_->getFd(), buff, sizeof(buff) - 1);

	if(rlen < 0) {
		std::cerr << "[connection] read pipe failed" << std::endl;
		cgi_->killCGI();
		return ERROR;
	} else if(rlen == 1023) {
		for(ssize_t i = 0; i < rlen; i++) {
			wbuff_.push_back(buff[i]);
		}
		return NOT_COMPLETED;
	}

	buff[rlen] = '\0';
	for(ssize_t i = 0; i < rlen; i++) {
		wbuff_.push_back(buff[i]);
	}

	close(cgi_->getFd());
	delete cgi_;
	cgi_ = NULL;

	return SUCCESS;
}

FileStatus Connection::writeSocket() {
	char buff[buff_size];

	std::cout << "reaching writeSocket" << std::endl;
	/* if(!request_) { */
	/* 	std::cerr << "[connection] No request found" << std::endl; */
	/* 	return ERROR; */
	/* } */

	if(wbuff_.empty()) {
		return NOT_COMPLETED;
	}

	// std::cout << wbuff_ << std::endl; // デバッグ用

	ssize_t copy_len = std::min(wbuff_.size(), static_cast<std::size_t>(buff_size));
	std::memcpy(buff, wbuff_.data(), copy_len);
	if(copy_len != buff_size)
		buff[copy_len] = '\0';
	wbuff_.erase(wbuff_.begin(), wbuff_.begin() + copy_len);
	ssize_t wlen = send(fd_, buff, copy_len, 0);
	if(wlen == -1)
		return ERROR;
	if(wlen == buff_size)
		return NOT_COMPLETED;

	delete response_;
	if(request_)
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

// ==================================== utils ====================================
std::vector<char> stringToVector(std::string str) {
	std::vector<char> vec(str.begin(), str.end());
	return vec;
}

std::string vectorToString(std::vector<char> vec) {
	std::string str(vec.begin(), vec.end());
	return str;
}
