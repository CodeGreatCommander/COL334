build:
	mkdir -p exe
	g++ -g -o exe/server server.cpp
	g++ -g -o exe/client client.cpp

run-fifo:
	@./exe/server fifo  & \
    sleep 1; \
    ./exe/client fifo

run-rr:
	@./exe/server rr  & \
	sleep 1; \
	./exe/client rr

run:
	@./exe/server --all & \
	sleep 1; \
	./exe/client --all

plot:
	@./exe/server --plot & \
	sleep 1; \
	./exe/client --plot
	python3 analysis.py
	rm -rf temp.txt

fairness:
	@./exe/server --fairness & \
	sleep 1; \
	./exe/client --fairness