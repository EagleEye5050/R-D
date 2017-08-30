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

#ifndef SRECON_TRANSLATION_BEHAVIOUR_H_
#define SRECON_TRANSLATION_BEHAVIOUR_H_

#include <memory>
#include <mutex>
#include <random>
#include <thread>

#include <grpc++/grpc++.h>

#include "control.pb.h"

namespace srecon {

class ExpectedBehaviour {
 public:
  ExpectedBehaviour();

  // Update the expected behaviour from a new requested definition.
  void Update(const BehaviourDefinition& definition);

  // Return the desired return status. May sleep for a while.
  grpc::Status BehaveUnary();

  grpc::Status BehaveStream();

 private:
  grpc::Status Behave(const Behaviour& behaviour);

  Behaviour default_;
  std::mutex mu_;
  BehaviourDefinition definition_;
  std::mt19937 urng_;
  std::unique_ptr<std::normal_distribution<>> jitter_;
  int next_unary_;
  int next_stream_;
};

}  // namespace srecon

#endif  // SRECON_TRANSLATION_BEHAVIOUR_H_
