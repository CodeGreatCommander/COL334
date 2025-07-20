import socket
import time
import argparse

# Constants
MSS = 1400  # Maximum Segment Size for each packet
WINDOW_SIZE = 1  # Number of packets in flight
DUP_ACK_THRESHOLD = 3  # Threshold for duplicate ACKs to trigger fast recovery
FILE_PATH = "./sample_files/sample_1.txt"
TIMEOUT = 1.0  # Initialize timeout to some value but update it as ACK packets arrive
ALPHA = 0.125
BETA = 0.25
def send_file(server_ip, server_port, enable_fast_recovery):
    """
    Send a predefined file to the client, ensuring reliability over UDP.
    """
    # Initialize UDP socket
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    server_socket.bind((server_ip, server_port))

    global TIMEOUT
    global WINDOW_SIZE



    # print(f"Server listening on {server_ip}:{server_port}")

    # Wait for client to initiate connection
    client_address = None
    file_path = FILE_PATH  # Predefined file name

    with open(file_path, 'rb') as file:
        seq_num = 0
        unacked_packets = {}
        duplicate_ack_count = 0
        last_ack_received = 0
        
        estimated_rtt = -1
        dev_rtt = -1
        retran_unacked_packets = set()


        while True:
            while len(unacked_packets)<WINDOW_SIZE: ## Use window-based sending
                chunk = file.read(MSS)
                if not chunk:
                    # End of file
                    # server_socket.sendto(b"END", client_address) 
                    # Send end signal to the client 
                    break

                # Create and send the packet
                packet = create_packet(seq_num, chunk)
                if client_address:
                    server_socket.sendto(packet, client_address)
                else:
                    # # print("Waiting for client connection...")
                    data, client_address = server_socket.recvfrom(1024)
                    # print(f"Connection established with client {client_address}")
                    server_socket.sendto(packet, client_address)  # Send the first packet
                

                ## 
                unacked_packets[seq_num] = (packet, time.time())  # Track sent packets
                # print(f"Sent packet {seq_num}")
                seq_num += len(chunk)

            # Wait for ACKs and retransmit if needed
            try:
            	## Handle ACKs, Timeout, Fast retransmit
                server_socket.settimeout(TIMEOUT)
                ack_packet, _ = server_socket.recvfrom(1024)
                # print(f"Received ACK packet: {ack_packet}")
                if ack_packet == b"START":
                    continue
                ack_seq_num = get_seq_no_from_ack_pkt(ack_packet)
                ack_orig_seq_num =  max([seq_num for seq_num in unacked_packets.keys() if seq_num < ack_seq_num], default=None)

                if ack_seq_num > last_ack_received:
                    # print(f"Received cumulative ACK for packet {ack_seq_num}", flush = True)
                    if ack_orig_seq_num not in retran_unacked_packets:
                        if(estimated_rtt==-1):
                            estimated_rtt = time.time()-unacked_packets[ack_orig_seq_num][1]
                            dev_rtt = estimated_rtt/2
                            WINDOW_SIZE = max(1, int(5e7*estimated_rtt/MSS/8))
                            # print("WINDOW_SIZE:", WINDOW_SIZE,"estimated_rtt:",estimated_rtt,"dev_rtt:", dev_rtt, "TIMEOUT:",TIMEOUT)
                        else:
                            estimated_rtt = (1-ALPHA)*estimated_rtt + ALPHA*(time.time()-unacked_packets[ack_orig_seq_num][1])
                            dev_rtt = (1-BETA)*dev_rtt + BETA*abs(time.time()-unacked_packets[ack_orig_seq_num][1]-estimated_rtt)
                            # print("WINDOW_SIZE:", WINDOW_SIZE,"estimated_rtt:",estimated_rtt,"dev_rtt:", dev_rtt, "TIMEOUT:",TIMEOUT)


                    last_ack_received = ack_seq_num
                    retran_unacked_packets = {seq for seq in retran_unacked_packets if seq >= ack_seq_num}
                    # Slide the window forward
                    duplicate_ack_count = 1 # Reset duplicate ACK count
                    # Remove acknowledged packets from the buffer
                    unacked_packets = {seq: packet for seq, packet in unacked_packets.items() if seq >= ack_seq_num} 
                    
                elif ack_seq_num == last_ack_received:
                    # Duplicate ACK received
                    if(duplicate_ack_count!=-1):
                        duplicate_ack_count += 1
                    # print(f"Received duplicate ACK for packet {ack_seq_num}, count={duplicate_ack_count}", flush = True)

                    if enable_fast_recovery and duplicate_ack_count >= DUP_ACK_THRESHOLD:
                        # print("Entering fast recovery mode")
                        retran_unacked_packets.add(min(unacked_packets.keys()))
                        fast_recovery(server_socket, client_address, unacked_packets)
                        duplicate_ack_count = -1  # Stop further duplicate ack till new packet
                if(estimated_rtt!=-1):
                    TIMEOUT = estimated_rtt+4*dev_rtt
                else:
                    TIMEOUT = 1

            except socket.timeout:
                # Timeout handling: retransmit all unacknowledged packets
                retran_unacked_packets = retran_unacked_packets.union(unacked_packets.keys())
                # print("Timeout occurred, retransmitting unacknowledged packets",flush = True)
                retransmit_unacked_packets(server_socket, client_address, unacked_packets)
                # fast_recovery(server_socket, client_address, unacked_packets)
                TIMEOUT*=2

            # Check if we are done sending the file
            if not chunk and len(unacked_packets) == 0:
                # print("File transfer complete",flush=True)
                send_end_signal_reliably(server_socket, client_address)
                # close the socket
                server_socket.close()
                # print("Server Socket closed")
                break

def create_packet(seq_num, data):
    """
    Create a packet with the sequence number and data.
    """
    # Convert the sequence number to a string and then encode it to bytes
    seq_num_bytes = str(seq_num).encode()

    # Ensure the data is in bytes
    if not isinstance(data, bytes):
        data = data.encode()

    # Concatenate the sequence number, delimiter, and data to form the packet
    packet = seq_num_bytes + b'|' + data

    return packet

    
def retransmit_unacked_packets(server_socket, client_address, unacked_packets):
    """
    Retransmit all unacknowledged packets.
    """
    for seq_num, (packet, _) in unacked_packets.items():
        server_socket.sendto(packet, client_address)
        # print(f"Retransmitted packet {seq_num}")
        unacked_packets[seq_num] = (packet, time.time())
    
    

def fast_recovery(server_socket, client_address, unacked_packets):
    """
    Retransmit the earliest unacknowledged packet (fast recovery).
    """
    seq_num, (packet, _) = min(unacked_packets.items(), key=lambda x: x[0])
    server_socket.sendto(packet, client_address)
    # print(f"Retransmitted packet {seq_num} for fast recovery")

    # update the time value for this packet 
    unacked_packets[seq_num] = (packet, time.time())
    
def get_seq_no_from_ack_pkt(ack_packet):
    """
    Extract the sequence number from the acknowledgment packet.
    """
    return int(ack_packet.decode().split('|')[0])

def send_end_signal_reliably(server_socket, client_address):
    """
    Reliably send the "END" packet to the client until acknowledgment is received.
    """
    end_packet = b"END"
    acknowledged = False
    retries = 0
    max_retries = 5  # Limit the number of retries to avoid infinite loop

    while not acknowledged and retries < max_retries:
        server_socket.sendto(end_packet, client_address)
        # print("Sent 'END' packet to client")
        
        try:
            # Wait for acknowledgment of the "END" packet
            # print(TIMEOUT)
            server_socket.settimeout(TIMEOUT)
            ack, _ = server_socket.recvfrom(1024)
            if ack == b"END_ACK":
                # print("Received acknowledgment for 'END' packet from client")
                acknowledged = True
            else:
                # print("Received unexpected packet, retrying 'END' transmission")
                pass
                
        except socket.timeout:
            retries += 1
            # print("Timeout waiting for 'END' acknowledgment, retrying...")
    
    if not acknowledged:
        # print("Failed to receive acknowledgment for 'END' packet after retries")
        pass


# Parse command-line arguments
parser = argparse.ArgumentParser(description='Reliable file transfer server over UDP.')
parser.add_argument('server_ip', help='IP address of the server')
parser.add_argument('server_port', type=int, help='Port number of the server')
parser.add_argument('fast_recovery', type=bool, help='Enable fast recovery')

args = parser.parse_args()

# Run the server
send_file(args.server_ip, args.server_port, args.fast_recovery)
# print("-"*100,flush= True)