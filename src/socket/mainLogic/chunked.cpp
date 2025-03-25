#include "Connection.hpp"

bool Connection::isChunked() {
	std::map<std::string, std::string> header = request_->getHeader();
	if(header.find("Transfer-Encoding") == header.end())
		return false;
	if(header["Transfer-Encoding"] == "chunked")
		return true;
	return false;
}

void Connection::setChunkedBody() {
	std::string chunked_body = request_->getBody();
	std::string parsed_body;

	while(true) {
		std::string::size_type crlf_pos = chunked_body.find("\r\n");
		if(crlf_pos == std::string::npos) {
			throw std::runtime_error("Invalid chunked body: No CRLF found for chunk size");
		}

		std::string chunked_size_str = chunked_body.substr(0, crlf_pos);
		if(chunked_size_str.empty()) {
			throw std::runtime_error("Invalid chunked body: Empty chunk size");
		}

		errno = 0;
		char* endptr = 0;
		long size = std::strtol(chunked_size_str.c_str(), &endptr, 16);

		if(endptr == chunked_size_str.c_str() || *endptr != '\0') {
			throw std::runtime_error("Invalid chunked body: Chunk size is not a valid hex number");
		}
		if(errno == ERANGE || size < 0) {
			std::cout << "[chunked] size: " << size << std::endl;
			throw std::runtime_error("Invalid chunked body: Chunk size out of range or negative");
		}

		chunked_body = chunked_body.substr(crlf_pos + 2);

		if(size == 0) {
			if(!chunked_body.empty() && chunked_body.substr(0, 2) != "\r\n") {
				throw std::runtime_error("Invalid chunked body: Missing final CRLF after chunk size 0");
			}
			break;
		}

		if(static_cast<std::string::size_type>(size) > chunked_body.size()) {
			throw std::runtime_error("Invalid chunked body: Chunk size exceeds remaining data");
		}

		parsed_body += chunked_body.substr(0, size);

		if(chunked_body.size() < static_cast<std::string::size_type>(size) + 2 ||
		   chunked_body.substr(size, 2) != "\r\n") {
			throw std::runtime_error("Invalid chunked body: Missing CRLF after chunk data");
		}
		chunked_body = chunked_body.substr(size + 2);
	}

	request_->setBody(parsed_body);
}