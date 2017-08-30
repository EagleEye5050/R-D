/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <csignal>
#include <iostream>
#include <memory>
#include <string>

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <grpc++/grpc++.h>

#include "greeter.grpc.pb.h"
#include "translator.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReaderWriter;
using grpc::Status;

// Available as global "int FLAGS_port"
DEFINE_int32(port, 50051, "Port on which to listen.");

// Available as global "std::string FLAGS_translation_server"
DEFINE_string(translation_server, "localhost:50061",
              "Server address of the translation server.");

// Exercise 3: Default deadline in ms.
DEFINE_int32(deadline_ms, 20*1000, "Default deadline in milliseconds.");

namespace srecon {

// Logic and data behind the server's behavior.
class GreeterServiceImpl final : public Greeter::Service {
 public:
  // EXERCISE 1:
  // Add Translator stub.
  GreeterServiceImpl() = default;

  Status SayHello(ServerContext* context, const HelloRequest* request,
                  HelloReply* reply) override {
    // EXERCISE 1:
    // Translate the prefix using Translator backend.
    std::string prefix("Hello");

    reply->set_message(prefix + ", " + request->name() + "!");
    return Status::OK;
  }

  // EXERCISE 5:
  // Implement the streaming call.
  Status ManyHellos(ServerContext* context,
                    ServerReaderWriter<HelloReply, HelloRequest>* stream) override {
    LOG(ERROR) << "Streaming call is unimplemented.";
    return Status(grpc::UNIMPLEMENTED, "Streaming call is unimplemented.");
  }

 private:
  // Add whatever you need.
};

}  // namespace srecon

// Trivial termination handler. Not guaranteed to be safe (it is not reentrant),
// but good enough for demonstration purposes.
grpc::Server* greeter_server = nullptr;
void handle_sigterm(int) {
  LOG(INFO) << "Received SIGTERM, shutting down.";
  if (greeter_server) {
    greeter_server->Shutdown();
    greeter_server = nullptr;
  }
}

void RunServer(const std::string& server_address) {
  srecon::GreeterServiceImpl service;

  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  greeter_server = server.get();  // For the signal handler.

  LOG(INFO) << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  if (FLAGS_port < 1025 || FLAGS_port > 65000) {
    LOG(FATAL) << "--port must be between 1024 and 65000";  // Crash ok
  }
  std::string server_address("0.0.0.0:");
  server_address += std::to_string(FLAGS_port);

  std::signal(SIGTERM, handle_sigterm);
  RunServer(server_address);

  return 0;
}
