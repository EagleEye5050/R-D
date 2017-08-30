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

GO = go
PROTOC = protoc
GRPC_GO_PLUGIN = protoc-gen-go
GRPC_GO_PLUGIN_PATH = /usr/local/lib/go/bin/$(GRPC_GO_PLUGIN)
GOPATH = /usr/local/lib/go:$(shell pwd)

PROTOS_PATH = ../protos
vpath %.proto $(PROTOS_PATH)

EXECUTABLES = greeter_client greeter_server greeter_server_demo

.PHONY: all builddir clean $(EXECUTABLES)

all: builddir $(EXECUTABLES)

builddir:
	@mkdir -p $(BUILDDIR)

greeter_client: $(BUILDDIR)/go_greeter_client

greeter_server: $(BUILDDIR)/go_greeter_server

greeter_server_demo: $(BUILDDIR)/go_greeter_server_demo

$(BUILDDIR)/go_greeter_client: src/greeter_client/main.go src/greeter/proto.pb.go
	GOPATH=$(GOPATH) $(GO) build -o $@ $<

$(BUILDDIR)/go_greeter_server: src/greeter_server/main.go src/greeter/proto.pb.go src/translator/proto.pb.go
	GOPATH=$(GOPATH) $(GO) build -o $@ $<

$(BUILDDIR)/go_greeter_server_demo: src/greeter_server_demo/main.go src/greeter/proto.pb.go src/translator/proto.pb.go
	GOPATH=$(GOPATH) $(GO) build -o $@ $<

clean:
	@echo Cleaning Go Build...
	@rm -f src/*/*.pb.go 2>/dev/null || true
	@rmdir src/{greeter,translator} 2>/dev/null || true
	@rm -f $(patsubst %,$(BUILDDIR)/go_%,greeter_client greeter_server greeter_server_demo)
	@rmdir $(BUILDDIR) 2>/dev/null || /bin/true

# This section is funkier than normal projects would have it:
# Running protoc is required to generate the Go files, but the subdirectory and
# renaming dance is an artefact of the mixed-language workshop.
.PRECIOUS: src/%/proto.pb.go
src/%/proto.pb.go: %.proto
	@mkdir -p src/$*
	$(PROTOC) -I $(PROTOS_PATH) --go_out=plugins=grpc:src/$* --plugin=protoc-gen-go=$(GRPC_GO_PLUGIN_PATH) $<
	@mv src/$*/$*.pb.go src/$*/proto.pb.go
