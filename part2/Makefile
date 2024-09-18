build:
	mkdir -p exe
	mkdir -p output
	g++ -g -o exe/server server.cpp
	g++ -g -o exe/client client.cpp

run:
	@./exe/server  & \
    sleep 1; \
    ./exe/client

plot:
	@./exe/server --plot &\
	sleep 1; \
	./exe/client --plot

clean:
	rm -rf exe
	rm output.txt
	rm plot.png