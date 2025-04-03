#include "Connection.hpp"

FileStatus Connection::writeSocket() {
	ssize_t buff_size = 1024; // todo 持たせ方の検討

	char buff[buff_size];

	/* if(!request_) { */
	/* 	std::cerr << "[connection] No request found" << std::endl; */
	/* 	return ERROR; */
	/* } */

	if(wbuff_.empty()) {
		return NOT_COMPLETED;
	}

	// std::cout << " >>>>> " << wbuff_.data() << " <<<<< " << std::endl; // デバッグ用

	ssize_t copy_len = std::min(wbuff_.size(), static_cast<std::size_t>(buff_size));
	std::memcpy(buff, wbuff_.data(), copy_len);
	if(copy_len != buff_size)
		buff[copy_len] = '\0';
	wbuff_.erase(wbuff_.begin(), wbuff_.begin() + copy_len);
	ssize_t wlen = send(fd_, buff, copy_len, 0);
	if(wlen == -1)
		return ERROR;
	if(wlen == buff_size)
		return NOT_COMPLETED;
	delete response_;
	if(request_)
		delete request_;
	response_ = NULL;
	request_ = NULL;
	std::cout << "[connection] write socket completed" << std::endl;
	return SUCCESS;
}
