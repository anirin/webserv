#include "Connection.hpp"

FileStatus Connection::readCGI() {
	ssize_t buff_size = 1024; // todo 持たせ方の検討

	char buff[buff_size];
	std::cout << "[read cgi] started to read CGI" << std::endl;
	ssize_t rlen = read(cgi_->getFd(), buff, sizeof(buff) - 1);

	if(rlen < 0) {
		std::cerr << "[read cgi] read pipe failed" << std::endl;
		delete cgi_;
		cgi_ = NULL;
		return ERROR;
	}

	// データを読み取れた場合はバッファに追加
	std::cout << "[read cgi] read CGI: " << rlen << std::endl;
	std::cout << "[read cgi] read CGI: " << rlen << std::endl;
	if(rlen > 0) {
		for(ssize_t i = 0; i < rlen; i++) {
			wbuff_.push_back(buff[i]);
		}
	}

	// CGIプロセスの状態を確認
	int status;
	pid_t result = waitpid(cgi_->getPid(), &status, WNOHANG);

	if(result == 0) {
		// プロセスがまだ実行中
		std::cout << "[read cgi] CGI process still running" << std::endl;
		return NOT_COMPLETED;
	} else if(result < 0) {
		// エラー
		std::cerr << "[read cgi] waitpid error: " << strerror(errno) << std::endl;
		delete cgi_;
		cgi_ = NULL;
		return ERROR;
	}

	// プロセスが終了した場合
	std::cout << "[read cgi] CGI process completed" << std::endl;

	// 残りのデータがあれば読み取る
	while((rlen = read(cgi_->getFd(), buff, sizeof(buff) - 1)) > 0) {
		for(ssize_t i = 0; i < rlen; i++) {
			wbuff_.push_back(buff[i]);
		}
	}

	std::cout << "[read cgi] wbuff size: " << wbuff_.size() << std::endl;
	if(!wbuff_.empty()) {
		std::cout << "[read cgi] wbuff preview: "
				  << std::string(wbuff_.begin(), wbuff_.begin() + std::min(wbuff_.size(), size_t(50))) << "..."
				  << std::endl;
	}

	buildStaticFileResponse(200);
	std::cout << "[read cgi] read CGI completed" << std::endl;
	return SUCCESS;
}
