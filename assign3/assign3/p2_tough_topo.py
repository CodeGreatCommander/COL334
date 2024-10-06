from mininet.topo import Topo
from mininet.net import Mininet
from mininet.node import RemoteController
from mininet.cli import CLI
from mininet.log import setLogLevel

class LargeCycleTopo(Topo):
    def build(self):
        # Add switches
        switches = [self.addSwitch(f's{i}') for i in range(1, 11)]

        # Add hosts and connect them to switches
        for i in range(1, 101):
            host = self.addHost(f'h{i}')
            switch = switches[(i - 1) % 10]
            self.addLink(host, switch)

        # Create a cycle among switches
        for i in range(10):
            self.addLink(switches[i], switches[(i + 1) % 10])
            self.addLink(switches[i], switches[(i + 2) % 10])

def run():
    topo = LargeCycleTopo()
    net = Mininet(topo=topo, controller=RemoteController)
    net.start()
    CLI(net)
    net.stop()

if __name__ == '__main__':
    setLogLevel('info')
    run()