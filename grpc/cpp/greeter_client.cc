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
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <grpc++/grpc++.h>

#include "greeter.grpc.pb.h"

DEFINE_string(user, "world", "The user to greet!");
DEFINE_string(greeter_server, "localhost:50051",
              "Server address of the greeter server.");
DEFINE_string(locale, "en_US", "The locale for the greeting.");
DEFINE_int32(deadline_ms, 20*1000, "Deadline in milliseconds.");
DEFINE_bool(wait_for_ready, true,
            "Whether to wait for the backend to become available. "
            "If false, fails fast.");
DEFINE_bool(streaming, false, "Whether to use the streaming API.");

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReaderWriter;
using grpc::Status;
using srecon::HelloRequest;
using srecon::HelloReply;
using srecon::Greeter;

class GreeterClient {
 public:
  explicit GreeterClient(std::shared_ptr<Channel> channel)
      : stub_(Greeter::NewStub(channel)) {}

  // Assembles the client's payload, sends it and presents the response back
  // from the server.
  std::string SayHello(const std::string& user) {
    // Data we are sending to the server.
    HelloRequest request;
    request.set_name(user);
    request.set_locale(FLAGS_locale);

    // Container for the data we expect from the server.
    HelloReply reply;

    // Context for the client. It can be used to convey extra information to
    // the server and/or tweak certain RPC behaviors. Here, we set a deadline.
    ClientContext context;
    context.set_wait_for_ready(FLAGS_wait_for_ready);
    auto deadline =
        std::chrono::system_clock::now() +
        std::chrono::milliseconds(FLAGS_deadline_ms);
    context.set_deadline(deadline);
    auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - std::chrono::system_clock::now());
    LOG(INFO) << "Deadline set to " << delta.count() << "ms from now.";

    // The actual RPC.
    Status status = stub_->SayHello(&context, request, &reply);

    delta = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - std::chrono::system_clock::now());
    LOG(INFO) << "Deadline remaining: " << delta.count() << "ms, status: "
              << (status.ok() ? "OK" : status.error_message());

    // Act upon its status.
    if (status.ok()) {
      return reply.message();
    } else {
      LOG(ERROR) << "Error code " << status.error_code() << ": "
                 << status.error_message() << std::endl;
      return "*** RPC failed ***";
    }
  }

  std::vector<std::string> AllTheHellos(const std::string& user) {
    std::vector<std::string> replies;

    // Create all the requests we are going to send.
    std::vector<HelloRequest> requests;
    {
      HelloRequest req;
      req.set_name(user);

      auto separator = FLAGS_locale.find("_");
      if (separator == FLAGS_locale.npos) {
        req.set_locale(FLAGS_locale);
        requests.push_back(req);
      } else {
        req.set_locale(std::string{FLAGS_locale, 0, separator});
        requests.push_back(req);
        req.set_locale(std::string{FLAGS_locale, separator+1});
        requests.push_back(req);
      }
    }

    // Context for the client, as for all RPCs.
    // Carries a deadline; if the server is slow, we'll take a partial result.
    ClientContext context;
    context.set_wait_for_ready(FLAGS_wait_for_ready);
    auto deadline =
        std::chrono::system_clock::now() +
        std::chrono::milliseconds(FLAGS_deadline_ms);
    context.set_deadline(deadline);
    auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - std::chrono::system_clock::now());
    LOG(INFO) << "Deadline set to " << delta.count() << "ms from now.";

    // Set up the bidirectional stream
    std::shared_ptr<ClientReaderWriter<HelloRequest, HelloReply>>
        stream(stub_->ManyHellos(&context));

    // Write in a separate thread, to avoid deadlock.
    // For <= 2 requests, probably unnecessary, because they will fit in the
    // buffers, but since there is no dependency between the read and write
    // streams in the example, this is the easiest safe solution.
    std::thread writer([stream, &requests]() {
      for (const auto& req : requests) {
        LOG(INFO) << "Requesting locale \"" << req.locale() << "\".";
        stream->Write(req);
      }
      stream->WritesDone();
    });

    // Collect all the replies.
    HelloReply reply;
    while (stream->Read(&reply)) {
      replies.emplace_back(std::move(reply.message()));
    }

    // Clean up the thread.
    writer.join();

    // Determine whether the RPC was a complete success, and handle that.
    Status status = stream->Finish();
    delta = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - std::chrono::system_clock::now());
    LOG(INFO) << "Deadline remaining: " << delta.count() << "ms, status: "
              << (status.ok() ? "OK" : status.error_message());
    if (!status.ok()) {
      std::string error_message =
          "*** RPC failed *** error code " +
          std::to_string(status.error_code()) + ": " +
          status.error_message();
      LOG(ERROR) << error_message;
      replies.emplace_back(std::move(error_message));
    }
    return replies;
  }

 private:
  std::unique_ptr<Greeter::Stub> stub_;
};

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  std::string user = (argc > 1 ? argv[1] : FLAGS_user);

  // Instantiate the client. It requires a channel, out of which the actual RPCs
  // are created. This channel models a connection to an endpoint (in this case,
  // by default localhost at port 50051). We indicate that the channel isn't
  // authenticated (use of InsecureChannelCredentials()).
  GreeterClient greeter(grpc::CreateChannel(
      FLAGS_greeter_server, grpc::InsecureChannelCredentials()));

  if (FLAGS_streaming) {
    std::vector<std::string> replies = greeter.AllTheHellos(user);
    LOG(INFO) << "Received " << replies.size() << " replies";
    for (const std::string& greeting : replies) {
      std::cout << "Greeting received: " << greeting << std::endl;
    }
  } else {
    std::string reply = greeter.SayHello(user);
    LOG(INFO) << "Greeting received: " << reply;
    std::cout << "Greeting received: " << reply << std::endl;
  }

  return 0;
}
