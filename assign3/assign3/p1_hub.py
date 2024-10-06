from ryu.base.app_manager import RyuApp
from ryu.controller import ofp_event
from ryu.controller.handler import MAIN_DISPATCHER, CONFIG_DISPATCHER
from ryu.controller.handler import set_ev_cls
from ryu.ofproto import ofproto_v1_0, ofproto_v1_3
from ryu.lib.mac import haddr_to_bin
from ryu.lib.packet import packet
from ryu.lib.packet import ethernet
from ryu.lib.packet import ether_types

class HubController(RyuApp):
    OFP_VERSIONS = [ofproto_v1_0.OFP_VERSION, ofproto_v1_3.OFP_VERSION]
    def __init__(self, *args, **kwargs):
        super(HubController, self).__init__(*args, **kwargs)

    def add_flow_3(self, datapath, priority, match, actions, buffer_id=None):
        ofproto = datapath.ofproto
        parser = datapath.ofproto_parser

        inst = [parser.OFPInstructionActions(ofproto.OFPIT_APPLY_ACTIONS,
                                             actions)]
        if buffer_id:
            mod = parser.OFPFlowMod(datapath=datapath, buffer_id=buffer_id,
                                    priority=priority, match=match,
                                    instructions=inst)
        else:
            mod = parser.OFPFlowMod(datapath=datapath, priority=priority,
                                    match=match, instructions=inst)
        datapath.send_msg(mod)

    @set_ev_cls(ofp_event.EventOFPSwitchFeatures, CONFIG_DISPATCHER)
    def switch_features_handler(self, ev):
        datapath = ev.msg.datapath
        ofproto = datapath.ofproto
        parser = datapath.ofproto_parser
        if(ofproto.OFP_VERSION==ofproto_v1_0.OFP_VERSION):
            return
        match = parser.OFPMatch()
        actions = [parser.OFPActionOutput(ofproto.OFPP_CONTROLLER,
                                          ofproto.OFPCML_NO_BUFFER)]
        self.add_flow_3(datapath, 0, match, actions)  

    @set_ev_cls(ofp_event.EventOFPPacketIn, MAIN_DISPATCHER)
    def _packet_in_handler(self, ev):
        self.logger.info("Packet received")
        msg = ev.msg
        datapath = msg.datapath
        ofproto = datapath.ofproto
        parser = datapath.ofproto_parser

        in_port = msg.in_port if ofproto.OFP_VERSION == ofproto_v1_0.OFP_VERSION else msg.match['in_port']

        pkt = packet.Packet(msg.data)
        eth = pkt.get_protocol(ethernet.ethernet)
        if eth.ethertype == ether_types.ETH_TYPE_LLDP:
            # ignore link layer discovery message to avoid unnecessary flooding
            return
        dst = eth.dst
        src = eth.src

        self.logger.info("packet in %s %s %s %s", datapath.id, src, dst, in_port)

        actions = [parser.OFPActionOutput(ofproto.OFPP_FLOOD)]#forwarding to all ports except the incoming port

        data = None
        if msg.buffer_id == ofproto.OFP_NO_BUFFER:
            data = msg.data

        out = parser.OFPPacketOut(
            datapath=datapath, buffer_id=msg.buffer_id, in_port=in_port,
            actions=actions, data=data)
        datapath.send_msg(out)

    # @set_ev_cls(ofp_event.EventOFPSwitchFeatures, MAIN_DISPATCHER)
    # def switch_features_handler(self, ev):
    #     self.logger.info("Switch connected: %s", ev.msg.datapath.id)
    
    @set_ev_cls(ofp_event.EventOFPPortStatus, MAIN_DISPATCHER)
    def _port_status_handler(self, ev):
        msg = ev.msg
        reason = msg.reason
        port_no = msg.desc.port_no
    
        ofproto = msg.datapath.ofproto
    
        port_str = str(port_no)
    
        if reason == ofproto.OFPPR_ADD:
            self.logger.info("Port added: %s", port_str)
        elif reason == ofproto.OFPPR_DELETE:
            self.logger.info("Port deleted: %s", port_str)
        elif reason == ofproto.OFPPR_MODIFY:
            self.logger.info("Port modified: %s", port_str)
        else:
            self.logger.info("Illegal port state: %s %s", port_str, reason)

    