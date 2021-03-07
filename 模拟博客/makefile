.PHONY:blog_system main.cpp db.hpp
blog_system:main.cpp db.hpp
	g++ -std=c++11 $^ -o $@ -L/usr/lib64/mysql -lmysqlclient -ljsoncpp -lpthread
