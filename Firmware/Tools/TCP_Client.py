import socket
import argparse
import multiprocessing

def client(server, port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((server, port))
    while True:
        payload = sock.recv(1024)
        print(payload.decode('latin1'), end='')
        
if __name__ == "__main__":

    parser = argparse.ArgumentParser(
        description='TCP Client')

    parser.add_argument('-server', type=str, default="",
                        help='Host Name or IP Address of the TCP Server')

    parser.add_argument('-port', type=int, default=2948,
                        help='TCP Port Number')

    args = parser.parse_args()

    print("Connecting to " + args.server + " on port " + str(args.port))

    proc = multiprocessing.Process(target = client, args = (args.server, args.port))
    proc.start()

    try:
        while True:
            pass
    except KeyboardInterrupt:
        proc.terminate()
