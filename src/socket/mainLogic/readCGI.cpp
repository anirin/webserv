#include "Connection.hpp"

FileStatus Connection::readCGI() {
	ssize_t buff_size = 1024; // todo 持たせ方の検討

	char buff[buff_size];
	std::cout << "[read cgi] started to read CGI" << std::endl;
	ssize_t rlen = read(cgi_->getFd(), buff, sizeof(buff) - 1);

	if(rlen < 0) {
		std::cerr << "[read cgi] read pipe failed" << std::endl;
		return ERROR;
	}

	// データを読み取れた場合はバッファに追加
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
		std::cerr << "[read cgi] waitpid error" << std::endl;
		return ERROR;
	}

	// プロセスが終了した場合
	std::cout << "[read cgi] CGI process completed" << std::endl;

	// プロセスの終了ステータスを確認
	bool hasError = false;
	if(WIFEXITED(status)) {
		int exitStatus = WEXITSTATUS(status);
		std::cout << "[read cgi] exit status: " << exitStatus << std::endl;
		if(exitStatus != 0) {
			std::cerr << "[read cgi] CGI process exited with non-zero status: " << exitStatus << std::endl;
			hasError = true;
		}
	} else if(WIFSIGNALED(status)) {
		std::cerr << "[read cgi] CGI process terminated by signal: " << WTERMSIG(status) << std::endl;
		hasError = true;
	}

	// 残りのデータがあれば読み取る
	while((rlen = read(cgi_->getFd(), buff, sizeof(buff) - 1)) > 0) {
		for(ssize_t i = 0; i < rlen; i++) {
			wbuff_.push_back(buff[i]);
		}
	}

	if(rlen < 0) {
		std::cerr << "[read cgi] read pipe error" << std::endl;
		return ERROR;
	}

	// エラーが発生した場合は500エラーを返す
	if(hasError) {
		std::cerr << "[read cgi] CGI execution failed" << std::endl;
		setErrorFd(500);
		buildStaticFileResponse(500);
	} else {
		buildStaticFileResponse(200);
	}

	std::cout << "[read cgi] read CGI completed" << std::endl;
	return SUCCESS;
}
