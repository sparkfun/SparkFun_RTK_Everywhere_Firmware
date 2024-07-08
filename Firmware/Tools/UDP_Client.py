import socket
import argparse
import multiprocessing

def client(server, port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.bind((server, port))
    while True:
        payload = sock.recvfrom(1024)
        print(payload[0].decode('latin1'), end='')
        
if __name__ == "__main__":

    parser = argparse.ArgumentParser(
        description='UDP Client')

    parser.add_argument('-server', type=str, default="",
                        help='Host Name or IP Address of the UDP Server')

    parser.add_argument('-port', type=int, default=10110,
                        help='UDP Port Number')

    args = parser.parse_args()

    if (args.server != ""):
        print("Connecting to " + args.server + " on port " + str(args.port))
    else:
        print("Listening on port " + str(args.port))

    proc = multiprocessing.Process(target = client, args = (args.server, args.port))
    proc.start()

    try:
        while True:
            pass
    except KeyboardInterrupt:
        proc.terminate()
