import socket
import argparse
import multiprocessing
import time

def client(server, port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind((server, port))
    sock.listen()
    totalBytes = 0
    lastBytes = 0
    startTime = time.time()
    while True:
        print("Waiting for new connection")
        conn, addr = sock.accept()
        print("Connection from " + str(addr))
        print("Sending ICY 200 OK")
        conn.send(b"ICY 200 OK")
        conn.settimeout(5.0)
        keepGoing = True
        while keepGoing:
            try:
                payload = conn.recv(1024)
            except TimeoutError:
                keepGoing = False
            totalBytes += len(payload)
            if time.time() - startTime > 5:
                startTime = time.time()
                rate = int((totalBytes - lastBytes) / 5)
                lastBytes = totalBytes
                print("Total bytes: " + str(totalBytes) + " (" + str(rate) + " Bps)")
        conn.close()
        print()
        
if __name__ == "__main__":

    parser = argparse.ArgumentParser(
        description='TCP Server')

    parser.add_argument('-server', type=str, default="",
                        help='Host Name or IP Address (e.g. 127.0.0.1 localhost)')

    parser.add_argument('-port', type=int, default=2101,
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
