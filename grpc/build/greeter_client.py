#!/usr/bin/env python
# Copyright 2015, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""The Python implementation of the GRPC helloworld.Greeter client."""

from __future__ import print_function

import argparse
import logging
import os
import sys
import time

import grpc

import greeter_pb2
import greeter_pb2_grpc


def run(flags):
  # The deadline is absolute - time taken by wait_for_ready is not available to
  # the RPC.
  deadline = time.time() + flags.deadline_ms/1000.0

  logging.info('Creating channel to greeter server at %s',
               flags.greeter_server)
  channel = grpc.insecure_channel(flags.greeter_server)
  if flags.wait_for_ready:
    logging.info('Waiting for channel to become ready...')
    try:
      grpc.channel_ready_future(channel).result(timeout=deadline - time.time())
    except:
      # Ignoring timeouts and other errors here.
      pass

  logging.info('Creating Greeter stub.')
  stub = greeter_pb2_grpc.GreeterStub(channel)

  logging.info('Sending request')
  try:
    response = stub.SayHello(greeter_pb2.HelloRequest(name=flags.user,
                                                      locale=flags.locale),
                             timeout=deadline - time.time())
    logging.info('Greeting received: %s', response.message)
    print ('Greeting received: %s' % response.message)
  except grpc.RpcError:
    logging.exception('Failed to fetch greeting:')
    print ('Failed to fetch greeting.')


if __name__ == '__main__':
  parser = argparse.ArgumentParser(description='The greeter client')
  parser.add_argument('--user', default='world',
                      help='The user to greet!')
  parser.add_argument('--greeter_server', default='localhost:50051',
                      help='Server address of the greeter server.')
  parser.add_argument('--locale', default='en_US',
                      help='The locale for the greeting.')
  parser.add_argument('--deadline_ms', type=int, default=20*1000,
                      help='Deadline in milliseconds.')
  parser.add_argument('--wait_for_ready',
                      type=lambda a: a[0].lower() not in ('0', 'f', 'n'),
                      default=True,
                      help=('Whether to wait for the backend to become '
                            'available. If false, fails fast.'))
  parser.add_argument('--log_dir', default='',
                      help='Server address of the translation server.')
  flags = parser.parse_args()
  logging.basicConfig(
      filename=os.path.join(flags.log_dir, 'greeter_client.py.INFO'),
      format=('%(asctime)s.%(msecs)d %(levelname)s %(module)s:%(lineno)s '
              '%(message)s'),
      datefmt='%Y%m%d %H:%M:%S',
      level=logging.INFO)
  run(flags)
