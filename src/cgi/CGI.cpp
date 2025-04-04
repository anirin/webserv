#include "CGI.hpp"

// ==================================== constructor and destructor ====================================
CGI::CGI(const std::string& script_path, const std::string& query_string, const std::vector<char>& body,
		 const std::map<std::string, std::string> headers)
	: _script_path(script_path), _query_string(query_string), _fd(-1), _pid(-1), _body(body), _headers(headers) {}

CGI::CGI(const std::string& script_path, const std::string& query_string,
		 const std::map<std::string, std::string> headers)
	: _script_path(script_path), _query_string(query_string), _fd(-1), _pid(-1), _headers(headers) {}

CGI::~CGI() {
	// ファイルディスクリプタのクローズ
	if(_fd != -1) {
		close(_fd);
		_fd = -1;
	}

	// 子プロセスの終了処理
	if(_pid > 0) {
		if(waitpid(_pid, NULL, WNOHANG) == 0) {
			kill(_pid, SIGKILL);
			waitpid(_pid, NULL, 0);
		}
	}
}

// ==================================== getter ====================================

int CGI::getFd() const {
	return _fd;
}

int CGI::getPid() const {
	return _pid;
}

// ==================================== methods ====================================

void CGI::createPipe(int pipefd[2]) {
	if(pipe(pipefd) == -1)
		throw std::runtime_error("[cgi] pipe creation failed");
}

pid_t CGI::createChildProcess() {
	pid_t pid = fork();
	if(pid == -1)
		throw std::runtime_error("[cgi] fork failed");
	return pid;
}

void CGI::executeScriptInChild(int pipefd[2]) {
	close(pipefd[0]);
	dup2(pipefd[1], STDOUT_FILENO);
	close(pipefd[1]);

	// POSTメソッドの場合、標準入力にボディデータを設定
	bool isPost = !_body.empty();
	if(isPost) {
		int stdin_pipe[2];
		if(pipe(stdin_pipe) == -1)
			throw std::runtime_error("[cgi] stdin pipe failed");

		// 重要: POSTデータに改行を追加（fgetsが行単位で読むため）
		std::string body_str(reinterpret_cast<char*>(&_body[0]), _body.size());
		body_str += "\n";
		write(stdin_pipe[1], body_str.c_str(), body_str.length());
		close(stdin_pipe[1]);

		dup2(stdin_pipe[0], STDIN_FILENO);
		close(stdin_pipe[0]);
	}

	// 環境変数の設定 - 必要最小限のものだけに簡略化
	std::vector<std::string> env_strings;

	// メソッドによって異なる環境変数
	if(isPost) {
		env_strings.push_back("REQUEST_METHOD=POST");
		std::stringstream ss;
		ss << _body.size() + 1; // 改行を追加したので+1
		env_strings.push_back("CONTENT_LENGTH=" + ss.str());

		if(_headers.find("Content-Type") != _headers.end()) {
			env_strings.push_back("CONTENT_TYPE=" + _headers.at("Content-Type"));
		} else {
			env_strings.push_back("CONTENT_TYPE=application/x-www-form-urlencoded");
		}
	} else {
		env_strings.push_back("REQUEST_METHOD=GET");
		env_strings.push_back("CONTENT_LENGTH=0");
	}

	// 基本的な環境変数
	env_strings.push_back("SCRIPT_NAME=" + _script_path);
	env_strings.push_back("SCRIPT_FILENAME=" + _script_path);
	env_strings.push_back("QUERY_STRING=" + _query_string);

	// 環境変数配列の作成
	char** env = new char*[env_strings.size() + 1];
	for(size_t i = 0; i < env_strings.size(); i++) {
		// strdupはmallocを使うため、new[]を使った実装に置き換え
		env[i] = new char[env_strings[i].length() + 1];
		std::strcpy(env[i], env_strings[i].c_str());
	}
	env[env_strings.size()] = NULL;

	// PHPスクリプトの実行
	char* args[] = {(char*)"/usr/bin/test/php", (char*)_script_path.c_str(), NULL};
	execve(args[0], args, env);

	// execveが失敗した場合のクリーンアップ
	for(size_t i = 0; i < env_strings.size(); i++) {
		delete[] env[i]; // 配列なのでdelete[]を使用
	}
	delete[] env;

	throw std::runtime_error("[cgi] execve failed");
}

// readScriptOutput関数の名前を変更し、実装を改善
void CGI::setupOutputFd(int pipefd[2]) {
	close(pipefd[1]); // 書き込み用のパイプは閉じる

	// 読み取り用のパイプをノンブロッキングモードに設定
	int flags = fcntl(pipefd[0], F_GETFL, 0);
	if(flags == -1) {
		throw std::runtime_error("[cgi] fcntl get flags failed");
	}

	if(fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK) == -1) {
		throw std::runtime_error("[cgi] fcntl set nonblock failed");
	}

	// 読み取り用のfdをクラス変数に保存
	_fd = pipefd[0];
}

void CGI::execute() {
	int pipefd[2];

	try {
		createPipe(pipefd);
		_pid = createChildProcess();

		if(_pid == 0) {
			// 子プロセスでスクリプト実行
			executeScriptInChild(pipefd);
		} else {
			// 親プロセスでパイプFDを設定
			setupOutputFd(pipefd);
			// waitpidはここで行わない - epollで管理するため
		}
	} catch(const std::exception& e) {
		if(_pid == 0)
			_exit(1);
		throw std::runtime_error(std::string("[cgi] execute failed: ") + e.what());
	}
}