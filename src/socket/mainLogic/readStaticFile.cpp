#include "Connection.hpp"

FileStatus Connection::readStaticFile(std::string file_path) {
	struct stat path_stat;
	if(stat(file_path.c_str(), &path_stat) == 0) {
		if(S_ISDIR(path_stat.st_mode)) {
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
