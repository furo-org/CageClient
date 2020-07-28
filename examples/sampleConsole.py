#!/usr/bin/python
# Copyright 2018 Tomoaki Yoshida<yoshida@furo.org>
#This software is released under the MIT License.
#http://opensource.org/licenses/mit-license.php

# send strings to cage::CommActor as console command
import zmq
import sys

command='stat fps'

if len(sys.argv)>1:
  host=sys.argv[1]

if len(sys.argv)>2:
  command=' '.join(sys.argv[2:])

cxt=zmq.Context()
sock=cxt.socket(zmq.REQ)
sock.setsockopt(zmq.REQ_CORRELATE, True);
sock.setsockopt(zmq.REQ_RELAXED, True);
sock.setsockopt(zmq.RCVTIMEO, 1000);
sock.setsockopt(zmq.SNDTIMEO, 1000);

sock.connect('tcp://%s:54323'%host)
print 'connecting to ', 'tcp://%s:54323'%host
req={
  'Type':'Console',
  'Input':command
  }

sock.send_json(req)
print req

rep=sock.recv()

print rep
