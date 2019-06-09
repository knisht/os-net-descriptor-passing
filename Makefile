all: server client
	

server: src/server.cpp src/rwutils.cpp src/rwutils.h
	g++ -g -DGLIBCXX_DEBUG -fsanitize=address,undefined $^ -o $@

client: src/client.cpp src/rwutils.h src/rwutils.cpp
	g++ -fsanitize=address,undefined $^ -o $@
	
clean: 
	rm -rf server client



