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

CXX = clang++
CPPFLAGS += -I/usr/local/include -I $(BUILDDIR) -pthread
CXXFLAGS += -std=c++11
LDFLAGS += -L/usr/local/lib `pkg-config --libs grpc++ grpc`       \
           -Wl,--no-as-needed -lgrpc++_reflection -Wl,--as-needed \
           -lglog -lgflags -lprotobuf -lpthread -ldl
PROTOC = protoc
GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`

PROTOS_PATH = ../protos
vpath %.proto $(PROTOS_PATH)

EXECUTABLES = greeter_client greeter_server greeter_server_demo translation_server exerciser
CPP_EXECUTABLES = $(patsubst %,$(BUILDDIR)/%,$(EXECUTABLES) )

vpath %.cc .

.PHONY: all builddir clean $(EXECUTABLES)

all: builddir $(CPP_EXECUTABLES)

builddir:
	@mkdir -p $(BUILDDIR)

greeter_client: $(BUILDDIR)/greeter_client

greeter_server: $(BUILDDIR)/greeter_server

greeter_server_demo: $(BUILDDIR)/greeter_server_demo

translation_server: $(BUILDDIR)/translation_server

exerciser: $(BUILDDIR)/exerciser

GREETER_CLIENT = greeter.pb.o greeter.grpc.pb.o greeter_client.o
$(BUILDDIR)/greeter_client: $(patsubst %,$(BUILDDIR)/%,$(GREETER_CLIENT))
	$(CXX) $^ $(LDFLAGS) -o $@

GREETER_SERVER = greeter.pb.o greeter.grpc.pb.o translator.pb.o translator.grpc.pb.o greeter_server.o
$(BUILDDIR)/greeter_server: $(patsubst %,$(BUILDDIR)/%,$(GREETER_SERVER))
	$(CXX) $^ $(LDFLAGS) -o $@

GREETER_SERVER_DEMO = greeter.pb.o greeter.grpc.pb.o translator.pb.o translator.grpc.pb.o greeter_server_demo.o
$(BUILDDIR)/greeter_server_demo: $(patsubst %,$(BUILDDIR)/%,$(GREETER_SERVER_DEMO))
	$(CXX) $^ $(LDFLAGS) -o $@

TRANSLATION_SERVER = translator.pb.o translator.grpc.pb.o control.pb.o control.grpc.pb.o translation_behaviour.o translation_control.o translation_server.o
$(BUILDDIR)/translation_server: $(patsubst %,$(BUILDDIR)/%,$(TRANSLATION_SERVER))
	$(CXX) $^ $(LDFLAGS) -o $@

EXERCISER = control.pb.o control.grpc.pb.o greeter.pb.o greeter.grpc.pb.o exerciser.o
$(BUILDDIR)/exerciser: $(patsubst %,$(BUILDDIR)/%,$(EXERCISER))
	$(CXX) $^ $(LDFLAGS) -o $@

.PRECIOUS: $(BUILDDIR)/%.grpc.pb.cc
$(BUILDDIR)/%.grpc.pb.cc: %.proto
	$(PROTOC) -I $(PROTOS_PATH) --grpc_out=$(BUILDDIR) --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $<

.PRECIOUS: $(BUILDDIR)/%.pb.cc
$(BUILDDIR)/%.pb.cc: %.proto
	$(PROTOC) -I $(PROTOS_PATH) --cpp_out=$(BUILDDIR) $<

$(BUILDDIR)/%.o: %.cc
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $^

clean:
	@echo Cleaning C++ Build...
	@rm -f $(patsubst %,$(BUILDDIR)/%,*.o *.pb.cc *.pb.h $(EXECUTABLES))
	@rmdir $(BUILDDIR) 2>/dev/null || /bin/true
