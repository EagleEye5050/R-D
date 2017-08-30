/*
 * Copyright 2015, gRPC Authors All rights reserved.
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

import io.grpc.ManagedChannel;
import io.grpc.ManagedChannelBuilder;
import io.grpc.StatusRuntimeException;

import org.kohsuke.args4j.spi.ExplicitBooleanOptionHandler;
import org.kohsuke.args4j.CmdLineException;
import org.kohsuke.args4j.CmdLineParser;
import org.kohsuke.args4j.Option;
import org.kohsuke.args4j.ParserProperties;

import java.util.concurrent.TimeUnit;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.ArrayList;
import java.util.List;

/**
 * A simple client that requests a greeting from the {@link GreeterServer}.
 */
public class GreeterClient {
  private static final Logger logger = Logger.getLogger(GreeterClient.class.getName());

  private ManagedChannel channel;
  private GreeterGrpc.GreeterBlockingStub blockingStub;

  @Option(name="--user", usage="The user to greet!")
  private String[] userNameOpt;

  @Option(name="--greeter_server", usage="Server address of the greeter server.")
  private String greeterServer = "localhost:50051";

  @Option(name="--locale", usage="The locale for the greeting.")
  private String localeName = "en_US";

  @Option(name="--deadline_ms", usage="Deadline in milliseconds.")
  private int deadlineMs = 20*1000;

  @Option(name="--wait_for_ready", handler=ExplicitBooleanOptionHandler.class, usage="Whether to wait for the backend to become available. If false, fails fast.")
  private boolean waitForReady = true;

  @Option(name="--streaming", usage="Whether to use the streaming API.")
  private boolean useStreaming = false;

  /** Construct client connecting to HelloWorld server at {@code host:port}. */
  public GreeterClient(String[] args) {
    CmdLineParser parser = new CmdLineParser(this, ParserProperties.defaults().withOptionValueDelimiter("="));
    try {
      parser.parseArgument(args);
    } catch( CmdLineException e ) {
      logger.log(Level.SEVERE, "Error parsing commandline", e);
      System.exit(1);
    }
    String[] hostPort = greeterServer.split(":");
    channel = ManagedChannelBuilder.forAddress(hostPort[0], Integer.parseInt(hostPort[1]))
        // Channels are secure by default (via SSL/TLS). For the example we disable TLS to avoid
        // needing certificates.
        .usePlaintext(true)
        .build();
    if (waitForReady) {
      blockingStub = GreeterGrpc.newBlockingStub(channel).withWaitForReady();
    } else {
      blockingStub = GreeterGrpc.newBlockingStub(channel);
    }
  }

  public void shutdown() throws InterruptedException {
    channel.shutdown().awaitTermination(5, TimeUnit.SECONDS);
  }

  /** Say hello. */
  public String SayHello(String name) {
    logger.info("Will try to greet " + name + " ...");
    // Data we are sending to the server.
    HelloRequest request = HelloRequest.newBuilder().setName(name).setLocale(localeName).build();

    // Container for the data we expect from the server.
    HelloReply response;

    try {
      // Send the RPC with a deadline.
      // A deadline is not a timeout: it refers to a particular point in time.
      // Setting it once on the stub is possible, and makes sense if (and only if) the stub will be
      // used for multiple calls that are part of the same higher-level activity.
      response = blockingStub.withDeadlineAfter(deadlineMs, TimeUnit.MILLISECONDS).sayHello(request);
    } catch (StatusRuntimeException e) {
      logger.log(Level.WARNING, "RPC failed: {0}", e.getStatus());
      return "*** RPC failed ***";
    }
    return response.getMessage();
  }

  public List<String> allTheHellos(String name) {
    List<String> replies = new ArrayList<String>();

    // Create all the requests we are going to send.
    List<HelloRequest> requests = new ArrayList<HelloRequest>();
    {
      String[] parts = localeName.split("_");
      for (String p : parts) {
        requests.add(HelloRequest.newBuilder().setName(name).setLocale(p).build());
      }
    }
    logger.log(Level.INFO, "Will send {0} requests.", requests.size());

    return replies;
  }

  /**
   * Greeter client.
   */
  public static void main(String[] args) throws Exception {
    new GreeterClient(args).doMain();
  }

  public void doMain() throws Exception {
    String userName = "world";
    if (userNameOpt != null) {
      userName = userNameOpt[0];
      for (int i=1; i < userNameOpt.length; ++i) {
        userName += " " + userNameOpt[i];
      }
    }
    try {
      if (useStreaming) {
        List<String> replies = allTheHellos(userName);
        logger.log(Level.INFO, "Greetings received: {0}", replies.size());
      } else {
        String reply = SayHello(userName);
        logger.log(Level.INFO, "Greeting received: {0}", reply);
        System.out.println("Greeting received: " + reply);
      }
    } finally {
      shutdown();
    }
  }
}
