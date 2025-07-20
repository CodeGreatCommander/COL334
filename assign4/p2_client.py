import socket
import argparse
import time

# Constants
MSS = 1400  # Maximum Segment Size
RECEIVER_WINDOW_SIZE = 10000  # Receiver window size

def receive_file(server_ip, server_port, pref_outfile):
    """
    Receive the file from the server with reliability, handling packet loss
    and reordering.
    """
    # Initialize UDP socket
    
    ## Add logic for handling packet loss while establishing connection
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    client_socket.settimeout(2)  # Set timeout for server response

    server_address = (server_ip, server_port)
    expected_seq_num = 0
    output_file_path = f"{pref_outfile}received_file.txt"  # Default file name

    received_first_packet = False

    with open(output_file_path, 'wb') as file:
        receiver_window = {}
        while True:
            try:
                if not received_first_packet:
                    client_socket.sendto(b"START", server_address)
                # Send initial connection request to server

                # Receive the packet
                packet, _ = client_socket.recvfrom(MSS + 100)  # Allow room for headers
                received_first_packet = True
                
                # Logic to handle end of file

                if packet == b"END":
                    # print("Received END signal from server, file transfer complete and sent END_ACK",flush = True)
                    client_socket.sendto(b"END_ACK", server_address)  # Send acknowledgment
                    time.sleep(0.1)#TODO: Find a solution for it
                    # close the socket 
                    client_socket.close()
                    # print("Client Socket closed")
                    # # print the length of the file
                    # print(f"Length of the file received is {file.tell()} bytes",flush = True)
                    break
                
                seq_num, data = parse_packet(packet)
                if(seq_num < RECEIVER_WINDOW_SIZE*MSS + expected_seq_num and seq_num not in receiver_window):
                    receiver_window[seq_num] = data

                # If the packet is in order, write it to the file
                if seq_num == expected_seq_num:
                    # print(f"Received packet {seq_num}, writing to file",flush = True)
                    while expected_seq_num in receiver_window:
                        file.write(receiver_window[expected_seq_num])
                        # print(f"{seq_num} writing to file",flush = True)
                        # Update expected seq number and send cumulative ACK for the received packet
                        temp = expected_seq_num
                        expected_seq_num += len(receiver_window[expected_seq_num])
                        receiver_window.pop(temp, None)
                    send_ack(client_socket, server_address, expected_seq_num)
                elif seq_num < expected_seq_num:
                    # Duplicate or old packet, send ACK again
                    # print(f"Received duplicate packet {seq_num}, sending ACK for {expected_seq_num} again", flush = True)
                    send_ack(client_socket, server_address, expected_seq_num)
                else:
                    # print(f"Received out of order packet {seq_num}, sending ACK for {expected_seq_num} again", flush = True)
                    send_ack(client_socket, server_address, expected_seq_num)
            except socket.timeout:
                # print("Timeout waiting for data",time.time(), flush = True)
                pass
                

def parse_packet(packet):
    """
    Parse the packet to extract the sequence number and data.
    """
    seq_num, data = packet.split(b'|', 1)
    return int(seq_num), data

def send_ack(client_socket, server_address, seq_num):
    """
    Send a cumulative acknowledgment for the received packet.
    """
    ack_packet = f"{seq_num}|ACK".encode()
    client_socket.sendto(ack_packet, server_address)
    # print(f"Sent cumulative ACK for packet {seq_num}", flush = True)


# Parse command-line arguments
parser = argparse.ArgumentParser(description='Reliable file receiver over UDP.')
parser.add_argument('server_ip', help='IP address of the server')
parser.add_argument('server_port', type=int, help='Port number of the server')
parser.add_argument('--pref_outfile', default='', help='Prefix for the output file')

args = parser.parse_args()
# print(args.pref_outfile)

# Run the client
receive_file(args.server_ip, args.server_port, args.pref_outfile)
# print("-"*100,flush= True)

