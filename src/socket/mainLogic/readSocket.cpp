#include "Connection.hpp"

FileStatus Connection::readSocket(MainConf* mainConf) {
	ssize_t buff_size = 1024; // todo 持たせ方の検討

	std::cout << "[read socket] started to read socket" << std::endl;
	char buff[buff_size];
	ssize_t rlen = recv(fd_, buff, buff_size, 0);

	if(rlen < 0) {
		return ERROR;
	} else if(rlen == 0) {
		std::cout << "[read socket] read socket closed by client" << std::endl;
		return CLOSED;
	} else if(rlen == buff_size) {
		rbuff_.insert(rbuff_.end(), buff, buff + rlen);
		return NOT_COMPLETED;
	}

	rbuff_.insert(rbuff_.end(), buff, buff + rlen);

	// start line と header だけ読み取ろうと試みる
	// parseできなければ not completed
	HttpRequest request = HttpRequest();
	std::map<std::string, std::string> headers;
	try {
		headers = request.parseRequestHeader(rbuff_);
	} catch(const std::exception& e) {
		std::cerr << "[read socket] Failed to parse request: " << e.what() << std::endl;
		return NOT_COMPLETED;
	}

	// case1: content length がある場合
	if(headers.find("Content-Length") != headers.end()) {
		std::istringstream iss(headers["Content-Length"]);
		size_t content_length;
		iss >> content_length;
		std::cout << "[read socket] content length: " << content_length << std::endl;

		// header より下の部分にある文字数をカウント
		if(getBodyLength(rbuff_) < content_length) {
			std::cout << "[read socket] not completed" << std::endl;
			return NOT_COMPLETED;
		}
		std::cout << "[read socket] completed" << std::endl;
	}

	// case2: chunked transfer encoding の場合
	if(headers.find("Transfer-Encoding") != headers.end() &&
	   headers["Transfer-Encoding"].find("chunked") != std::string::npos) {
		std::cout << "[read socket] chunked transfer encoding" << std::endl;

		if(!hasFinalChunk(rbuff_)) {
			std::cout << "[read socket] not completed (no final chunk)" << std::endl;
			return NOT_COMPLETED;
		}
		std::cout << "[read socket] completed (chunked)" << std::endl;
	}

	std::cout << "rbuff_ size: " << rbuff_.size() << std::endl;
	std::cout << "rbuff_: [[ " << std::string(rbuff_.begin(), rbuff_.end()) << "]]" << std::endl;

	return processAfterReadCompleted(mainConf);
}

// ボディ部分の長さを計算する関数
size_t Connection::getBodyLength(const std::vector<char>& buffer) {
	std::string buffer_str(buffer.begin(), buffer.end());

	// ヘッダーとボディの区切り（空行）を探す
	size_t header_end = buffer_str.find("\r\n\r\n");
	if(header_end == std::string::npos) {
		// 区切りが見つからない場合はボディなし
		return 0;
	}

	// ヘッダー終了位置 + 4(\r\n\r\n分)からがボディ
	size_t body_start = header_end + 4;

	// ボディ部分の長さを返す
	return buffer.size() - body_start;
}

// chunkedエンコーディングの最終チャンクが存在するか確認する関数
bool Connection::hasFinalChunk(const std::vector<char>& buffer) {
	std::string buffer_str(buffer.begin(), buffer.end());

	// ヘッダー部分の終了位置を特定
	size_t header_end = buffer_str.find("\r\n\r\n");
	if(header_end == std::string::npos) {
		return false;
	}

	size_t pos = header_end + 4; // ボディの開始位置

	// チャンクを順番に処理
	while(pos < buffer_str.size()) {
		// チャンクサイズ行を探す
		size_t chunk_size_end = buffer_str.find("\r\n", pos);
		if(chunk_size_end == std::string::npos) {
			return false; // チャンクサイズ行が完全ではない
		}

		// チャンクサイズを16進数として解析
		std::string chunk_size_str = buffer_str.substr(pos, chunk_size_end - pos);
		std::istringstream chunk_iss(chunk_size_str);
		size_t chunk_size;
		chunk_iss >> std::hex >> chunk_size;

		// チャンクサイズが0ならすべてのデータを読み終わった
		if(chunk_size == 0) {
			// 最後のチャンクが完全かチェック (0\r\n\r\n)
			size_t final_end = chunk_size_end + 4; // 0\r\n\r\n
			if(final_end > buffer_str.size()) {
				return false; // 最終チャンクが不完全
			}
			return true;
		}

		// チャンクデータの終わりの位置を計算
		size_t chunk_data_start = chunk_size_end + 2;			   // \r\nの後
		size_t chunk_data_end = chunk_data_start + chunk_size + 2; // データ + \r\n

		// バッファが不足している場合
		if(chunk_data_end > buffer_str.size()) {
			return false;
		}

		// 次のチャンクの開始位置に移動
		pos = chunk_data_end;
	}

	return false; // 最終チャンクが見つからなかった
}
