part1:
	sudo python3 p1_exp.py LOSS
	sudo python3 p1_exp.py DELAY

part2:
	sudo python3 p2_exp.py LOSS
	sudo python3 p2_exp.py DELAY
	sudo python3 p2_exp_fairness.py

part3:
	sudo python3 p3_exp.py LOSS
	sudo python3 p3_exp.py DELAY
	sudo python3 p3_exp_fairness.py

start_controller:
	ryu run ryu.app.simple_switch

clean:
	rm -f *.txt *.csv *.png