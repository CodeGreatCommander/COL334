build:
	mkdir -p exe
	mkdir -p output
	g++ -g -o exe/server server.cpp
	g++ -g -o exe/client client.cpp

run:
	@./exe/server  & \
    sleep 3; \
    ./exe/client