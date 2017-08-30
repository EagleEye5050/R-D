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

import io.grpc.Context;
import io.grpc.ManagedChannel;
import io.grpc.ManagedChannelBuilder;
import io.grpc.Server;
import io.grpc.ServerBuilder;
import io.grpc.Status;
import io.grpc.StatusRuntimeException;
import io.grpc.protobuf.services.ProtoReflectionService;
import io.grpc.stub.StreamObserver;

import org.kohsuke.args4j.CmdLineException;
import org.kohsuke.args4j.CmdLineParser;
import org.kohsuke.args4j.Option;
import org.kohsuke.args4j.ParserProperties;
import org.kohsuke.args4j.spi.IntOptionHandler;

import java.io.IOException;
import java.util.concurrent.TimeUnit;
import java.util.Iterator;
import java.util.logging.Level;
import java.util.logging.Logger;

public class GreeterServerDemo {
  private static final Logger logger = Logger.getLogger(GreeterServer.class.getName());

  @Option(name="--port", handler=IntOptionHandler.class, usage="Port on which to listen.")
  int listenPort;

  @Option(name="--deadline_ms", handler=IntOptionHandler.class, usage="Default deadline in milliseconds.")
  int deadlineMs = 20*1000;

  // For EXERCISE 1:
  @Option(name="--translation_server", usage="host:port of the translation server.")
  String translationServerOpt = "localhost:50061";
  // Populated from the flag:
  private String translationServerHost;
  private int translationServerPort;
  private int defaultDeadline;

  // The translator:
  private ManagedChannel translatorChannel;
  private TranslatorGrpc.TranslatorBlockingStub translatorStub;

  // The service implementation:
  private Server server;

  /** The constructor only parses the commandline arguments. */
  public GreeterServerDemo(String[] args) {
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
    defaultDeadline = deadlineMs;

    translatorChannel =
        ManagedChannelBuilder.forAddress(translationServerHost, translationServerPort)
        .usePlaintext(true)
        .build();
    translatorStub = TranslatorGrpc.newBlockingStub(translatorChannel);
  }

  /** Create the gRPC server and listen for requests. */
  public void start() throws IOException {
    server = ServerBuilder.forPort(listenPort)
        .addService(new GreeterImpl(translatorStub, defaultDeadline))
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
        GreeterServerDemo.this.stop();
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
    GreeterServerDemo server = new GreeterServerDemo(args);
    server.start();
    server.blockUntilShutdown();
  }

  static class GreeterImpl extends GreeterGrpc.GreeterImplBase {

    private TranslatorGrpc.TranslatorBlockingStub translatorStub;
    private int defaultDeadline;

    public GreeterImpl(TranslatorGrpc.TranslatorBlockingStub stub, int deadline) {
      translatorStub = stub;
      defaultDeadline = deadline;
    }

    @Override
    public void sayHello(HelloRequest req, StreamObserver<HelloReply> responseObserver) {
      String prefix = "Hello";

      // Translate the prefix using Translator backend.
      TranslationRequest request = TranslationRequest.newBuilder()
          .setMessage(prefix)
          .setLocale(req.getLocale())
          .build();
      TranslationReply response;
      try {
        // Explicitly propagate the client deadline, if no deadline has been set by the client use
        // the default deadline.
        if (Context.current().getDeadline() == null) {
          logger.info("We set the default deadline to " + defaultDeadline);
          response = translatorStub.withDeadlineAfter(defaultDeadline, TimeUnit.MILLISECONDS).translate(request);
        } else {
          response = translatorStub.withDeadline(Context.current().getDeadline()).translate(request);
        }
        prefix = response.getTranslation();
      } catch (StatusRuntimeException e) {
        logger.log(Level.WARNING, "RPC failed: {0}", e.getStatus());
      }

      HelloReply reply = HelloReply.newBuilder().setMessage(prefix + ", " + req.getName() + "!").build();
      responseObserver.onNext(reply);
      responseObserver.onCompleted();
    }

    // The streaming call returns the processor. The grpc library will asynchronously call its
    // onNext() method for every received request.
    @Override
    public StreamObserver<HelloRequest> manyHellos(final StreamObserver<HelloReply> responseObserver) {
      return new StreamObserver<HelloRequest>() {
        boolean haveRequests = false;

        @Override
        public void onNext(HelloRequest request) {
          logger.info("Received request: " + request);
          haveRequests = true;

          AllTranslationsRequest translationRequest = AllTranslationsRequest.newBuilder()
              .setMessage("Hello")
              .addLocales(request.getLocale())
              .build();
          Iterator<AllTranslationsReply> results;
          try {
            results = translatorStub.allTranslations(translationRequest);
          } catch (StatusRuntimeException ex) {
            logger.log(Level.WARNING, "RPC failed: {0}", ex.getStatus());
            responseObserver.onError(ex);
            return;
          }
          boolean found = results.hasNext();
          while (results.hasNext()) {
            AllTranslationsReply translation = results.next();
            found = true;
            // Check whether the client still cares (don't do work if, say, their
            // caller's deadline has expired):
            if (Context.current().isCancelled()) {
              responseObserver.onError(Status.CANCELLED.withDescription("Cancelled by client").asRuntimeException());
              return;
            }
            HelloReply reply = HelloReply.newBuilder().setMessage(translation.getTranslation() + ", " + request.getName() + "!").build();
            responseObserver.onNext(reply);
          }
          if (!found) {
            responseObserver.onError(Status.ABORTED
                .withDescription("No translations found for \"Hello\" in locales matching \"" + request.getLocale() + "\"")
                .asRuntimeException());
            return;
          }
        }

        @Override
        public void onError(Throwable t) {
          logger.log(Level.SEVERE, "Encountered error: ", t);
          responseObserver.onError(t);
        }

        @Override
        public void onCompleted() {
          if (!haveRequests) {
            responseObserver.onError(Status.FAILED_PRECONDITION
                .withDescription("No requests received")
                .asException());
          } else {
            responseObserver.onCompleted();
          }
        }
      };
    }
  }

}
