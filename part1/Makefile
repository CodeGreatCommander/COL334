build:
	mkdir -p exe
	g++ -g -o exe/server server.cpp
	g++ -g -o exe/client client.cpp

run:
	@./exe/server & \
    sleep 1; \
    ./exe/client

plot:
	@./exe/server --plot &\
	sleep 1; \
	./exe/client --plot
	@python3 analysis.py
	@rm temp.txt

clean:
	rm -rf exe
	rm -f output.txt
	rm -f plot.png