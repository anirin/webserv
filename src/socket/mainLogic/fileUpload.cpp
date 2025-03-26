#include "Connection.hpp"

// ファイル upload の処理
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

FileStatus Connection::fileUpload() {
	std::cout << "[connection] Processing file upload" << std::endl;

	std::map<std::string, std::string> headers = request_->getHeader();
	std::string content_type = headers["Content-Type"];
	std::string body = request_->getBody();

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
	size_t pos = body.find(boundary_delimiter);
	if(pos == std::string::npos) {
		std::cerr << "[connection] Invalid multipart format" << std::endl;
		setErrorFd(400);
		buildStaticFileResponse(400);
		return SUCCESS_STATIC;
	}

	// 最初のboundary以降を処理
	pos = body.find("\r\n", pos) + 2;
	size_t end_pos = body.find(boundary_delimiter, pos);

	if(end_pos == std::string::npos) {
		std::cerr << "[connection] Invalid multipart format - ending boundary not found" << std::endl;
		setErrorFd(400);
		buildStaticFileResponse(400);
		return SUCCESS_STATIC;
	}

	std::string part = body.substr(pos, end_pos - pos - 2); // -2 for \r\n

	// ヘッダー部分とコンテンツ部分を分離
	size_t headers_end = part.find("\r\n\r\n");
	if(headers_end == std::string::npos) {
		std::cerr << "[connection] Invalid part format" << std::endl;
		setErrorFd(400);
		buildStaticFileResponse(400);
		return SUCCESS_STATIC;
	}

	std::string part_headers = part.substr(0, headers_end);
	std::string part_content = part.substr(headers_end + 4); // +4 for \r\n\r\n

	// ファイル名を抽出
	std::string filename;
	size_t filename_pos = part_headers.find("filename=\"");
	if(filename_pos != std::string::npos) {
		size_t filename_end = part_headers.find("\"", filename_pos + 10);
		filename = part_headers.substr(filename_pos + 10, filename_end - (filename_pos + 10));
	} else {
		std::cerr << "[connection] Filename not found in part headers" << std::endl;
		setErrorFd(400);
		buildStaticFileResponse(400);
		return SUCCESS_STATIC;
	}

	// todo アップロードディレクトリを決定 決め内ダメ絶対
	std::string upload_dir = "./www/uploads/";
	// if(!conf_value_._upload_store.empty()) {
	// 	upload_dir = "." + conf_value_._root + conf_value_._upload_store;
	// 	// ディレクトリの末尾に/がなければ追加
	// 	if(upload_dir[upload_dir.length() - 1] != '/')
	// 		upload_dir += '/';
	// }

	// アップロードディレクトリが存在しない場合は作成
	struct stat st;
	if(stat(upload_dir.c_str(), &st) != 0) {
		if(mkdir(upload_dir.c_str(), 0755) != 0) {
			std::cerr << "[connection] Failed to create upload directory: " << upload_dir << std::endl;
			setErrorFd(500);
			buildStaticFileResponse(500);
			return SUCCESS_STATIC;
		}
	}

	// ファイル保存
	std::string file_path = upload_dir + filename;
	std::ofstream file(file_path.c_str(), std::ios::binary);
	if(!file) {
		std::cerr << "[connection] Failed to create file: " << file_path << std::endl;
		setErrorFd(500);
		buildStaticFileResponse(500);
		return SUCCESS_STATIC;
	}

	file.write(part_content.c_str(), part_content.size());
	file.close();

	std::cout << "[connection] File uploaded successfully: " << file_path << std::endl;

	// 成功レスポンスを構築
	std::stringstream response_body;
	response_body << "<html>\r\n"
				  << "<head><title>Upload Successful</title></head>\r\n"
				  << "<body>\r\n"
				  << "<h1>File Upload Successful</h1>\r\n"
				  << "<p>Uploaded file: " << filename << "</p>\r\n"
				  << "</body>\r\n"
				  << "</html>\r\n";

	wbuff_ = response_body.str();
	buildStaticFileResponse(201); // 201 Created

	return SUCCESS_STATIC;
}
