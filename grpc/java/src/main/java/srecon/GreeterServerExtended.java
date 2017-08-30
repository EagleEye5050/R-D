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

import com.sun.net.httpserver.HttpServer;
import io.grpc.Context;
import io.grpc.ManagedChannel;
import io.grpc.ManagedChannelBuilder;
import io.grpc.Server;
import io.grpc.ServerBuilder;
import io.grpc.Status;
import io.grpc.StatusRuntimeException;
import io.grpc.internal.AbstractManagedChannelImplBuilder;
import io.grpc.protobuf.services.ProtoReflectionService;
import io.grpc.stub.StreamObserver;
import io.opencensus.contrib.zpages.ZPageHandlers;
import io.opencensus.trace.Tracer;
import io.opencensus.trace.Tracing;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.util.Arrays;
import java.util.Collections;
import java.util.Iterator;
import java.util.logging.Level;
import java.util.logging.Logger;
import org.kohsuke.args4j.CmdLineException;
import org.kohsuke.args4j.CmdLineParser;
import org.kohsuke.args4j.Option;
import org.kohsuke.args4j.ParserProperties;
import org.kohsuke.args4j.spi.IntOptionHandler;

public class GreeterServerExtended {
  private static final Logger logger = Logger.getLogger(GreeterServer.class.getName());

  @Option(name = "--port", handler = IntOptionHandler.class, usage = "Port on which to listen.")
  int listenPort;

  @Option(name = "--debug_port", handler = IntOptionHandler.class, usage = "Port for debugging listen.")
  int debugPort = 8080;

  // For EXERCISE 1:
  @Option(name = "--translation_server", usage = "host:port of the translation server.")
  String translationServerOpt = "localhost:50061";

  // Populated from the flag:
  private String translationServerHost;
  private int translationServerPort;

  // The translator:
  private ManagedChannel translatorChannel;
  private TranslatorGrpc.TranslatorBlockingStub translatorStub;

  // The debug HTTP server:
  HttpServer debugServer = null;

  // The service implementation:
  private Server server;

  /** The constructor only parses the commandline arguments. */
  public GreeterServerExtended(String[] args) {
    CmdLineParser parser = new CmdLineParser(this, ParserProperties.defaults().withOptionValueDelimiter("="));
    try {
      parser.parseArgument(args);
    } catch(CmdLineException e ) {
      logger.log(Level.SEVERE, "Error parsing commandline", e);
      System.exit(1);
    }
    String[] hostPort = translationServerOpt.split(":");
    translationServerHost = hostPort[0];
    translationServerPort = Integer.parseInt(hostPort[1]);

    ManagedChannelBuilder builder =
        ManagedChannelBuilder.forAddress(translationServerHost, translationServerPort)
        .usePlaintext(true);
    // This is not needed if 1.6.0 grpc is released or they build a new 1.6.0-SNAPSHOT.
    if (builder instanceof AbstractManagedChannelImplBuilder) {
      ((AbstractManagedChannelImplBuilder) builder).setEnableTracing(true);
    }
    translatorChannel = builder.build();
    translatorStub = TranslatorGrpc.newBlockingStub(translatorChannel);
  }

  public void startDebugServer() throws IOException {
    debugServer = HttpServer.create(new InetSocketAddress(debugPort), 10);
    // This is needed until gRPC 1.7.0;
    Tracing.getExportComponent()
        .getSampledSpanStore()
        .registerSpanNamesForCollection(
            Arrays.asList(
                "Recv.srecon.Greeter.SayHello",
                "Recv.srecon.Greeter.ManyHellos",
                "Sent.srecon.Translator.Translate",
                "Sent.srecon.Translator.AllTranslations"));
    ZPageHandlers.registerAllToHttpServer(debugServer);
    debugServer.start();
  }

  /** Create the gRPC server and listen for requests. */
  public void start() throws IOException {
    server = ServerBuilder.forPort(listenPort)
        .addService(new GreeterImpl(translatorStub))
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
        GreeterServerExtended.this.stop();
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
    GreeterServerExtended server = new GreeterServerExtended(args);
    server.startDebugServer();
    server.start();
    server.blockUntilShutdown();
  }

  static class GreeterImpl extends GreeterGrpc.GreeterImplBase {
    private static final Tracer tracer = Tracing.getTracer();

    private TranslatorGrpc.TranslatorBlockingStub translatorStub;

    public GreeterImpl(TranslatorGrpc.TranslatorBlockingStub stub) {
      translatorStub = stub;
    }

    @Override
    public void sayHello(HelloRequest req, StreamObserver<HelloReply> responseObserver) {
      String prefix = "Hello";

      // Translate the prefix using Translator backend.
      TranslationRequest request = TranslationRequest.newBuilder()
          .setMessage(prefix)
          .setLocale(req.getLocale())
          .build();
      tracer.getCurrentSpan().addAnnotation("Send translation request");
      TranslationReply response;
      try {
        // Explicitly porpagate the client deadline:
        response = translatorStub.withDeadline(Context.current().getDeadline()).translate(request);
        prefix = response.getTranslation();
        tracer.getCurrentSpan().addAnnotation("Recv translation response");
      } catch (StatusRuntimeException e) {
        logger.log(Level.WARNING, "RPC failed: {0}", e.getStatus());
        tracer.getCurrentSpan().addAnnotation("Traslation failed.");
      }
      HelloReply reply = HelloReply.newBuilder().setMessage(prefix + ", " + req.getName() + "!").build();
      tracer.getCurrentSpan().addAnnotation("Send response: " + reply.getMessage());
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

        public void onError(Throwable t) {
          logger.log(Level.SEVERE, "Encountered error: ", t);
        }

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
