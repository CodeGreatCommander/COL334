from ryu.base.app_manager import RyuApp
from ryu.controller import ofp_event
from ryu.controller.handler import MAIN_DISPATCHER
from ryu.controller.handler import set_ev_cls
from ryu.ofproto import ofproto_v1_0
from ryu.lib.mac import haddr_to_bin
from ryu.lib import addrconv
from ryu.lib.packet import packet_base, packet_utils
from ryu.lib.packet import packet, ethernet, ether_types, lldp
from ryu.topology import event, switches
from ryu.topology.api import get_switch, get_link

import time

from collections import deque, Counter
from heapq import heappush, heappop

def dijstra_structure(switches, adj):
    blocked_ports = {}
    for switch in switches:
        heap = [(0,switch,-1,-1,-1)]
        visited = set()
        tree_edges = set()
        distance = {switch:0}
        while len(heap):
            dist, node, node_port, par, par_port = heappop(heap)
            if node in visited:
                continue
            if par != -1:
                p1, p2 = (node, node_port), (par, par_port)
                tree_edges.add((min((p1,p2),(p2,p1)),max((p1,p2),(p2,p1))))
            visited.add(node)
            for neighbor in adj[node]:
                if neighbor[0] not in visited and (neighbor[0] not in distance or dist+neighbor[-1]<distance[neighbor[0]]):
                    distance[neighbor[0]] = dist+neighbor[-1]
                    heappush(heap,(dist+neighbor[-1],neighbor[0],neighbor[2],node,neighbor[1]))
        blocked_ports[switch] = {sw:set() for sw in switches}
        for nod in adj:
            for neighbor in adj[nod]:
                p1, p2 = (nod, neighbor[1]), (neighbor[0], neighbor[2])
                if (min((p1,p2),(p2,p1)),max((p1,p2),(p2,p1))) not in tree_edges:
                    blocked_ports[switch][p1[0]].add(p1[1])
                    blocked_ports[switch][p2[0]].add(p2[1])
    return blocked_ports
                    

        
    

class CustomTLV:
    def __init__(self, tlv_type, tlv_value):
        self.tlv_type = tlv_type
        self.tlv_value = tlv_value

    def serialize(self):
        # Get the length of the TLV value in bytes
        tlv_length = len(self.tlv_value)
        # Ensure that the length is less than 512 (since you're using 9 bits for the length)
        if tlv_length >= 512:
            raise ValueError("TLV value is too long (max 511 bytes allowed)")

        # Pack the type (7 bits) and length (9 bits) into a 16-bit integer
        tlv_type_length = (self.tlv_type << 9) | tlv_length
        # Convert this 16-bit integer to 2 bytes
        tlv_type_length_bytes = tlv_type_length.to_bytes(2, 'big')
        
        # Return the serialized TLV (type-length followed by the value)
        return tlv_type_length_bytes + self.tlv_value

class SPR_Switch(RyuApp):
    OFP_VERSIONS = [ofproto_v1_0.OFP_VERSION]

    def __init__(self, *args, **kwargs):
        super(SPR_Switch, self).__init__(*args, **kwargs)
        self.mac_to_port = {}
        self.mac_switch = {}
        self.blocked_ports = {}
        self.sw_dpids = set()
        self.dpids_datapath = {}
        self.adj = {}
        self.delay = {}
        self.change = False
    
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
        msg = ev.msg
        datapath = msg.datapath
        ofproto = datapath.ofproto
        parser = datapath.ofproto_parser

        in_port = msg.in_port

        pkt = packet.Packet(msg.data)
        eth = pkt.get_protocol(ethernet.ethernet)
        if eth.ethertype == ether_types.ETH_TYPE_LLDP:
            self.match_lldp(pkt.get_protocol(lldp.lldp), datapath.id, in_port )
            return
        dst = eth.dst
        src = eth.src
        if(self.change):
            self.handle_topo_change()
            self.change = False

        dpid = datapath.id
        self.mac_to_port.setdefault(dpid, {})

        # self.logger.info("packet in %s %s %s %s", dpid, src, dst, in_port)

        self.mac_to_port[dpid][src] = in_port

        if dst in self.mac_to_port[dpid]:
            actions = [parser.OFPActionOutput(self.mac_to_port[dpid][dst])]
            self.add_flow(datapath, in_port, dst, src, actions)
        else:
            actions = []
            
            if src not in self.mac_switch:
                self.mac_switch[src] = dpid
            mac_switch = self.mac_switch[src]
            for port_no, port in datapath.ports.items():
                if port_no not in self.blocked_ports[mac_switch][dpid] and port_no != in_port:
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
        self.dpids_datapath[ev.switch.dp.id] = ev.switch.dp
        self.adj[ev.switch.dp.id] = set()
        self.change = True
    @set_ev_cls(event.EventSwitchLeave)
    def remove_switch(self,ev):
        removed_switch = ev.switch.dp.id
        self.sw_dpids.remove(removed_switch)
        del self.dpids_datapath[removed_switch]
        del self.adj[removed_switch]
        for switch in self.adj:
            self.adj[switch].discard(removed_switch)
        self.change = True
    @set_ev_cls(event.EventLinkAdd)
    def link_add(self,ev):
        self.adj[ev.link.src.dpid].add((ev.link.dst.dpid, ev.link.src.port_no, ev.link.dst.port_no))
        self.adj[ev.link.dst.dpid].add((ev.link.src.dpid, ev.link.dst.port_no, ev.link.src.port_no))
        for i in range(10):
            datapath, src_port_no = self.dpids_datapath[ev.link.src.dpid], ev.link.src.port_no
            lldp_pkt = self.build_lldp_packet(ev.link.src, src_port_no)
            actions = [datapath.ofproto_parser.OFPActionOutput(src_port_no)]
            out = datapath.ofproto_parser.OFPPacketOut(
                datapath=datapath,
                buffer_id=datapath.ofproto.OFP_NO_BUFFER,
                in_port=datapath.ofproto.OFPP_CONTROLLER,
                actions=actions,
                data=lldp_pkt.data
            )
            datapath.send_msg(out)
        self.change = True
        
    @set_ev_cls(event.EventLinkDelete)
    def link_delete(self,ev):
        self.adj[ev.link.src.dpid].discard((ev.link.dst.dpid, ev.link.src.port_no, ev.link.dst.port_no))
        self.adj[ev.link.dst.dpid].discard((ev.link.src.dpid, ev.link.dst.port_no, ev.link.src.port_no))
        self.change = True
    
    def handle_topo_change(self):
        self.logger.info("Topology changed")
        self.logger.info(self.sw_dpids)
        self.mac_to_port.clear()
        adj = {}
        for switch in self.adj:
            adj[switch] = set()
            for link in self.adj[switch]:
                adj[switch].add((link[0],link[1],link[2],self.delay[(switch,link[1])][0]/self.delay[(switch,link[1])][1] if (switch,link[1]) in self.delay else 1e9))
        self.logger.info(adj)
        self.blocked_ports = dijstra_structure(self.sw_dpids, adj)
        self.logger.info(self.blocked_ports)

    # LLDP Packet Handlers
    def build_lldp_packet(self, src, port_no):
        timestamp = str(time.time()).encode('utf-8')
        # Create Ethernet frame
        eth = ethernet.ethernet(
            dst=lldp.LLDP_MAC_NEAREST_BRIDGE,
            src=src.hw_addr,  
            ethertype=ethernet.ether.ETH_TYPE_LLDP  
        )

        # Create LLDP Chassis ID and Port ID
        chassis_id = lldp.ChassisID(
            subtype=lldp.ChassisID.SUB_LOCALLY_ASSIGNED,
            chassis_id=str(src.dpid).encode('utf-8')  # Use the datapath ID as chassis ID
        )
        port_id = lldp.PortID(
            subtype=lldp.PortID.SUB_PORT_COMPONENT,
            port_id=str(port_no).encode('utf-8')  # Use the port number as port ID
        )
        ttl = lldp.TTL(ttl=120)  # Time-to-live for LLDP packet
        
        # Add a custom TLV to differentiate LLDP packets
        custom_tlv = CustomTLV(
            tlv_type=127,  # Use TLV type 127 for custom TLVs (per LLDP spec)
            tlv_value=timestamp  # A custom value to differentiate (in string)
        )

        # Build the LLDP packet with Ethernet and LLDP protocols
        lldp_pkt = packet.Packet()
        lldp_pkt.add_protocol(eth)
        lldp_pkt.add_protocol(lldp.lldp(tlvs=[chassis_id, port_id, ttl, custom_tlv]))
        lldp_pkt.serialize()

        return lldp_pkt      
    
    def match_lldp(self, lldp_pkt, src_id, src_port):
        for tlv in lldp_pkt.tlvs:
            if isinstance(tlv, lldp.ChassisID) or isinstance(tlv, lldp.PortID) or isinstance(tlv, lldp.TTL):
                continue  # Skip known TLV types
            if hasattr(tlv, 'tlv_type') and tlv.tlv_type == 127:
                tlv_value = float(tlv.tlv_info)
                pair = (src_id,src_port)
                if((src_id,src_port) not in self.delay):
                    self.delay[(src_id,src_port)] = (time.time() - tlv_value, 1)
                else:
                    self.delay[pair] = (self.delay[pair][0] + (time.time() - tlv_value)/2, self.delay[pair][1]+1)
                print("LLDP", self.delay)

            
