# Generated by the gRPC Python protocol compiler plugin. DO NOT EDIT!
import grpc

import translator_pb2 as translator__pb2


class TranslatorStub(object):
  """The Translation Service definition.
  """

  def __init__(self, channel):
    """Constructor.

    Args:
      channel: A grpc.Channel.
    """
    self.Translate = channel.unary_unary(
        '/srecon.Translator/Translate',
        request_serializer=translator__pb2.TranslationRequest.SerializeToString,
        response_deserializer=translator__pb2.TranslationReply.FromString,
        )
    self.AllTranslations = channel.unary_stream(
        '/srecon.Translator/AllTranslations',
        request_serializer=translator__pb2.AllTranslationsRequest.SerializeToString,
        response_deserializer=translator__pb2.AllTranslationsReply.FromString,
        )


class TranslatorServicer(object):
  """The Translation Service definition.
  """

  def Translate(self, request, context):
    """Takes a message and the locale to translate it to.
    """
    context.set_code(grpc.StatusCode.UNIMPLEMENTED)
    context.set_details('Method not implemented!')
    raise NotImplementedError('Method not implemented!')

  def AllTranslations(self, request, context):
    """Streaming service which takes messages and the locales to translate it to.
    """
    context.set_code(grpc.StatusCode.UNIMPLEMENTED)
    context.set_details('Method not implemented!')
    raise NotImplementedError('Method not implemented!')


def add_TranslatorServicer_to_server(servicer, server):
  rpc_method_handlers = {
      'Translate': grpc.unary_unary_rpc_method_handler(
          servicer.Translate,
          request_deserializer=translator__pb2.TranslationRequest.FromString,
          response_serializer=translator__pb2.TranslationReply.SerializeToString,
      ),
      'AllTranslations': grpc.unary_stream_rpc_method_handler(
          servicer.AllTranslations,
          request_deserializer=translator__pb2.AllTranslationsRequest.FromString,
          response_serializer=translator__pb2.AllTranslationsReply.SerializeToString,
      ),
  }
  generic_handler = grpc.method_handlers_generic_handler(
      'srecon.Translator', rpc_method_handlers)
  server.add_generic_rpc_handlers((generic_handler,))
