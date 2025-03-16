/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: atsu <atsu@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/13 11:25:14 by rmatsuba          #+#    #+#             */
/*   Updated: 2025/03/17 02:11:02 by atsu             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Connection.hpp"

std::time_t Connection::timeout_ = 100000;
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

void Connection::setCGI() {
	std::string location_path = request_->getLocationPath();
	int path_size = location_path.size();

	if(path_size > 4 && location_path.substr(path_size - 4, 4) == ".php") {
		CGI *cgi = new CGI(location_path);
		// todo エラーハンドリング
		cgi_ = cgi;
		std::cout << "[connection] cgi is set" << std::endl;
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
	readStaticFile(error_page);

	return;
}

void Connection::buildStaticFileResponse() {
	std::map<std::string, std::string> r_header = request_->getHeader();
	std::string path = request_->getLocationPath();
	std::string server_name = request_->getServerName();

	response_ = new HttpResponse();
	response_->setBody(wbuff_); // content length 格納のためにまずは body をセット
	// todo header のステータスの設定
	response_->setStartLine(200); // status code は request 段階で確定
	response_->setHeader(r_header, path, server_name);

	wbuff_ = response_->buildResponse();
	std::cout << "[connection] response build" << std::endl;
}

std::string Connection::buildAutoIndexContent(const std::string &path) {
	DIR *dir;
	struct dirent *entry;
	struct stat file_stat;
	std::stringstream html;

	// ディレクトリを開く
	dir = opendir(path.c_str());
	if(!dir) {
		std::cerr << "[connection] Cannot open directory: " << path << std::endl;
		return "Directory cannot be opened.";
	}

	// HTML生成
	html << "<html>\r\n"
		 << "<head><title>Index of " << path << "</title></head>\r\n"
		 << "<body>\r\n"
		 << "<h1>Index of " << path << "</h1>\r\n"
		 << "<hr>\r\n"
		 << "<pre>\r\n";

	// 親ディレクトリへのリンク
	html << "<a href=\"../\">../</a>\r\n";

	// ディレクトリ内の各ファイル/フォルダを処理
	while((entry = readdir(dir)) != NULL) {
		std::string name = entry->d_name;

		// "."は表示しない
		if(name == ".")
			continue;

		// ファイルパスを構築
		std::string full_path = path;
		if(path[path.length() - 1] != '/')
			full_path += "/";
		full_path += name;

		// ファイル情報を取得
		if(stat(full_path.c_str(), &file_stat) != 0)
			continue;

		// ディレクトリ判定
		bool is_dir = S_ISDIR(file_stat.st_mode);

		// 最終更新日時のフォーマット
		char time_buf[64];
		struct tm *tm_info = localtime(&file_stat.st_mtime);
		strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M", tm_info);

		// エントリ表示（名前、更新日時、サイズ）
		std::string display_name = name + (is_dir ? "/" : "");
		html << "<a href=\"" << name << (is_dir ? "/" : "") << "\">" << std::left << std::setw(50) << display_name
			 << "</a> " << time_buf << "  ";

		// サイズ表示（ディレクトリの場合は表示しない）
		if(!is_dir) {
			html << file_stat.st_size;
		}
		html << "\r\n";
	}

	html << "</pre>\r\n"
		 << "<hr>\r\n"
		 << "</body>\r\n"
		 << "</html>\r\n";

	closedir(dir);

	return html.str();
}

FileStatus Connection::buildRedirectResponse(const std::string& redirectPath) {
    std::cout << "[connection] building redirect response to: " << redirectPath << std::endl;
    
    // リクエストヘッダーを取得
    std::map<std::string, std::string> r_header = request_->getHeader();
    
    response_->setStartLine(302);
    
    // Locationヘッダーを追加
    r_header["Location"] = redirectPath;
	// r_header["Keep-Alive"] = "none";
	r_header["Connection"] = "close";
    
    // 空のbodyを設定
    std::stringstream body;
	body << "<html>\r\n"
		<< "<head><title>302 Found</title></head>\r\n"
		<< "<body>\r\n"
		<< "<center><h1>302 Found</h1></center>\r\n"
		<< "<hr><center>Redirect to: <a href=\"" 
		<< redirectPath << "\">" << redirectPath << "</a></center>\r\n"
		<< "</body>\r\n"
		<< "</html>\r\n";

	wbuff_ = body.str();
    response_->setBody(wbuff_);
    
    // ヘッダーの設定
	response_->setStartLine(302);
    response_->setHeader(r_header, "", request_->getServerName());
    
    // レスポンス全体を構築
    wbuff_ = response_->buildResponse();
    return SUCCESS_STATIC;
}

void Connection::setHttpRequest(MainConf *mainConf) {
	request_ = new HttpRequest(rbuff_, mainConf);
	std::cout << "[connection] debug : request " << request_ << std::endl;
	conf_value_ = mainConf->getConfValue(request_->getPort(), request_->getServerName(), request_->getRequestPath());
	std::cout << "[connection] request is set" << std::endl;
	// std::cout << rbuff_ << std::endl;

	// mainConf->debug_print_conf_value(conf_value_);
}

void Connection::setHttpResponse() {
	response_ = new HttpResponse();
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
	response_->setStatusCode(503);

	setErrorFd(503); // 503

	return true;
}

// ==================================== read and write ====================================

FileStatus Connection::processAfterReadCompleted(MainConf *mainConf) {
	setHttpRequest(mainConf);
	setHttpResponse();

	// config 設定
	std::string requestPath = request_->getRequestPath();
    std::string fsPath = getFilesystemPath(requestPath);

    // autoindex処理
    if (isAutoindexableDirectory(fsPath)) {
        std::cout << "[connection] autoindex is set" << std::endl;
        wbuff_ = buildAutoIndexContent(fsPath);
        buildStaticFileResponse();
        return SUCCESS_STATIC;
    }

	// redirect処理
	std::vector<std::string> redirect = conf_value_._return;
	if (!redirect.empty() && redirect.size() > 1) {
		std::string redirectPath = redirect[1];
		return buildRedirectResponse(redirectPath);
	}

	// error ハンドリング
	if(!request_->isValidRequest()) {
		std::cerr << "[connection] invalid request" << std::endl;
		// todo
		// error 400, 404, 405, 414, 505 はこの段階で確定する
		// responose_->setResponseStartLine("status code");
		// 前もって登録をしておく もしも error page
		// を読み込むなら、下のgetの処理と同様に
		// bodyを必要としないなら、headerを作成して write　の処理を呼び出
		setErrorFd(404); // 404 決め打ち テスト用
		buildStaticFileResponse();
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
			buildStaticFileResponse();
			return SUCCESS_STATIC;
		}
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

std::string Connection::getFilesystemPath(const std::string& requestPath) const {
    std::string rootPath = conf_value_._root; // ドキュメントルートのパス
    std::string cleanRequestPath = requestPath;
    std::string fsPath;

    // ドキュメントルートを設定
    fsPath = "." + rootPath;
    
    // ルートパスの末尾に/がないなら追加
    if (!fsPath.empty() && fsPath[fsPath.length() - 1] != '/')
        fsPath += '/';
    
    // リクエストパスの先頭の/を削除（もしあれば）
    if (!cleanRequestPath.empty() && cleanRequestPath[0] == '/')
        cleanRequestPath = cleanRequestPath.substr(1);
    
    // パスを組み合わせる
    fsPath += cleanRequestPath;
    
    std::cout << "[connection] Filesystem path resolved: " << fsPath << std::endl;
    return fsPath;
}

bool Connection::isAutoindexableDirectory(const std::string& fsPath) const {
    struct stat path_stat;
    
    // パスがディレクトリかつautoindexが有効か確認
    if (conf_value_._autoindex && 
        stat(fsPath.c_str(), &path_stat) == 0 && 
        S_ISDIR(path_stat.st_mode)) {
        return true;
    }
    return false;
}