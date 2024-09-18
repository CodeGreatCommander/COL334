build:
	@mkdir -p exe
	@mkdir -p output
	g++ -g -o exe/server server.cpp
	g++ -g -o exe/client client.cpp

run-aloha:
	@./exe/server aloha & \
    sleep 1; \
    ./exe/client aloha

run-beb:
	@./exe/server beb & \
	sleep 1; \
	./exe/client beb

run-csma:
	@./exe/server csma & \
	sleep 1; \
	./exe/client csma

run:
	@./exe/server --all & \
	sleep 1; \
	./exe/client --all

plot:
	@./exe/server --plot & \
	sleep 1; \
	./exe/client --plot

clean:
	@rm -rf exe output