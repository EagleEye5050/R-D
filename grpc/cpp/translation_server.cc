/*
 *
 * Copyright 2017, Google Inc.
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
#include <map>
#include <memory>
#include <string>

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <grpc++/grpc++.h>

#include "translator.grpc.pb.h"

// Error injection and control API:
#include "translation_behaviour.h"
#include "translation_control.h"

DEFINE_int32(port, 50061, "Port on which to listen.");

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerWriter;
using grpc::Status;

using google::INFO;
using google::WARNING;
using google::ERROR;

namespace srecon {

static std::map<std::string, std::map<std::string, std::string>> kTransDB =
{
  {"An error occurred",
      {{"en_GB", "Pardon me all to hell"},
       {"en_US", "Oops, my bad"},
       {"de_DE", "Ein Fehler ist aufgetreten"}}},
  {"Hello",
      {{"en_GB", "How do you do"},
       {"en_US", "Word up"},
       {"de_DE", "Guten Tag"},
       {"de_CH", "Grüezi"},
       {"fr_CH", "Âllo"}}},
  {"Goodbye",
      {{"en_GB", "Toodle pip"},
       {"en_US", "Smell you later"},
       {"de_DE", "Tschüß"}}}
};

// Logic behind the server's behavior.
class TranslationServiceImpl final : public Translator::Service {
 public:
  explicit TranslationServiceImpl(ExpectedBehaviour* behaviour)
      : Translator::Service(), behaviour_(behaviour) {}

 protected:
  Status Translate(ServerContext* context, const TranslationRequest* request,
                   TranslationReply* reply) override {
    if (request->locale().empty()) {
      LOG(WARNING) << "Received request with no locale.";
      return Status(grpc::INVALID_ARGUMENT, "No locale set.");
    }

    auto message = kTransDB.find(request->message());
    if (message == kTransDB.end()) {
      LOG_EVERY_N(INFO, 10) << "Received request for unknown message.";
      return Status(grpc::NOT_FOUND, "Message text unknown");
    }

    const auto& by_locale = message->second;
    const auto translation = by_locale.find(request->locale());
    if (translation == by_locale.end()) {
      LOG(INFO) << "Cannot translate message \"" << request->message()
                << "\" into locale \"" << request->locale() << "\"";
      return Status(grpc::NOT_FOUND,
                    request->message() + " untranslatable to " +
                    request->locale());
    }
    auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(
        context->deadline() - std::chrono::system_clock::now());
    LOG(INFO) << "Received translation request ["
              << request->ShortDebugString() << "], with deadline "
              << delta.count() << "ms from now.";
    reply->set_translation(translation->second);
    return behaviour_->BehaveUnary();  // May exceed deadline
  }

  Status AllTranslations(ServerContext* context,
                         const AllTranslationsRequest* request,
                         ServerWriter<AllTranslationsReply>* writer) override {
    auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(
        context->deadline() - std::chrono::system_clock::now());
    LOG(INFO) << "Received translation stream request ["
              << request->ShortDebugString() << "], with deadline "
              << delta.count() << "ms from now.";
    bool found = false;

    Status result;

    for (const auto& by_message : kTransDB) {
      const std::string& message = by_message.first;
      if (!request->message().empty() && request->message() != message) {
        continue;
      }

      for (const auto& by_locale : by_message.second) {
        const std::string& locale = by_locale.first;
        const std::string& translation = by_locale.second;
        if (request->locales_size() == 0 ||
            std::any_of(request->locales().begin(), request->locales().end(),
                        [&locale](const std::string& l) {
                          return locale.find(l) != locale.npos;
                        })) {
          found = true;
          AllTranslationsReply reply;
          reply.set_message(message);
          reply.set_locale(locale);
          reply.set_translation(translation);
          result = behaviour_->BehaveStream();
          if (!result.ok()) {
            return result;
          }
          writer->Write(reply);
        }
      }
    }

    if (!found) {
      return Status(grpc::NOT_FOUND, "Nothing matched the request");
    }

    return Status::OK;
  }

 private:
  ExpectedBehaviour* behaviour_;
};

}  // namespace srecon

// Trivial termination handler. Not guaranteed to be safe (it is not reentrant),
// but good enough for demonstration purposes.
grpc::Server* translation_server = nullptr;
void handle_sigterm(int) {
  LOG(INFO) << "Received SIGTERM, shutting down.";
  if (translation_server) {
    translation_server->Shutdown();
    translation_server = nullptr;
  }
}

void RunServer(const std::string& server_address) {
  srecon::ExpectedBehaviour injected;
  srecon::TranslatorControlImpl behaviour_service(&injected);
  srecon::TranslationServiceImpl service(&injected);

  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register both services.
  builder.RegisterService(&service);
  builder.RegisterService(&behaviour_service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  translation_server = server.get();  // For the signal handler.

  LOG(INFO) << "Translation Service listening on " << server_address
            << std::endl;

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
