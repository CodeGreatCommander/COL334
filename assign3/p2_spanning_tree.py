from ryu.base.app_manager import RyuApp
from ryu.controller import ofp_event
from ryu.controller.handler import MAIN_DISPATCHER
from ryu.controller.handler import set_ev_cls
from ryu.ofproto import ofproto_v1_0
from ryu.lib.mac import haddr_to_bin
from ryu.lib.packet import packet
from ryu.lib.packet import ethernet
from ryu.lib.packet import ether_types
from ryu.topology import event, switches
from ryu.topology.api import get_switch, get_link

from collections import deque

def get_tree(switches, adj):
    # BFS tree
    visited = set()
    tree_edges = set()
    blocked_ports = {switch:set() for switch in switches}
    for switch in switches:
        if switch in visited: continue
        visited.add(switch)
        queue = deque([switch])
        while len(queue):
            node = queue.popleft()
            for neighbor in adj[node]:
                if neighbor[0] not in visited:
                    visited.add(neighbor[0])
                    queue.append(neighbor[0])
                    tree_edges.add(((node,neighbor[1]),(neighbor[0],neighbor[2])))
                else:
                    pair = ((node,neighbor[1]),(neighbor[0],neighbor[2]))
                    if pair not in tree_edges and pair[::-1] not in tree_edges:
                        blocked_ports[node].add(neighbor[1])
                        blocked_ports[neighbor[0]].add(neighbor[2])
    return blocked_ports


    

class SpanningTree(RyuApp):
    OFP_VERSIONS = [ofproto_v1_0.OFP_VERSION]

    def __init__(self, *args, **kwargs):
        super(SpanningTree, self).__init__(*args, **kwargs)
        self.change = False
        self.mac_to_port = {}
        self.blocked_ports = {}
        self.sw_dpids = set()
        self.adj = {}
    
    #Self Learning Switch

    def add_flow(self, datapath, in_port, dst, src, actions):
        ofproto = datapath.ofproto

        match = datapath.ofproto_parser.OFPMatch(
            in_port=in_port,
            dl_dst=dst, dl_src=src)

        mod = datapath.ofproto_parser.OFPFlowMod(
            datapath=datapath, match=match, cookie=0,
            command=ofproto.OFPFC_ADD, idle_timeout=0, hard_timeout=0,
            priority=ofproto.OFP_DEFAULT_PRIORITY,
            flags=ofproto.OFPFF_SEND_FLOW_REM, actions=actions)
        datapath.send_msg(mod)


    @set_ev_cls(ofp_event.EventOFPPacketIn, MAIN_DISPATCHER)
    def _packet_in_handler(self, ev):
        if(self.change):
            self.handle_topo_change()
            self.change = False
        msg = ev.msg
        datapath = msg.datapath
        ofproto = datapath.ofproto
        parser = datapath.ofproto_parser

        in_port = msg.in_port

        pkt = packet.Packet(msg.data)
        eth = pkt.get_protocol(ethernet.ethernet)
        if eth.ethertype == ether_types.ETH_TYPE_LLDP:
            return
        dst = eth.dst
        src = eth.src

        dpid = datapath.id
        self.mac_to_port.setdefault(dpid, {})

        self.logger.info("packet in %s %s %s %s", dpid, src, dst, in_port)

        self.mac_to_port[dpid][src] = in_port

        if dst in self.mac_to_port[dpid]:
            actions = [parser.OFPActionOutput(self.mac_to_port[dpid][dst])]
            self.add_flow(datapath, in_port, dst, src, actions)
        else:
            actions = []
            for port_no, port in datapath.ports.items():
                if port_no not in self.blocked_ports[dpid] and port_no != in_port:
                    actions.append(parser.OFPActionOutput(port_no))


        data = None
        if msg.buffer_id == ofproto.OFP_NO_BUFFER:
            data = msg.data

        out = parser.OFPPacketOut(
            datapath=datapath, buffer_id=msg.buffer_id, in_port=in_port,
            actions=actions, data=data)
        datapath.send_msg(out)
    #Topology change detection

    @set_ev_cls(event.EventSwitchEnter)
    def add_switch(self,ev):
        self.sw_dpids.add(ev.switch.dp.id)
        self.adj[ev.switch.dp.id] = set()
        self.change = True

    @set_ev_cls(event.EventSwitchLeave)
    def remove_switch(self,ev):
        removed_switch = ev.switch.dp.id
        self.sw_dpids.remove(removed_switch)
        del self.adj[removed_switch]
        for switch in self.adj:
            self.adj[switch].discard(removed_switch)
        self.change = True


    @set_ev_cls(event.EventLinkAdd)
    def link_add(self,ev):
        self.adj[ev.link.src.dpid].add((ev.link.dst.dpid, ev.link.src.port_no, ev.link.dst.port_no))
        self.adj[ev.link.dst.dpid].add((ev.link.src.dpid, ev.link.dst.port_no, ev.link.src.port_no))
        self.change = True
        
    @set_ev_cls(event.EventLinkDelete)
    def link_delete(self,ev):
        self.adj[ev.link.src.dpid].discard((ev.link.dst.dpid, ev.link.src.port_no, ev.link.dst.port_no))
        self.adj[ev.link.dst.dpid].discard((ev.link.src.dpid, ev.link.dst.port_no, ev.link.src.port_no))
        self.change = True
    
    def handle_topo_change(self):
        self.logger.info("Topology changed")
        self.logger.info(self.sw_dpids)
        self.logger.info(self.adj)
        self.mac_to_port.clear()
        self.blocked_ports = get_tree(self.sw_dpids, self.adj)
        self.logger.info(self.blocked_ports)          

            
