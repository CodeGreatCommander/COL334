build:
	@mkdir -p exe
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

run-cscd:
	@./exe/server csma & \
	sleep 1; \
	./exe/client csma

run:
	@./exe/server --all & \
	sleep 1; \
	./exe/client --all

server-aloha:
	@./exe/server aloha

server-beb:
	@./exe/server beb

server-cscd:
	@./exe/server csma

client-aloha:
	@./exe/client aloha

client-beb:
	@./exe/client beb


plot:
	@./exe/server --plot & \
	sleep 1; \
	./exe/client --plot
	@python3 analysis.py
	@rm -f temp.txt

clean:
	@rm -rf exe output