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

#include <cassert>
#include <vector>
#include <memory>
#include <string>

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <google/protobuf/text_format.h>
#include <grpc++/grpc++.h>

#include "control.grpc.pb.h"
#include "greeter.grpc.pb.h"

DEFINE_string(greeter_server, "localhost:50051",
              "Server address of the greeter server.");
DEFINE_string(translation_server, "localhost:50061",
              "Server address of the translation server.");
DEFINE_int32(exercise, 0,
             "Which Exercise to test.");

namespace srecon {

// The test cases to be run, by exercise.

struct UnaryTestCase {
  const char* description;
  const char* request;    // HelloRequest.
  int deadline_ms;        // Deadline in ms (0 means none)
  const char* behaviour;  // Behaviour triggered.
  grpc::StatusCode code;  // Expected outcome
  const char* expected;   // Expected HelloReply if code == OK.
};

struct StreamTestCase {
  const char* description;
  int deadline_ms;
  const std::vector<std::string> requests;
  const std::vector<std::string> behaviours;
  const grpc::StatusCode code;
  const std::vector<std::string> expected;
};

static const std::vector<std::vector<struct UnaryTestCase>> unary_testcases{
  // Exercise 0: There is no exercise 0.
  {},
  // Exercise 1: Add a new gRPC backend.
  // Well-behaved, expect correct replies.
  {
    {.description = "en_US translation requested",
     .request = "name:\"SREcon attendee\" locale:\"en_US\"",
     .behaviour = "result: OK",
     .code = grpc::OK,
     .expected = "message:\"Word up, SREcon attendee!\""},
    {.description = "en_GB translation requested",
     .request = "name:\"SREcon attendee\" locale:\"en_GB\"",
     .behaviour = "result: OK",
     .code = grpc::OK,
     .expected = "message:\"How do you do, SREcon attendee!\""},
    {.description = "de_DE translation requested for a different name",
     .request = "name:\"Bob\" locale:\"de_DE\"",
     .behaviour = "result: OK",
     .code = grpc::OK,
     .expected = "message:\"Guten Tag, Bob!\""},
  },
  // Exercise 2: Client Deadlines and Server Timeouts
  // Very slow (but present) translation server.
  {
    {.description = "No client deadline, server faster than default deadline: "
                    "Expect OK, just slow",
     .request = "name:\"SREcon attendee\" locale:\"en_US\"",
     .behaviour = "result: OK jitter { mean_ms: 5000 }",
     .code = grpc::OK,
     .expected = "message:\"Word up, SREcon attendee!\""},
    {.description = "No client deadline, server slower than default deadline: "
                    "Expect default reply",
     .request = "name:\"SREcon attendee\" locale:\"en_US\"",
     .behaviour = "result: OK jitter { mean_ms: 25000 stddev_ms: 0 }",
     .code = grpc::OK,
     .expected = "message:\"Hello, SREcon attendee!\""},
    {.description = "Client deadline overrides default deadline: "
                    "Expect OK, just slow",
     .request = "name:\"SREcon attendee\" locale:\"en_US\"",
     .deadline_ms = 30 * 1000,
     .behaviour = "result: OK jitter { mean_ms: 25000 }",
     .code = grpc::OK,
     .expected = "message:\"Word up, SREcon attendee!\""},
    {.description = "Client deadline overrides default deadline: "
                    "Expect deadline_exceeded",
     .request = "name:\"SREcon attendee\" locale:\"en_US\"",
     .deadline_ms = 1000,
     .behaviour = "result: OK jitter { mean_ms: 5000 }",
     .code = grpc::DEADLINE_EXCEEDED,
     .expected = ""},
  },
  // Exercise 3: Backend Disappears.
  // (no behaviour): expect default response.
  {
    {.description = "Translation server unreachable: Expect default",
     .request = "name:\"SREcon attendee\" locale:\"en_US\"",
     .deadline_ms = 1000,
     .code = grpc::OK,
     .expected = "message:\"Hello, SREcon attendee!\""},
  },
  // Exercise 4: Be cheap and be helpful
  {
    {.description = "Requested locale unknown: Expect default",
     .request = "name:\"SREcon attendee\" locale:\"cn_US\"",
     .behaviour = "result: NOT_FOUND jitter { mean_ms: 1000 stddev_ms: 200 }",
     .deadline_ms = 5000,
     .code = grpc::DO_NOT_USE,
     .expected = "message:\"Hello, SREcon attendee!\""},
    {.description = "Potential timeout",
     .request = "name:\"SREcon attendee\" locale:\"en_GB\"",
     .behaviour = "result: OK jitter { mean_ms: 1000 stddev_ms: 200 }",
     .deadline_ms = 1000,
     .code = grpc::DO_NOT_USE,
     .expected = "message:\"How do you do, SREcon attendee!\""},
    {.description = "Potential timeout",
     .request = "name:\"SREcon attendee\" locale:\"en_GB\"",
     .behaviour = "result: OK jitter { mean_ms: 1000 stddev_ms: 200 }",
     .deadline_ms = 1000,
     .code = grpc::DO_NOT_USE,
     .expected = "message:\"How do you do, SREcon attendee!\""},
    {.description = "Potential timeout",
     .request = "name:\"SREcon attendee\" locale:\"en_GB\"",
     .behaviour = "result: OK jitter { mean_ms: 1000 stddev_ms: 200 }",
     .deadline_ms = 1000,
     .code = grpc::DO_NOT_USE,
     .expected = "message:\"How do you do, SREcon attendee!\""},
    {.description = "Potential timeout",
     .request = "name:\"SREcon attendee\" locale:\"en_GB\"",
     .behaviour = "result: OK jitter { mean_ms: 1000 stddev_ms: 200 }",
     .deadline_ms = 1000,
     .code = grpc::DO_NOT_USE,
     .expected = "message:\"How do you do, SREcon attendee!\""},
  },
  // Exercise 5: BiDi Streaming, Client and Server: No Unary tests
  {},
  // Exercise 6: Streaming Timeouts and Other Amusements: No Unary tests
  {},
};

static const std::vector<std::vector<struct StreamTestCase>> stream_testcases{
  // Exercises 0-4 have no streaming support.
  {},
  {},
  {},
  {},
  {},
  // Exercise 5: BiDi Streaming
  {
    {
     .description = "Trivial Stream-based implementation of Unary call",
     .requests = { "name:\"SREcon attendee\" locale:\"en_US\"" },
     .behaviours = { "result: OK" },
     .code = grpc::OK,
     .expected = { "message:\"Word up, SREcon attendee!\"" }
    },
    {
     .description = "Simple stream equivalent of multiple Unary calls",
     .requests = {
       "name:\"SREcon attendee\" locale:\"en_US\"",
       "name:\"SREcon attendee\" locale:\"en_GB\"",
       "name:\"SREcon attendee\" locale:\"de_CH\"",
     },
     .behaviours = {
       "result: OK",
       "result: OK",
       "result: OK",
     },
     .code = grpc::OK,
     .expected = {
       "message:\"Word up, SREcon attendee!\"",
       "message:\"How do you do, SREcon attendee!\"",
       "message:\"Grüezi, SREcon attendee!\"",
     }
    },
    {
     .description = "Multiple replies",
     .requests = {
       "name:\"SREcon attendee\" locale:\"en\"",
       "name:\"SREcon attendee\" locale:\"CH\"",
     },
     .behaviours = {
       "result: OK",
       "result: OK",
       "result: OK",
       "result: OK",
     },
     .code = grpc::OK,
     .expected = {
       "message:\"How do you do, SREcon attendee!\"",
       "message:\"Word up, SREcon attendee!\"",
       "message:\"Grüezi, SREcon attendee!\"",
       "message:\"Âllo, SREcon attendee!\"",
     }
    },
  },
  // Exercise 6: Streaming Timeouts and Other Amusements.
  {
    {.description = "Broken Stream",
     .requests = { "name:\"SREcon attendee\" locale:\"en\"" },
     .behaviours = {
       "result: OK",
       "result: UNKNOWN",
     },
     .code = grpc::DO_NOT_USE,
     .expected = {
       "message:\"How do you do, SREcon attendee!\"",
     }
    },
    {.description = "Broken Stream",
     .requests = {
       "name:\"SREcon attendee\" locale:\"en\"",
       "name:\"SREcon attendee\" locale:\"CH\"",
     },
     .behaviours = {
       "result: OK",
       "result: OK",
       "result: OK",
       "result: UNKNOWN",
     },
     .code = grpc::UNKNOWN,
     .expected = {
       "message:\"How do you do, SREcon attendee!\"",
       "message:\"Word up, SREcon attendee!\"",
       "message:\"Grüezi, SREcon attendee!\"",
     }
    },
    {.description = "Timeout while receiving",
     .deadline_ms = 2500,
     .requests = {
       "name:\"SREcon attendee\" locale:\"en\"",
       "name:\"SREcon attendee\" locale:\"CH\"",
     },
     .behaviours = {
       "result: OK jitter { mean_ms: 1000 }",
       "result: OK jitter { mean_ms: 1000 }",
       "result: OK jitter { mean_ms: 1000 }",
     },
     .code = grpc::DEADLINE_EXCEEDED,
     .expected = {
       "message:\"How do you do, SREcon attendee!\"",
       "message:\"Word up, SREcon attendee!\"",
     }
    },
    {.description = "Timeout while sending requests",
     .deadline_ms = 1500,
     .requests = {
       "name:\"SREcon attendee\" locale:\"en\"",
       "name:\"SREcon attendee\" locale:\"CH\"",
     },
     .behaviours = {
       "result: OK jitter { mean_ms: 1000 }",
       "result: OK jitter { mean_ms: 1000 }",
       "result: OK jitter { mean_ms: 1000 }",
       "result: OK jitter { mean_ms: 1000 }",
     },
     .code = grpc::DEADLINE_EXCEEDED,
     .expected = {
       "message:\"How do you do, SREcon attendee!\"",
     }
    },
  },
};

bool RunUnaryTests(int exercise,
                   TranslatorControl::Stub* control,
                   Greeter::Stub* greeter) {
  bool is_ok = true;
  const std::vector<struct UnaryTestCase>& cases = unary_testcases[exercise];
  if (cases.empty()) return is_ok;
  LOG(INFO) << "Running " << cases.size()
            << " Unary test cases for Exercise " << exercise << ".";
  {
    // The behaviours need to be accumulated into a repeated field and set in
    // one request.
    BehaviourDefinition def;
    for (const struct UnaryTestCase& c : cases) {
      if (!c.behaviour) {
        continue;
      }
      google::protobuf::TextFormat::ParseFromString(
          c.behaviour, def.add_unary());
    }
    if (def.unary_size() > 0) {
      BehaviourReply unused_reply;
      grpc::ClientContext ctx;
      ctx.set_deadline(std::chrono::system_clock::now() +
                       std::chrono::seconds(5));
      grpc::Status status = control->SetBehaviour(&ctx, def, &unused_reply);
      // There may not be a translator running. This is OK if no behaviours are
      // set.
      if (!status.ok()) {
        LOG(FATAL)  // Crash ok
            << "Unable to set Translator Behaviour! Request: ["
            << def.ShortDebugString() << "] error "
            << status.error_code() << " ("
            << status.error_message() << ")";
      }
    }
  }
  // If any test case fails, the test as a whole will fail, but run all other
  // cases anyway.
  for (int i = 0; i < cases.size(); ++i) {
    const struct UnaryTestCase& c = cases[i];
    HelloRequest request;
    google::protobuf::TextFormat::ParseFromString(c.request, &request);
    HelloReply reply;
    grpc::ClientContext ctx;

    auto start_time = std::chrono::system_clock::now();
    if (c.deadline_ms > 0) {
      ctx.set_deadline(start_time +
                       std::chrono::milliseconds(c.deadline_ms));
    }
    grpc::Status status = greeter->SayHello(&ctx, request, &reply);
    auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - start_time);

    if (c.code == grpc::DO_NOT_USE) {
      LOG(INFO) << "Completed test case " << i << ": "
                << c.description << ".\n\tRequest: [" << c.request
                << "] returned status " << status.error_code()
                << " (" << status.error_message() << "), reply ["
                << reply.ShortDebugString() << "], took "
                << delta.count() << "ms.";
    } else if (c.code == grpc::OK) {
      HelloReply expected;
      google::protobuf::TextFormat::ParseFromString(c.expected, &expected);
      if (!status.ok()) {
        is_ok = false;
        LOG(ERROR) << "Unexpected failure for test case " << i << ": "
                   << c.description << ".\n\tRequest: [" << c.request
                   << "], expected [" << c.expected
                   << "], received error " << status.error_code()
                   << " (" << status.error_message() << "), took "
                   << delta.count() << "ms.";
      } else if (expected.message() != reply.message()) {
        is_ok = false;
        LOG(ERROR) << "Unexpected reply for test case " << i << ": "
                   << c.description << ".\n\tRequest: [" << c.request
                   << "], expected message [" << expected.message()
                   << "], received [" << reply.message() << "], took "
                   << delta.count() << "ms.";
      } else {
        LOG(INFO) << "Success for test case " << i << ": "
                  << c.description << ".\n\tRequest: [" << c.request
                  << "] returned reply [" << c.expected << "], took "
                  << delta.count() << "ms.";
      }
    } else if (status.ok()) {
      is_ok = false;
      LOG(ERROR) << "Unexpected success for test case " << i << ": "
                 << c.description << ".\n\tRequest: [" << c.request
                 << "] returned reply [" << reply.ShortDebugString()
                 << "], took " << delta.count() << "ms.";
    } else if (status.error_code() != c.code) {
      is_ok = false;
      LOG(ERROR) << "Unexpected Error Code for test case " << i << ": "
                 << c.description << ".\n\tRequest: [" << c.request
                 << "], expected code " << c.code
                 << ", received " << status.error_code()
                 << " (" << status.error_message() << "), took "
                 << delta.count() << "ms.";
    } else {
      LOG(INFO) << "Success for test case " << i << ": "
                << c.description << ".\n\tRequest: [" << c.request
                << "] failed with code " << c.code << ", took "
                << delta.count() << "ms.";
    }
  }
  return is_ok;
}

bool DiffStreamResults(std::vector<std::string> expected,
                       std::vector<HelloReply> received,
                       std::string* result) {
  bool ok = true;
  result->clear();
  for (int i = 0; i < std::min(expected.size(), received.size()); ++i) {
    HelloReply e;
    google::protobuf::TextFormat::ParseFromString(expected[i], &e);
    if (e.message() == received[i].message()) {
      *result =
          *result + "\tMessage " + std::to_string(i) + " matches: \"" +
          e.message() + "\"\n";
    } else {
      ok = false;
      *result =
          *result + "\tMessage " + std::to_string(i) + " mismatch, expected \"" +
          e.message() + "\", received \"" + received[i].message() + "\"\n";
    }
  }
  if (expected.size() > received.size()) {
    ok = false;
    *result =
        *result + "\tIncomplete reply, expected " +
        std::to_string(expected.size()) + " replies, received only " +
        std::to_string(received.size()) + ", missing:\n";
    for (int i = received.size(); i < expected.size(); ++i) {
      *result = *result + "\t  " + expected[i] + "\n";
    }
  } else if (received.size() > expected.size()) {
    ok = false;
    *result =
        *result + "\tToo many replies, expected only " +
        std::to_string(expected.size()) + " replies, received " +
        std::to_string(received.size()) + ", extra:\n";
    for (int i = expected.size(); i < received.size(); ++i) {
      *result = *result + "\t  " + received[i].ShortDebugString() + "\n";
    }
  }
  return ok;
}

bool RunStreamTests(int exercise,
                    TranslatorControl::Stub* control,
                    Greeter::Stub* greeter) {
  bool is_ok = true;
  const std::vector<struct StreamTestCase>& cases = stream_testcases[exercise];
  LOG(INFO) << "Running " << cases.size()
            << " Streaming test cases for Exercise " << exercise << ".";
  {
    // The behaviours need to be accumulated into a repeated field and set in
    // one request.
    BehaviourDefinition def;
    for (const struct StreamTestCase& c : cases) {
      for (const auto& b : c.behaviours) {
        google::protobuf::TextFormat::ParseFromString(b, def.add_stream());
      }
    }
    if (def.stream_size() > 0) {
      BehaviourReply unused_reply;
      grpc::ClientContext ctx;
      ctx.set_deadline(std::chrono::system_clock::now() +
                       std::chrono::seconds(5));
      grpc::Status status = control->SetBehaviour(&ctx, def, &unused_reply);
      if (!status.ok()) {
        LOG(FATAL)  // Crash ok
            << "Unable to set Translator Behaviour! Request: "
            << def.ShortDebugString() << " error "
            << status.error_code() << " ("
            << status.error_message() << ")";
      }
    }
  }
  for (int i = 0; i < cases.size(); ++i) {
    const struct StreamTestCase& c = cases[i];

    grpc::ClientContext ctx;

    auto start_time = std::chrono::system_clock::now();
    if (c.deadline_ms > 0) {
      ctx.set_deadline(start_time +
                       std::chrono::milliseconds(c.deadline_ms));
    }
    std::shared_ptr<grpc::ClientReaderWriter<HelloRequest, HelloReply>>
        stream(greeter->ManyHellos(&ctx));
    for (const auto& r : c.requests) {
      HelloRequest req;
      google::protobuf::TextFormat::ParseFromString(r, &req);
      stream->Write(req);
    }
    stream->WritesDone();
    std::vector<HelloReply> received;
    HelloReply rec;
    while (stream->Read(&rec)) {
      received.emplace_back(std::move(rec));
    }
    grpc::Status status = stream->Finish();

    auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(
        start_time - std::chrono::system_clock::now());
    std::string message;
    bool replies_ok = DiffStreamResults(c.expected, received, &message);
    if (c.code == grpc::DO_NOT_USE) {
      LOG(INFO) << "Completed test case " << i << ": " << c.description
                << ", sent " << c.requests.size() << " requests. received "
                << received.size() << " replies, with final status "
                << status.error_code() << " (" << status.error_message()
                << "), took " << delta.count() << "ms.\n\t" << message;
    } else {
      if (status.error_code() == c.code) {
        if (replies_ok) {
          LOG(INFO) << "Success for test case " << i << ": " << c.description
                    << ". Result:\n" << message;
        } else {
          is_ok = false;
          LOG(ERROR) << "Unexpected result for test case " << i << ": "
                     << c.description << ". Result:\n" << message;
        }
      } else {
        is_ok = false;
        LOG(ERROR) << "Unexpected result for test case " << i << ": "
                   << c.description << ". Received error "
                   << status.error_code() << " (" << status.error_message()
                   << "), expected " << c.code << ". Result:\n" << message;
      }
    }
  }
  return is_ok;
}

}  // namespace srecon

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  assert(srecon::unary_testcases.size() == srecon::stream_testcases.size());

  if (FLAGS_exercise < 0 || FLAGS_exercise > srecon::unary_testcases.size()) {
    LOG(ERROR)
        << "--exercise must be >=0 and <= " << srecon::unary_testcases.size();
    return 1;
  }

  LOG(INFO) << "Creating Control connection to " << FLAGS_translation_server;
  std::unique_ptr<srecon::TranslatorControl::Stub> control{
      srecon::TranslatorControl::NewStub(
          grpc::CreateChannel(FLAGS_translation_server,
                              grpc::InsecureChannelCredentials()))};

  LOG(INFO) << "Creating Greeter connection to " << FLAGS_greeter_server;
  std::unique_ptr<srecon::Greeter::Stub> greeter{
      srecon::Greeter::NewStub(
          grpc::CreateChannel(FLAGS_greeter_server,
                              grpc::InsecureChannelCredentials()))};

  bool ok =
      RunUnaryTests(FLAGS_exercise, control.get(), greeter.get()) &&
      RunStreamTests(FLAGS_exercise, control.get(), greeter.get());
  return ok ? 0 : 1;
}
