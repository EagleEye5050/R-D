#
# Copyright 2017, Google Inc.
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
#

BUILDDIR=../build

PYTHON = python
PROTOC = protoc
GRPC_PY_PLUGIN = grpc_python_plugin
GRPC_PY_PLUGIN_PATH ?= `which $(GRPC_PY_PLUGIN)`

PROTOS_PATH = ../protos
vpath %.proto $(PROTOS_PATH)

EXECUTABLES = greeter_client greeter_server greeter_server_demo
PY_EXECUTABLES = $(patsubst %,$(BUILDDIR)/%.py,$(EXECUTABLES) )

.PHONY: all builddir clean $(EXECUTABLES)

all: builddir $(PY_EXECUTABLES)

builddir:
	@mkdir -p $(BUILDDIR)

greeter_client: $(BUILDDIR)/greeter_client.py
GREETER_CLIENT_DEPS = greeter_pb2.py greeter_pb2_grpc.py

greeter_server: $(BUILDDIR)/greeter_server.py
GREETER_SERVER_DEPS = greeter_pb2.py  greeter_pb2_grpc.py translator_pb2.py translator_pb2_grpc.py

greeter_server_demo: $(BUILDDIR)/greeter_server_demo.py

$(BUILDDIR)/greeter_client.py: greeter_client.py $(patsubst %,$(BUILDDIR)/%, $(GREETER_CLIENT_DEPS))
	install -m 0755 $< $@

$(BUILDDIR)/greeter_server.py: greeter_server.py $(patsubst %,$(BUILDDIR)/%, $(GREETER_SERVER_DEPS))
	install -m 0755 $< $@

$(BUILDDIR)/greeter_server_demo.py: greeter_server_demo.py $(patsubst %,$(BUILDDIR)/%, $(GREETER_SERVER_DEPS))
	install -m 0755 $< $@

.PRECIOUS: $(BUILDDIR)/%_pb2.py
$(BUILDDIR)/%_pb2.py: %.proto
	$(PROTOC) -I $(PROTOS_PATH) --python_out=$(BUILDDIR) $<

.PRECIOUS: $(BUILDDIR)/%_pb2_grpc.py
$(BUILDDIR)/%_pb2_grpc.py: %.proto
	$(PROTOC) -I $(PROTOS_PATH) --python_out=$(BUILDDIR) --grpc_out=$(BUILDDIR) --plugin=protoc-gen-grpc=$(GRPC_PY_PLUGIN_PATH) $<

clean:
	@echo Cleaning Python Build...
	@rm -f $(patsubst %,$(BUILDDIR)/%,*.pyc *_pb2.py *_pb2_grpc.py greeter_client.py greeter_server.py greeter_server_demo.py)
	@rmdir $(BUILDDIR) 2>/dev/null || /bin/true

