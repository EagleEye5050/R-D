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

#include <chrono>

#include <glog/logging.h>

#include "translation_behaviour.h"

namespace srecon{

ExpectedBehaviour::ExpectedBehaviour()
    : urng_(std::random_device()()), next_unary_(0), next_stream_(0) {
  // By default, everything works with no extra delay.
  Behaviour* b = definition_.add_unary();
  b->mutable_jitter()->set_mean_ms(0);
  b->set_result(OK);

  b = definition_.add_stream();
  b->mutable_jitter()->set_mean_ms(0);
  b->set_result(OK);
}

// Update the expected behaviour from a new requested definition.
void ExpectedBehaviour::Update(const BehaviourDefinition& definition) {
  LOG(INFO) << "Received new BehaviourDefinition, with "
            << definition.unary_size() << " unary results, and "
            << definition.stream_size() << " stream results.";
  std::lock_guard<std::mutex> lock(mu_);
  definition_ = definition;
  next_unary_ = next_stream_ = 0;
  if (definition.unary_size() > 0 &&
      definition.unary(0).jitter().mean_ms() > 0) {
    jitter_.reset(new std::normal_distribution<>(
        definition.unary(0).jitter().mean_ms(),
        definition.unary(0).jitter().stddev_ms()));
  } else if (definition.stream_size() > 0 &&
             definition.stream(0).jitter().mean_ms() > 0) {
    jitter_.reset(new std::normal_distribution<>(
        definition.stream(0).jitter().mean_ms(),
        definition.stream(0).jitter().stddev_ms()));
  } else {
    jitter_.release();
  }
}

grpc::Status ExpectedBehaviour::BehaveUnary() {
  if (definition_.unary_size() > next_unary_) {
    return Behave(definition_.unary(next_unary_++));
  } else {
    return Behave(default_);
  }
}

grpc::Status ExpectedBehaviour::BehaveStream() {
  if (definition_.stream_size() > next_stream_) {
    return Behave(definition_.stream(next_stream_++));
  } else {
    return Behave(default_);
  }
}

grpc::Status ExpectedBehaviour::Behave(const Behaviour& behaviour) {
  long long sleep_time = 0;
  grpc::Status result;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (behaviour.jitter().mean_ms() > 0) {
      if (!jitter_ ||
          jitter_->mean() != behaviour.jitter().mean_ms()) {
        jitter_.reset(new std::normal_distribution<>(
            behaviour.jitter().mean_ms(),
            behaviour.jitter().stddev_ms()));
        LOG(INFO) << "New delay jitter: mean " << jitter_->mean()
                  << "ms, stddev " << jitter_->stddev() << "ms.";
      }
      sleep_time = std::floor((*jitter_)(urng_));
    }
    result = grpc::Status(
        static_cast<grpc::StatusCode>(behaviour.result()),
        "an error occurred");
  }
  if (result.ok()) {
    LOG(INFO) << "Sleeping for " << sleep_time << "ms, then returning OK.";
  } else {
    LOG(INFO) << "Sleeping for " << sleep_time << "ms, then returning error ("
              << result.error_code() << ").";
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
  return result;
}

}  // namespace srecon
