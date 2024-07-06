import socket
import argparse
import multiprocessing


def client(server, port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind((server, port))
    sock.listen()
    while True:
        print("Waiting for new connection")
        conn, addr = sock.accept()
        print("Connection from " + str(addr))
        conn.settimeout(5.0)
        keepGoing = True
        while keepGoing:
            try:
                payload = conn.recv(1024)
            except TimeoutError:
                keepGoing = False
            if len(payload) > 0:
                print(payload.decode(), end='')
        conn.close()
        print()
        
if __name__ == "__main__":

    parser = argparse.ArgumentParser(
        description='TCP Server')

    parser.add_argument('-server', type=str, default="",
                        help='Host Name or IP Address (e.g. 127.0.0.1 localhost)')

    parser.add_argument('-port', type=int, default=2948,
                        help='TCP Port Number')

    args = parser.parse_args()

    if (args.server != ""):
        print("Listening on " + args.server + ":" + str(args.port))
    else:
        print("Listening on port " + str(args.port))

    proc = multiprocessing.Process(target = client, args = (args.server, args.port))
    proc.start()

    try:
        while True:
            pass
    except KeyboardInterrupt:
        proc.terminate()
