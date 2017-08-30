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

#include <chrono>
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

DEFINE_int32(port, 50051, "Port on which to listen.");
DEFINE_string(translation_server, "localhost:50061",
              "Server address of the translation server.");
DEFINE_int32(deadline_ms, 20*1000, "Default deadline in milliseconds.");

namespace srecon {

// Logic and data behind the server's behavior.
class GreeterServiceImpl final : public Greeter::Service {
 public:
  explicit GreeterServiceImpl(std::shared_ptr<Channel> channel)
      : stub_(Translator::NewStub(channel)) {}

  Status SayHello(ServerContext* context, const HelloRequest* request,
                  HelloReply* reply) override {
    std::string prefix("Hello");

    TranslationRequest t_request;
    t_request.set_message(prefix);
    t_request.set_locale(request->locale());
    TranslationReply t_reply;
    // Propagate deadline.
    // Note: This will not explicitly set the "deadline" property of the client
    // context; the client context retains a pointer to the parent context,
    // and gRPC will use that for the propagated values.
    std::unique_ptr<ClientContext> t_context =
        ClientContext::FromServerContext(*context);
    t_context->set_wait_for_ready(false);
    // Set the default deadline if not set by the client.
    if (context->deadline() ==
        std::chrono::system_clock::time_point::max()) {
      auto default_deadline =
        std::chrono::system_clock::now() +
        std::chrono::milliseconds(FLAGS_deadline_ms);
      LOG(INFO) << "Default deadline was set.";
    t_context->set_deadline(default_deadline);
    }

    auto start_time = std::chrono::system_clock::now();
    Status status = stub_->Translate(t_context.get(), t_request, &t_reply);
    auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - start_time);
    LOG(INFO) << "Call to Translator Backend took " << delta.count() << "ms.";

    if (status.ok()) {
      prefix = t_reply.translation();
    } else {
      LOG(ERROR) << "Translator backend failed, error code "
                 << status.error_code()
                 << ", message: " << status.error_message()
                 << " (returning default to caller).";
    }

    reply->set_message(prefix + ", " + request->name() + "!");
    return Status::OK;
  }

  Status ManyHellos(ServerContext* context,
                    ServerReaderWriter<HelloReply, HelloRequest>* stream) override {
    bool ok = false;
    HelloRequest request;
    while (stream->Read(&request)) {
      ok = true;
      LOG_EVERY_N(INFO, 10) << "Received request: " << request.DebugString();
      AllTranslationsRequest t_request;
      t_request.set_message("Hello");
      t_request.add_locales(request.locale());

      // The outgoing call needs a ClientContext. However, this is a streaming
      // call, so setting the deadline makes little sense.
      std::unique_ptr<ClientContext> t_context =
          ClientContext::FromServerContext(*context);
      auto start_time = std::chrono::system_clock::now();
      auto t_stream = stub_->AllTranslations(t_context.get(), t_request);

      bool found = false;
      AllTranslationsReply t_reply;
      while (t_stream->Read(&t_reply)) {
        auto read_time = std::chrono::system_clock::now();
        auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(
            read_time - start_time);
        start_time = read_time;
        LOG(INFO) << "Streaming call to Translator Backend received a reply "
                  << "after " << delta.count() << "ms.";
        found = true;
        HelloReply reply;
        reply.set_message(t_reply.translation() + ", " + request.name() + "!");
        // Check whether the client still cares (don't so work if, say, their
        // caller's deadline has expired):
        if (context->IsCancelled()) {
          LOG(INFO) << "Deadline exceeded or Client cancelled, abandoning.";
          return Status::CANCELLED;
        }
        // Otherwise, send back what we have so far:
        LOG_EVERY_N(INFO, 10) << "Sending back " << reply.DebugString();
        stream->Write(reply);
      }
      Status t_status = t_stream->Finish();
      if (!t_status.ok()) {
        return t_status;
      }
      if (!found) {
        std::string error_message =
            "No translations found for \"Hello\" in locales matching \"" +
            request.locale() + "\"";
        return Status(grpc::ABORTED, error_message);
      }
    }
    if (ok) {
      return Status::OK;
    } else {
      return Status(grpc::FAILED_PRECONDITION, "No requests received");
    }
  }

 private:
  std::unique_ptr<Translator::Stub> stub_;
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
  srecon::GreeterServiceImpl service(grpc::CreateChannel(
      FLAGS_translation_server, grpc::InsecureChannelCredentials()));

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
