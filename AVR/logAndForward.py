import argparse
import serial
#import Gpib
import re
from visa import *
import sys
import fileinput

parser = argparse.ArgumentParser(description='Log data from project and forward some stuff to GPIB')
parser.add_argument('filename', metavar='filename', type=str, help='The filename to log to')
parser.add_argument('--show', action='store_true', help='Print to the command line and log')
args=parser.parse_args()


def parseAndPrint (logFile, gpibInstruments, message):

	#we want to separate the GPIB commands from the log lines
	#result will be none if the regex didn't match one of our special
	#GPIB command lines
	if args.show:
		sys.stdout.write(message)
		sys.stdout.flush()

	logFile.write(message)
	logFile.flush()

	commRegex = re.compile(r"<(\w+)<([\w\-., ]+)>>.*")
	errRegex = re.compile(r"[*][*]([\w:\-., ]*)[*][*]")
	doneRegex = re.compile(r"[*][*]Exit[*][*]")

	result=commRegex.match(message)
	if result is not None:
		try:
			instrument = gpibInstruments[result.group(1)]
			instrument.write(result.group(2)+'\r\n')
		except:
			sys.stdout.write("Instrument "+result.group(1)+" could not be found.  Continuing.\n")
			sys.stdout.flush()

	result=errRegex.match(message)

	if result is not None and not args.show:
		sys.stdout.write(message)
		sys.stdout.flush()

	result=doneRegex.match(message)
	if result is not None:
		logFile.close()
		sys.exit()

	return




serPort = serial.Serial( 2, baudrate=19200, timeout=10 )

#psGpibPort = Gpib.Gpib(pad=10)

gpibInstruments = dict([])
try:
    psGpibPort = instrument("GPIB::2")
    gpibInstruments['ps'] = psGpibPort
except:
    print "Could not open power supply - continuing without it"

try:
    loadGpibPort = instrument("GPIB::3")
    gpibInstruments['al'] = loadGpibPort
except:
    print "Could not open active load - continuing without it"

#gpibInstruments = {'ps': psGpibPort, 'al': loadGpibPort}
#gpibInstruments['ps'].write('r\r\n')

bytesWaiting=0;

try:
	logFile = open(args.filename, 'wb')
	while(True):
		while serPort.inWaiting() != bytesWaiting:
			bytesWaiting = serPort.inWaiting();
			time.sleep(0.1)

		if bytesWaiting > 0:
			bytes = bytearray(bytesWaiting)
			serPort.readinto(bytes)
			message = str(bytes)

			for m in iter(message.splitlines()):
				parseAndPrint(logFile, gpibInstruments, m + "\r\n")

finally:
	logFile.close()


