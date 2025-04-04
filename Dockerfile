FROM ubuntu:latest

RUN apt-get update && apt-get install -y \
    build-essential \
    siege \
    make \
    lsof \
    curl \
    php \
    valgrind && apt-get clean

CMD ["/bin/bash"]

# valgrind --leak-check=full ./webserv 
# siege http://127.0.0.1:8080/empty.html
# lsof -p $(pgrep webserv)
# curl http://127.0.0.1:8080/index.php
# docker exec -it ubuntu_siege_container /bin/bash