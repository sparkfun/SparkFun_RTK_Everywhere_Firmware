# Performs a 'diff' on two RTK (Everywhere) Firmware settings CSV files
#
# E.g. the settings CSVs passed between the firmware and the web config javascript
# The order of the settings going in one direction is very different to the other direction,
# making it tricky to compare them directly
# This code performs an exhaustive diff: looking for differences between the two;
# any duplicates; and finding any settings present in one but not the other

import sys
import os

class RTK_Settings_Diff():

    def __init__(self, File1:str = None, File2:str = None):
        self.filename1 = File1
        self.filename2 = File2

    def setFilename1(self, File:str):
        self.filename1 = File

    def setFilename2(self, File:str):
        self.filename2 = File

    def readByte(self, fi):
        fileBytes = fi.read(1)
        if (len(fileBytes) == 0):
            return None
        #print(chr(fileBytes[0]))
        return chr(fileBytes[0])
    
    def readFile(self, filename):
        print('Processing',filename)
        print()
        filesize = os.path.getsize(filename) # Record the file size

        # Try to open file for reading
        try:
            fi = open(filename,"rb")
        except:
            raise Exception('Invalid file!')
        
        # Read the file
        firstThing = ''
        secondThing = ''
        isFirstThing = True
        things = {}

        while filesize > 0:
            c = self.readByte(fi)
            if c == None:
                fi.close()
                return things
            elif c == ',':
                isFirstThing = not isFirstThing
                if isFirstThing:
                    if firstThing in things.keys(): # Seen it before?
                        print('Duplicate setting: {},{} : {}'.format(firstThing, secondThing, things[firstThing]))
                    else:
                        things[firstThing] = secondThing
                    firstThing = ''
                    secondThing = ''
                    #print(firstThing, secondThing)
            elif c == '\r':
                pass
            elif c == '\n':
                pass
            else:
                if isFirstThing:
                    firstThing = firstThing + str(c)
                else:
                    secondThing = secondThing + str(c)
            filesize = filesize - 1
        
        fi.close()
        return things

    def diff(self):

        print()

        things1 = self.readFile(self.filename1)
        #print(things1)

        print()

        things2 = self.readFile(self.filename2)
        #print(things2)

        print()

        for thing1 in things1.keys():
            if thing1 not in things2.keys():
                print('{},{} : only found in file 1'.format(thing1,things1[thing1]))

        print()

        for thing2 in things2.keys():
            if thing2 not in things1.keys():
                print('{},{} : only found in file 2'.format(thing2,things2[thing2]))

        print()

        for thing1 in things1.keys():
            if thing1 in things2.keys():
                if things1[thing1] != things2[thing1]:
                    print('Diff: {},{} : {}'.format(thing1,things1[thing1],things2[thing1]))

if __name__ == '__main__':

    import argparse

    parser = argparse.ArgumentParser(description='SparkFun RTK Firmware Settings Diff')
    parser.add_argument('File1', metavar='File1', type=str, help='The path to the first settings CSV file')
    parser.add_argument('File2', metavar='File2', type=str, help='The path to the second settings CSV file')
    args = parser.parse_args()

    diff = RTK_Settings_Diff(args.File1, args.File2)

    diff.diff()
