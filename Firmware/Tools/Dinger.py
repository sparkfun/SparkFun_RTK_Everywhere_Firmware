import argparse
import multiprocessing
import serial
from datetime import datetime

def dinger(port, baud, find, printLines):
    ser = serial.Serial(port, baud)
    buffer = list(find)
    for i in range(len(find)):
        buffer[i] = ' '
    while True:
        for i in range(1, len(find)):
            buffer[i-1] = buffer[i]
        buffer[len(find) - 1] = ser.read()
        s = ''
        for c in buffer:
            s += chr(ord(c))
        if s == find:
            print('\a') # Ding
            print("Ding! " + str(find) + " found at " + datetime.now().isoformat())
            if (printLines > 0):
                for i in range (printLines):
                    print(str(ser.readline())[2:-1])
        
if __name__ == "__main__":

    parser = argparse.ArgumentParser(
        description='Dinger')

    parser.add_argument('-port', type=str, default="COM1",
                        help='COM port')

    parser.add_argument('-baud', type=int, default=115200,
                        help='Baud rate')

    parser.add_argument('-find', type=str, default="# => ",
                        help='Ding on this')

    parser.add_argument('-print', type=int, default=1,
                        help='Print following lines')

    args = parser.parse_args()

    proc = multiprocessing.Process(target = dinger, args = (args.port, args.baud, args.find, args.print))
    proc.start()

    try:
        while True:
            pass
    except KeyboardInterrupt:
        proc.terminate()
