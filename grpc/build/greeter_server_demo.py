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

"""The Python implementation of the GRPC helloworld.Greeter server."""

from concurrent import futures
import argparse
import logging
import os
import time

import grpc

import greeter_pb2
import greeter_pb2_grpc
import translator_pb2
import translator_pb2_grpc

_ONE_DAY_IN_SECONDS = 60 * 60 * 24


class Greeter(greeter_pb2_grpc.GreeterServicer):
  def __init__(self, channel):
    super(Greeter, self).__init__()
    logging.info('Creating Translator stub.')
    self._translator = translator_pb2_grpc.TranslatorStub(channel)

  def SayHello(self, request, context):
    prefix = 'Hello'
    t_request = translator_pb2.TranslationRequest(message=prefix,
                                                  locale=request.locale)
    try:
      # Propagate the deadline if set, otherwise use the default.
      t_timeout = context.time_remaining()
      if context.time_remaining() >= 2**32-1:
        t_timeout = flags.deadline_ms/1000.0
        logging.info('Using default deadline %s seconds', t_timeout)
      t_response = self._translator.Translate(t_request, timeout=t_timeout)
      prefix = t_response.translation
    except grpc.RpcError:
      logging.exception('Failed to fetch translation:')
    return greeter_pb2.HelloReply(message='%s, %s!' % (prefix, request.name))

  def ManyHellos(self, request_iterator, context):
    prefix = "Hello"
    ok = False

    for request in request_iterator:
      ok = True  # At least one actual request in stream
      logging.info('Received request: %r', request)

      t_request = translator_pb2.AllTranslationsRequest(
          message=prefix, locales=[request.locale])
      found = False

      # No deadline on the outbound call, because it's streaming.
      for t_reply in self._translator.AllTranslations(t_request):
        found = True  # Message is known to the translator
        if not context.is_active():
          # Client cancelled or disconnected, stop doing work.
          return
        # Otherwise, send back what we have so far:
        yield greeter_pb2.HelloReply(
            message='%s, %s!' % (t_reply.translation, request.name))

      if not found:
        error_message = (
            'No translations found for "Hello" in locales matching %s' %
            (request.locale,))
        # Could log this explicitly, but the grpc framework does that anyway.
        raise grpc.RpcError(grpc.StatusCode.ABORTED, error_message)

    # API specific: at least one request was expected.
    if not ok:
      raise grpc.RpcError(grpc.StatusCode.FAILED_PRECONDITION,
                          'No requests received')


def serve(flags):
  logging.info('Creating channel to translation server at %s',
               flags.translation_server)
  translator = grpc.insecure_channel(flags.translation_server)
  server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
  greeter_pb2_grpc.add_GreeterServicer_to_server(Greeter(translator), server)
  server.add_insecure_port('[::]:%d' % flags.port)
  server.start()
  logging.info('Server listening on port %d', flags.port)
  try:
    while True:
      time.sleep(_ONE_DAY_IN_SECONDS)
  except KeyboardInterrupt:
    server.stop(0)


if __name__ == '__main__':
  parser = argparse.ArgumentParser(description='The greeter server.')
  parser.add_argument('--port', type=int, default=50051,
                      help='Port on which to listen.')
  parser.add_argument('--translation_server', default='localhost:50061',
                      help='Server address of the translation server.')
  parser.add_argument('--deadline_ms', type=int, default=20*1000,
                      help="Default deadline in milliseconds.")
  parser.add_argument('--log_dir', default='',
                      help='Where to write the logfile.')
  flags = parser.parse_args()
  logging.basicConfig(
      filename=os.path.join(flags.log_dir, 'greeter_server.py.INFO'),
      format=('%(asctime)s.%(msecs)d %(levelname)s %(module)s:%(lineno)s '
              '%(message)s'),
      datefmt='%Y%m%d %H:%M:%S',
      level=logging.INFO)
  if flags.port < 1025 or flags.port > 65000:
    logging.critical('--port must be between 1024 and 65000 (not %d)',
                     flags.port)
    sys.exit(1)
  serve(flags)
