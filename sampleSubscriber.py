#!/usr/bin/python

# Dump all information published by cage::CommActor
import zmq
import sys

port="54321"
host="127.0.0.1"
if len(sys.argv)>1:
  host=sys.argv[1]

cxt=zmq.Context()
sock=cxt.socket(zmq.SUB)

sock.connect('tcp://%s:%s'%(host,port))
sock.setsockopt(zmq.SUBSCRIBE,'')

while True:
  print (sock.recv())
