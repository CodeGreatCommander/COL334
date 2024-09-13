build:
	@mkdir -p exe
	@mkdir -p output
	g++ -g -o exe/server server.cpp
	g++ -g -o exe/client client.cpp

run-aloha:
	@./exe/server aloha & \
    sleep 3; \
    ./exe/client aloha

run-beb:
	@./exe/server beb & \
	sleep 3; \
	./exe/client beb

run-csma:
	@./exe/server csma & \
	sleep 3; \
	./exe/client csma

clean:
	@rm -rf exe output