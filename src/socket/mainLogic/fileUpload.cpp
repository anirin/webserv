#include <algorithm>

#include "Connection.hpp"

// ベクターから文字列パターンを検索する関数
size_t findInVector(const std::vector<char>& data, const std::string& pattern, size_t start_pos = 0) {
	if(start_pos >= data.size() || pattern.empty())
		return std::string::npos;

	for(size_t i = start_pos; i <= data.size() - pattern.size(); ++i) {
		bool found = true;
		for(size_t j = 0; j < pattern.size(); ++j) {
			if(data[i + j] != pattern[j]) {
				found = false;
				break;
			}
		}
		if(found)
			return i;
	}

	return std::string::npos;
}

// ベクターから文字列を抽出する関数
std::string extractString(const std::vector<char>& data, size_t start, size_t length) {
	std::string result;
	for(size_t i = 0; i < length && start + i < data.size(); ++i) {
		result += data[start + i];
	}
	return result;
}

// マルチパートヘッダーからファイル情報を抽出する関数
bool extractFileInfo(const std::string& part_headers, std::string& filename, std::string& file_content_type) {
	// ファイル名を抽出
	size_t filename_pos = part_headers.find("filename=\"");
	if(filename_pos != std::string::npos) {
		size_t filename_end = part_headers.find("\"", filename_pos + 10);
		filename = part_headers.substr(filename_pos + 10, filename_end - (filename_pos + 10));
	} else {
		return false;
	}

	// Content-Type を抽出
	file_content_type = "application/octet-stream"; // デフォルト
	size_t content_type_pos = part_headers.find("Content-Type:");
	if(content_type_pos != std::string::npos) {
		size_t content_type_start = part_headers.find_first_not_of(" \t", content_type_pos + 13);
		size_t content_type_end = part_headers.find("\r\n", content_type_start);
		if(content_type_end != std::string::npos) {
			file_content_type = part_headers.substr(content_type_start, content_type_end - content_type_start);
		}
	}
	return true;
}

// アップロードファイルを保存する関数
bool saveUploadFile(const std::vector<char>& body, size_t start, size_t size, const std::string& upload_dir,
					const std::string& filename) {
	struct stat st;
	// アップロードディレクトリが存在しない場合は作成
	if(stat(upload_dir.c_str(), &st) != 0) {
		if(mkdir(upload_dir.c_str(), 0755) != 0) {
			return false;
		}
	}

	// ファイル保存
	std::string file_path = upload_dir + filename;
	std::ofstream file(file_path.c_str(), std::ios::binary);
	if(!file) {
		return false;
	}

	file.write(&body[start], size);
	file.close();
	return true;
}

// レスポンスHTMLを作成する関数
std::string createResponseHtml(const std::string& filename, const std::string& file_content_type) {
	std::stringstream response_body;
	response_body << "<html>\r\n"
				  << "<head><title>Upload Successful</title></head>\r\n"
				  << "<body>\r\n"
				  << "<h1>File Upload Successful</h1>\r\n"
				  << "<p>Uploaded file: " << filename << " (" << file_content_type << ")</p>\r\n";
	response_body << "</body>\r\n"
				  << "</html>\r\n";

	return response_body.str();
}

// ファイル upload かどうか判定する関数
bool Connection::isFileUpload() {
	if(!request_) {
		std::cerr << "[connection] No request available" << std::endl;
		return false;
	}

	Method method = request_->getMethod();
	std::map<std::string, std::string> headers = request_->getHeader();
	std::string content_type = headers["Content-Type"];

	if(method == POST && content_type.find("multipart/form-data") != std::string::npos) {
		std::cout << "[connection] Detected file upload request" << std::endl;
		return true;
	} else {
		std::cout << "[connection] Not a file upload request" << std::endl;
		return false;
	}
}

// ファイルアップロードの処理
FileStatus Connection::fileUpload() {
	std::cout << "[connection] Processing file upload" << std::endl;

	std::map<std::string, std::string> headers = request_->getHeader();
	std::string content_type = headers["Content-Type"];
	std::vector<char> body = request_->getBody();

	// boundaryを抽出
	std::string boundary;
	size_t boundary_pos = content_type.find("boundary=");
	if(boundary_pos != std::string::npos) {
		boundary = content_type.substr(boundary_pos + 9);
	} else {
		std::cerr << "[connection] Boundary not found in Content-Type" << std::endl;
		setErrorFd(400);
		buildStaticFileResponse(400);
		return SUCCESS_STATIC;
	}

	// 完全なboundary文字列を作成
	std::string boundary_delimiter = "--" + boundary;

	// bodyをboundaryで分割
	size_t pos = findInVector(body, boundary_delimiter);
	if(pos == std::string::npos) {
		std::cerr << "[connection] Invalid multipart format" << std::endl;
		setErrorFd(400);
		buildStaticFileResponse(400);
		return SUCCESS_STATIC;
	}

	// 最初のboundary以降を処理
	size_t header_start_pos = findInVector(body, "\r\n", pos) + 2;
	if(header_start_pos == std::string::npos || header_start_pos + 2 > body.size()) {
		std::cerr << "[connection] Invalid multipart format - header not found" << std::endl;
		setErrorFd(400);
		buildStaticFileResponse(400);
		return SUCCESS_STATIC;
	}

	// 終了境界を検索
	size_t end_pos = findInVector(body, boundary_delimiter, header_start_pos);
	if(end_pos == std::string::npos) {
		std::cerr << "[connection] Invalid multipart format - ending boundary not found" << std::endl;
		setErrorFd(400);
		buildStaticFileResponse(400);
		return SUCCESS_STATIC;
	}

	// ヘッダー部分を抽出
	size_t headers_end = findInVector(body, "\r\n\r\n", header_start_pos);
	if(headers_end == std::string::npos) {
		std::cerr << "[connection] Invalid part format" << std::endl;
		setErrorFd(400);
		buildStaticFileResponse(400);
		return SUCCESS_STATIC;
	}

	// ヘッダー文字列を作成
	std::string part_headers = extractString(body, header_start_pos, headers_end - header_start_pos);

	// コンテンツの開始位置とサイズを計算
	size_t content_start = headers_end + 4;			   // +4 for \r\n\r\n
	size_t content_size = end_pos - content_start - 2; // -2 for \r\n before boundary

	// ファイル情報を抽出
	std::string filename;
	std::string file_content_type;
	if(!extractFileInfo(part_headers, filename, file_content_type)) {
		std::cerr << "[connection] Filename not found in part headers" << std::endl;
		setErrorFd(400);
		buildStaticFileResponse(400);
		return SUCCESS_STATIC;
	}

	std::cout << "[connection] File content type: " << file_content_type << std::endl;

	// todo
	// アップロードディレクトリを決定
	std::string upload_dir = "./www/uploads/";

	// ファイル保存
	if(!saveUploadFile(body, content_start, content_size, upload_dir, filename)) {
		std::cerr << "[connection] Failed to save file" << std::endl;
		setErrorFd(500);
		buildStaticFileResponse(500);
		return SUCCESS_STATIC;
	}

	std::string file_path = upload_dir + filename;
	std::cout << "[connection] File uploaded successfully: " << file_path << std::endl;

	// 成功レスポンスを構築
	std::string content = createResponseHtml(filename, file_content_type);
	wbuff_ = stringToVector(content);
	buildStaticFileResponse(201); // 201 Created

	return SUCCESS_STATIC;
}
