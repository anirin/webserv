#include "Connection.hpp"

void Connection::deleteFile()
{
	std::remove(request_->getLocationPath().c_str());
	std::cout << "delete file" << std::endl;
	std::string response_body = "<html>\r\n"
								"<head><title>200 OK</title></head>\r\n"
								"<body>\r\n"
								"<h1>200 OK</h1>\r\n"
								"<p>File deleted successfully</p>\r\n"
								"</body>\r\n"
								"</html>\r\n";
	wbuff_ = response_body;

	return ;
}
