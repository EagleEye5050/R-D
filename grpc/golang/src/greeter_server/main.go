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

package main

import (
	"errors"
	"flag"
	"fmt"
	"log"
	"net"
	// Exercise 2
	// "time"

	greeter_pb "greeter"

	"golang.org/x/net/context"
	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"
	// For the Translator backend:
	// translator_pb "translator"
)

var serverPort = flag.Int("port", 50051, "Port on which to listen.")
var translationServer = flag.String("translation_server", "localhost:50061", "Server address of the translation server.")

// Exercise 2: Default deadline in ms.
//var deadlineMs = flag.Int("deadline_ms", 20*1000, "Default deadline in milliseconds.")

// server is used to implement helloworld.GreeterServer.
type server struct{}

// EXERCISE 1: Add Translator stub to the server.

// SayHello implements greeter.GreeterServer
func (s *server) SayHello(ctx context.Context, in *greeter_pb.HelloRequest) (*greeter_pb.HelloReply, error) {
	prefix := "Hello"
	// EXERCISE 1: Translate the prefix using Translator backend.
	return &greeter_pb.HelloReply{Message: prefix + " " + in.Name}, nil
}

// EXERCISE 5: Implement streaming call.
func (s *server) ManyHellos(clientStream greeter_pb.Greeter_ManyHellosServer) error {
	return errors.New("Streaming call is unimplemented")
}

func main() {
	flag.Parse()

	if *serverPort < 1025 || *serverPort > 65000 {
		log.Fatal("--port must be between 1025 and 65000")
	}
	port := fmt.Sprintf(":%d", *serverPort)

	lis, err := net.Listen("tcp", port)
	if err != nil {
		log.Fatalf("failed to listen: %v", err)
	}
	log.Printf("Server listening on %v", port)

	s := grpc.NewServer()
	greeter_pb.RegisterGreeterServer(s, &server{})

	// Register reflection service on gRPC server.
	reflection.Register(s)

	if err := s.Serve(lis); err != nil {
		log.Fatalf("failed to serve: %v", err)
	}
}
