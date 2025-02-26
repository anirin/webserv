/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: atsu <atsu@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/13 11:25:14 by rmatsuba          #+#    #+#             */
/*   Updated: 2025/02/27 06:48:22 by atsu             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Connection.hpp"

std::time_t Connection::timeout_ = 1000;
ssize_t buff_size = 1024; // todo 持たせ方の検討

// ==================================== constructor and destructor ====================================

Connection::Connection() : ASocket() {}

Connection::Connection(int listenerFd) : ASocket() {
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
	static_fd_ = -1;
}

Connection::Connection(const Connection &other) : ASocket(other) {
	if(this == &other)
		return;
	fd_ = other.fd_;
	static_fd_ = other.static_fd_;
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

int Connection::getStaticFd() const {
	return static_fd_;
}

CGI *Connection::getCGI() const {
	return cgi_;
}
FileTypes Connection::getFdType(int fd) const {
	if(fd == static_fd_)
		return STATIC;
	else if(cgi_ != NULL && fd == cgi_->getFd())
		return PIPE;
	else
		return SOCKET;
}

// ==================================== setter ====================================

void Connection::setReadFd() {
	std::string location_path = request_->getLocationPath();
	int path_size = location_path.size();
	int fd;

	// todo cgi 呼び出し判定は後方一致でいいのか？
	if(path_size > 4 && location_path.substr(path_size - 4, 4) == ".php") {
		CGI *cgi = new CGI(location_path);
		// todo エラーハンドリング
		cgi_ = cgi;
		std::cout << "[connection] cgi is set" << std::endl;
		return;
	} else {
		fd = open(location_path.c_str(), O_RDONLY);
		if(fd == -1) {
			std::cerr << "[connection] open failed" << std::endl;
			throw std::runtime_error("[connection] open failed");
		}
		static_fd_ = fd;
		std::cout << "[connection] static_fd is set" << std::endl;
		return;
	}
}

void Connection::setErrorFd(int status_code) {
	if(conf_value_._error_page.empty()) {
		std::cerr << "[connection] error_page is not set" << std::endl;
		// todo page がない場合の処理
		throw std::runtime_error("[connection] error_page is not set");
	}

	std::string error_page;
	std::string page_path = conf_value_._error_page[status_code];
	if(page_path.empty()) {
		std::cerr << "[connection] status code error_page is not set" << std::endl;
	} else {
		error_page = "./www/" + page_path;
		// todo 本来は内部リダイレクト
	}

	std::cout << "[connection] error_page: " << error_page << " is set" << std::endl;
	int fd = open(error_page.c_str(), O_RDONLY);
	if(fd == -1) {
		std::cerr << "[connection] open failed" << std::endl;
		throw std::runtime_error("[connection] open failed");
	}
	static_fd_ = fd;

	return;
}

void Connection::buildStaticFileResponse() {
	std::map<std::string, std::string> r_header = request_->getHeader();
	std::string path = request_->getLocationPath();
	std::string server_name = request_->getServerName();

	response_ = new HttpResponse();
	response_->setBody(wbuff_);	  // content length 格納のためにまずは body をセット
	response_->setStartLine(200); // status code は request 段階で確定
	response_->setHeader(r_header, path, server_name);

	wbuff_ = response_->buildResponse();
	std::cout << "[connection] response build" << std::endl;
}

void Connection::setHttpRequest(MainConf *mainConf) {
	request_ = new HttpRequest(rbuff_, mainConf);
	conf_value_ = mainConf->getConfValue(request_->getPort(), request_->getServerName(), request_->getRequestPath());
	std::cout << "[connection] request is set" << std::endl;
	// std::cout << rbuff_ << std::endl;
}

void Connection::setHttpResponse() {
	response_ = new HttpResponse();
}

void Connection::clearValue() {
	// clear cgi
	rbuff_.clear();
	wbuff_.clear();
	delete request_; // 二重解放の可能性あり
	delete response_;
	request_ = NULL;
	response_ = NULL;
	lastActive_ = 0;
	timeout_ = 0;
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
	response_->setStatusCode(503);

	setErrorFd(503); // 503

	return true;
}

// ==================================== read and write ====================================

FileStatus Connection::processAfterReadCompleted(MainConf *mainConf) {
	setHttpRequest(mainConf);
	setHttpResponse();

	if(!request_->isValidRequest()) {
		std::cerr << "[connection] invalid request" << std::endl;
		// todo
		// error 400, 404, 405, 414, 505 はこの段階で確定する
		// responose_->setResponseStartLine("status code");
		// 前もって登録をしておく もしも error page
		// を読み込むなら、下のgetの処理と同様に
		// bodyを必要としないなら、headerを作成して write　の処理を呼び出
		setErrorFd(404); // 404 決め打ち テスト用

		return SUCCESS_STATIC;
	}

	Method method = request_->getMethod();
	if(method == GET) {
		setReadFd();
		// todo pipeの時にはcgiをセットして、writeを許可する
		if(cgi_ != NULL)
			return SUCCESS_CGI;
		return SUCCESS_STATIC;
	} else if(method == POST) {
		// todo
	} else if(method == DELETE) {
		// todo
	} else {
		// todo
	}

	return SUCCESS;
}

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

	return processAfterReadCompleted(mainConf);
}

FileStatus Connection::readStaticFile() {
	char buff[buff_size];
	ssize_t rlen = read(static_fd_, buff, buff_size);

	if(rlen < 0) {
		close(static_fd_);
		static_fd_ = -1;
		return ERROR;
	} else if(rlen == 0) {
		close(static_fd_);
		static_fd_ = -1;
		return SUCCESS;
	} else if(rlen == buff_size) {
		wbuff_ += buff;
		return NOT_COMPLETED;
	}

	buff[rlen] = '\0';
	wbuff_ += buff;

	buildStaticFileResponse();

	close(static_fd_);
	static_fd_ = -1;

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
	if(static_fd_ != -1) {
		close(static_fd_);
		static_fd_ = -1;
	} else if(cgi_ != NULL && cgi_->getFd() != -1) {
		cgi_->killCGI();
		delete cgi_;
		cgi_ = NULL;
	}
}

// ==================================== utils ====================================

/* Convert vector to string */
std::string vecToString(std::vector<std::string> vec) {
	std::string str;
	for(std::vector<std::string>::iterator it = vec.begin(); it != vec.end(); ++it) {
		str += *it;
		str += " ";
	}
	return str;
}

/* Convert map to string */
std::string mapToString(std::map<std::string, std::string> mapdata) {
	std::string str;
	for(std::map<std::string, std::string>::iterator it = mapdata.begin(); it != mapdata.end(); ++it) {
		str += it->first;
		str += ": ";
		str += it->second;
		str += "\r\n";
	}
	return str;
}
