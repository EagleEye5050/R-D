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

BUILDDIR = build
LOGDIR = logs

.PHONY: all clean realclean

default:
	@echo There is no default build target. 1>&2
	@exit 1

all: cpp go python

clean:
	@echo Cleaning $(BUILDDIR)/
	@make -C cpp -w BUILDDIR=../$(BUILDDIR) clean
	@make -C python -w BUILDDIR=../$(BUILDDIR) clean
	@make -C golang -w BUILDDIR=../$(BUILDDIR) clean
	@make -C java -w BUILDDIR=../$(BUILDDIR) clean
	@rmdir $(BUILDDIR) 2>/dev/null || /bin/true

realclean: clean
	@echo Cleaning $(LOGDIR)/
	@rm -f $(LOGDIR)/*.INFO.* $(LOGDIR)/*.WARNING.* $(LOGDIR)/*.ERROR.* $(LOGDIR)/*.FATAL.*
	@rm -f $(LOGDIR)/*.INFO $(LOGDIR)/*.WARNING $(LOGDIR)/*.ERROR $(LOGDIR)/*.FATAL
	@rm -f ./*.INFO ./*.WARNING ./*.ERROR ./*.FATAL
	@rm -f $(LOGDIR)/*.STDOUT $(LOGDIR)/*.STDERR
	@rm -f ./*.STDOUT ./*.STDERR

# Building for the various languages.

.PHONY: cpp go golang py python java

cpp: $(BUILDDIR)
	@make -C cpp -w BUILDDIR=../$(BUILDDIR)

golang: go

go: $(BUILDDIR)
	@make -C golang -w BUILDDIR=../$(BUILDDIR)

py: python

python: $(BUILDDIR)
	@make -C python -w BUILDDIR=../$(BUILDDIR)

java: $(BUILDDIR)
	@make -C java -w BUILDDIR=../$(BUILDDIR)

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)
