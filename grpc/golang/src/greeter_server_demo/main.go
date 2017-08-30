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
	"flag"
	"fmt"
	"io"
	"log"
	"net"
	"time"

	greeter_pb "greeter"
	translator_pb "translator"

	"golang.org/x/net/context"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/reflection"
	"google.golang.org/grpc/status"
)

var serverPort = flag.Int("port", 50051, "Port on which to listen.")
var translationServer = flag.String("translation_server", "localhost:50061", "Server address of the translation server.")
var deadlineMs = flag.Int("deadline_ms", 20*1000, "Default deadline in milliseconds.")

// server is used to implement helloworld.GreeterServer.
type server struct {
	translator translator_pb.TranslatorClient
}

// SayHello implements greeter.GreeterServer
func (s *server) SayHello(ctx context.Context, in *greeter_pb.HelloRequest) (*greeter_pb.HelloReply, error) {
	prefix := "Hello"

	tReq := translator_pb.TranslationRequest{
		Message: prefix,
		Locale:  in.Locale,
	}
	// To propagate the deadline, use the current context.
	// If no deadline has been set by the client use the default.
	defaultDeadline := time.Now().Add(time.Duration(*deadlineMs) * time.Millisecond)
	clientDeadline, deadlineSet := ctx.Deadline()
	if !deadlineSet {
		log.Printf("Setting default deadline : %d", defaultDeadline)
		clientDeadline = defaultDeadline
	}
	log.Printf("Using deadline : %d", clientDeadline)
	ctx, cancel := context.WithDeadline(ctx, clientDeadline)
	defer cancel()
	reply, err := s.translator.Translate(ctx, &tReq)
	if err != nil {
		log.Printf("Translator backend failed: %v", err)
	} else {
		prefix = reply.Translation
	}
	return &greeter_pb.HelloReply{Message: prefix + ", " + in.Name + "!"}, nil
}

// ManyHellos implements greeter.GreeterServer
func (s *server) ManyHellos(clientStream greeter_pb.Greeter_ManyHellosServer) error {
	inCount := 0
	for {
		in, err := clientStream.Recv()
		if err == io.EOF {
			if inCount > 0 {
				// All HelloRequests done.
				return nil
			}
			log.Print("No requests received")
			return status.Error(codes.FailedPrecondition, "No requests received")
		}
		if err != nil {
			log.Printf("Failed to read request: %v", err)
			return err
		}
		inCount++
		// Create the translation request from the current HelloRequest
		tReq := translator_pb.AllTranslationsRequest{
			Message: "Hello",
			Locales: []string{in.Locale},
		}
		// Call the Translator's streaming service.
		translatorStream, err := s.translator.AllTranslations(context.Background(), &tReq)
		if err != nil {
			// RPC error
			log.Printf("Failed to request translations: %v", err)
			return err
		}
		tCount := 0
		for {
			tReply, err := translatorStream.Recv()
			if err == io.EOF {
				// All translations read.
				if tCount > 0 {
					break
				}
				return status.Errorf(codes.Aborted, "No translations found for \"Hello\" in locales matching \"%s\"", in.Locale)
			}
			if err != nil {
				// RPC error
				log.Printf("Failed to read translation: %v", err)
				return err
			}
			tCount++
			reply := &greeter_pb.HelloReply{Message: tReply.Translation + ", " + in.Name + "!"}
			if err := clientStream.Send(reply); err != nil {
				log.Printf("Failed to send reply: %v", err)
				return err
			}
		}
	}
}

func main() {
	flag.Parse()

	if *serverPort < 1025 || *serverPort > 65000 {
		log.Fatal("--port must be between 1025 and 65000")
	}
	port := fmt.Sprintf(":%d", *serverPort)

	conn, err := grpc.Dial(*translationServer, grpc.WithInsecure())
	if err != nil {
		log.Fatalf("Unable to create channel to %s: %v", *translationServer, err)
	}
	defer conn.Close()
	translatorClient := translator_pb.NewTranslatorClient(conn)

	lis, err := net.Listen("tcp", port)
	if err != nil {
		log.Fatalf("failed to listen: %v", err)
	}
	log.Printf("Server listening on %v", port)

	s := grpc.NewServer()
	greeter_pb.RegisterGreeterServer(s, &server{translatorClient})

	// Register reflection service on gRPC server.
	reflection.Register(s)

	if err := s.Serve(lis); err != nil {
		log.Fatalf("failed to serve: %v", err)
	}
}
