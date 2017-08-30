/*
 * Copyright 2017, Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package srecon;

import io.grpc.Server;
import io.grpc.ServerBuilder;
import io.grpc.protobuf.services.ProtoReflectionService;
import io.grpc.stub.StreamObserver;

import org.kohsuke.args4j.spi.IntOptionHandler;
import org.kohsuke.args4j.CmdLineException;
import org.kohsuke.args4j.CmdLineParser;
import org.kohsuke.args4j.Option;
import org.kohsuke.args4j.ParserProperties;

import java.io.IOException;
import java.util.concurrent.TimeUnit;
import java.util.logging.Level;
import java.util.logging.Logger;

public class GreeterServer {
  private static final Logger logger = Logger.getLogger(GreeterServer.class.getName());

  @Option(name="--port", handler=IntOptionHandler.class, usage="Port on which to listen.")
  int listenPort;

  // EXERCISE 2: Default deadline in ms.
  @Option(name="--deadline_ms", handler=IntOptionHandler.class, usage="Default deadline in milliseconds.")
  int deadlineMs = 20*1000;

  // For EXERCISE 1:
  @Option(name="--translation_server", usage="host:port of the translation server.")
  String translationServerOpt = "localhost:50061";
  // Populated from the flag:
  private String translationServerHost;
  private int translationServerPort;

  // The service implementation:
  private Server server;

  /** The constructor only parses the commandline arguments. */
  public GreeterServer(String[] args) {
    CmdLineParser parser = new CmdLineParser(this, ParserProperties.defaults().withOptionValueDelimiter("="));
    try {
      parser.parseArgument(args);
    } catch( CmdLineException e ) {
      logger.log(Level.SEVERE, "Error parsing commandline", e);
      System.exit(1);
    }
    String[] hostPort = translationServerOpt.split(":");
    translationServerHost = hostPort[0];
    translationServerPort = Integer.parseInt(hostPort[1]);

    // EXERCISE 1: Add the translation Server stub.
  }

  /** Create the gRPC server and listen for requests. */
  public void start() throws IOException {
    server = ServerBuilder.forPort(listenPort)
        .addService(new GreeterImpl())
        .addService(ProtoReflectionService.newInstance())  // For grpc_cli
        .build()
        .start();

    logger.info("Server started, listening on " + listenPort);

    // Support clean shutdown:
    Runtime.getRuntime().addShutdownHook(new Thread() {
      @Override
      public void run() {
        // Use stderr here since the logger may have been reset by its JVM shutdown hook.
        System.err.println("*** shutting down gRPC server (JVM shutting down)");
        GreeterServer.this.stop();
        System.err.println("*** server shut down");
      }
    });
  }

  public void stop() {
    if (server != null) {
      server.shutdown();
    }
  }

  /**
   * Await termination on the main thread since the grpc library uses daemon threads.
   */
  private void blockUntilShutdown() throws InterruptedException {
    if (server != null) {
      server.awaitTermination();
    }
  }

  /**
   * Run the greeter server.
   */
  public static void main(String[] args) throws Exception {
    GreeterServer server = new GreeterServer(args);
    server.start();
    server.blockUntilShutdown();
  }

  static class GreeterImpl extends GreeterGrpc.GreeterImplBase {

    @Override
    public void sayHello(HelloRequest req, StreamObserver<HelloReply> responseObserver) {
      // EXERCISE 1:
      // Translate the prefix using Translator backend.
      String prefix = "Hello";

      HelloReply reply = HelloReply.newBuilder().setMessage(prefix + " " + req.getName()).build();
      responseObserver.onNext(reply);
      responseObserver.onCompleted();
    }

    // EXERCISE 5:
    // Implement the streaming call.
    @Override
    public StreamObserver<HelloRequest> manyHellos(final StreamObserver<HelloReply> responseObserver) {
      return new StreamObserver<HelloRequest>() {
        @Override
        public void onNext(HelloRequest request) {
          // Called for every incoming request.
          responseObserver.onError(new UnsupportedOperationException("UNIMPLEMENTED"));
        }

        @Override
        public void onError(Throwable t) {
          // Called if the incoming or outgoing streams have errors.
          logger.log(Level.SEVERE, "Encountered error: ", t);
          responseObserver.onError(t);
        }

        @Override
        public void onCompleted() {
          // Called when the incoming stream is completed (onNext will no longer be called).
          responseObserver.onCompleted();
        }
      };
    }
  }

}
