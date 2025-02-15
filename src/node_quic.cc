#include "debug_utils.h"
#include "node.h"
#include "env-inl.h"
#include "histogram-inl.h"
#include "node_crypto.h"  // SecureContext
#include "node_process.h"
#include "node_quic_crypto.h"
#include "node_quic_session-inl.h"
#include "node_quic_socket.h"
#include "node_quic_stream.h"
#include "node_quic_state.h"
#include "node_quic_util.h"

#include <climits>
#include <algorithm>

namespace node {

using crypto::SecureContext;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::Value;

namespace quic {

namespace {
// Register the JavaScript callbacks the internal binding will use to report
// status and updates. This is called only once when the quic module is loaded.
void QuicSetCallbacks(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsObject());
  Local<Object> obj = args[0].As<Object>();

#define SETFUNCTION(name, callback)                                           \
  do {                                                                        \
    Local<Value> fn;                                                          \
    CHECK(obj->Get(env->context(),                                            \
                   FIXED_ONE_BYTE_STRING(env->isolate(), name)).ToLocal(&fn));\
    CHECK(fn->IsFunction());                                                  \
    env->set_quic_on_##callback##_function(fn.As<Function>());                \
  } while (0)

  SETFUNCTION("onSocketReady", socket_ready);
  SETFUNCTION("onSocketClose", socket_close);
  SETFUNCTION("onSocketError", socket_error);
  SETFUNCTION("onSessionReady", session_ready);
  SETFUNCTION("onSessionCert", session_cert);
  SETFUNCTION("onSessionClientHello", session_client_hello);
  SETFUNCTION("onSessionClose", session_close);
  SETFUNCTION("onSessionError", session_error);
  SETFUNCTION("onSessionHandshake", session_handshake);
  SETFUNCTION("onSessionKeylog", session_keylog);
  SETFUNCTION("onSessionPathValidation", session_path_validation);
  SETFUNCTION("onSessionSilentClose", session_silent_close);
  SETFUNCTION("onSessionStatus", session_status);
  SETFUNCTION("onSessionTicket", session_ticket);
  SETFUNCTION("onSessionVersionNegotiation", session_version_negotiation);
  SETFUNCTION("onStreamReady", stream_ready);
  SETFUNCTION("onStreamClose", stream_close);
  SETFUNCTION("onStreamError", stream_error);
  SETFUNCTION("onStreamReset", stream_reset);
  SETFUNCTION("onSocketServerBusy", socket_server_busy);

#undef SETFUNCTION
}

void QuicProtocolVersion(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(NGTCP2_PROTO_VER);
}

void QuicALPNVersion(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  args.GetReturnValue().Set(OneByteString(env->isolate(), NGTCP2_ALPN_H3));
}

// Sets QUIC specific configuration options for the SecureContext.
// It's entirely likely that there's a better way to do this, but
// for now this works.
void QuicInitSecureContext(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsObject());  // Secure Context
  CHECK(args[1]->IsString());  // groups
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args[0].As<Object>(),
                          args.GetReturnValue().Set(UV_EBADF));

  constexpr auto ssl_opts = (SSL_OP_ALL & ~SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS) |
                            SSL_OP_SINGLE_ECDH_USE |
                            SSL_OP_CIPHER_SERVER_PREFERENCE |
                            SSL_OP_NO_ANTI_REPLAY;
  SSL_CTX_set_options(**sc, ssl_opts);
  SSL_CTX_clear_options(**sc, SSL_OP_ENABLE_MIDDLEBOX_COMPAT);
  SSL_CTX_set_mode(**sc, SSL_MODE_RELEASE_BUFFERS | SSL_MODE_QUIC_HACK);
  SSL_CTX_set_default_verify_paths(**sc);
  SSL_CTX_set_max_early_data(**sc, std::numeric_limits<uint32_t>::max());
  SSL_CTX_set_alpn_select_cb(**sc, ALPN_Select_Proto_CB, nullptr);
  SSL_CTX_set_client_hello_cb(**sc, Client_Hello_CB, nullptr);
  SSL_CTX_set_tlsext_status_cb(**sc, TLS_Status_Callback);
  SSL_CTX_set_tlsext_status_arg(**sc, nullptr);
  CHECK_EQ(
      SSL_CTX_add_custom_ext(
          **sc,
          NGTCP2_TLSEXT_QUIC_TRANSPORT_PARAMETERS,
          SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_3_ENCRYPTED_EXTENSIONS,
          Server_Transport_Params_Add_CB,
          Transport_Params_Free_CB, nullptr,
          Server_Transport_Params_Parse_CB,
          nullptr), 1);

  const node::Utf8Value groups(env->isolate(), args[1]);
  if (!SSL_CTX_set1_groups_list(**sc, *groups)) {
    unsigned long err = ERR_get_error();  // NOLINT(runtime/int)
    if (!err)
      return env->ThrowError("Failed to set groups");
    return crypto::ThrowCryptoError(env, err);
  }
}

void QuicInitSecureContextClient(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsObject());  // Secure Context
  CHECK(args[1]->IsString());  // groups
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args[0].As<Object>(),
                          args.GetReturnValue().Set(UV_EBADF));

  SSL_CTX_set_mode(**sc, SSL_MODE_QUIC_HACK);
  SSL_CTX_clear_options(**sc, SSL_OP_ENABLE_MIDDLEBOX_COMPAT);
  SSL_CTX_set_default_verify_paths(**sc);
  SSL_CTX_set_tlsext_status_cb(**sc, TLS_Status_Callback);
  SSL_CTX_set_tlsext_status_arg(**sc, nullptr);

  CHECK_EQ(SSL_CTX_add_custom_ext(
      **sc,
      NGTCP2_TLSEXT_QUIC_TRANSPORT_PARAMETERS,
      SSL_EXT_CLIENT_HELLO | SSL_EXT_TLS1_3_ENCRYPTED_EXTENSIONS,
      Client_Transport_Params_Add_CB,
      Transport_Params_Free_CB,
      nullptr,
      Client_Transport_Params_Parse_CB,
      nullptr), 1);


  const node::Utf8Value groups(env->isolate(), args[1]);
  if (!SSL_CTX_set1_groups_list(**sc, *groups)) {
    unsigned long err = ERR_get_error();  // NOLINT(runtime/int)
    if (!err)
      return env->ThrowError("Failed to set groups");
    return crypto::ThrowCryptoError(env, err);
  }

  SSL_CTX_set_session_cache_mode(
    **sc, SSL_SESS_CACHE_CLIENT | SSL_SESS_CACHE_NO_INTERNAL_STORE);
  SSL_CTX_sess_set_new_cb(**sc, [](SSL* ssl, SSL_SESSION* session) {
    QuicClientSession* s =
        static_cast<QuicClientSession*>(
            SSL_get_app_data(ssl));
    return s->SetSession(session);
  });
}
}  // namespace


void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();
  HandleScope scope(isolate);

  HistogramBase::Initialize(env);

  std::unique_ptr<QuicState> state(new QuicState(isolate));
#define SET_STATE_TYPEDARRAY(name, field)             \
  target->Set(context,                                \
              FIXED_ONE_BYTE_STRING(isolate, (name)), \
              (field)).FromJust()
  SET_STATE_TYPEDARRAY(
    "sessionConfig", state->quicsessionconfig_buffer.GetJSArray());
#undef SET_STATE_TYPEDARRAY

  env->set_quic_state(std::move(state));

  QuicSocket::Initialize(env, target, context);
  QuicServerSession::Initialize(env, target, context);
  QuicClientSession::Initialize(env, target, context);
  QuicStream::Initialize(env, target, context);

  env->SetMethod(target,
                 "setCallbacks",
                 QuicSetCallbacks);
  env->SetMethod(target,
                 "protocolVersion",
                 QuicProtocolVersion);
  env->SetMethod(target,
                 "alpnVersion",
                 QuicALPNVersion);
  env->SetMethod(target,
                 "initSecureContext",
                 QuicInitSecureContext);
  env->SetMethod(target,
                 "initSecureContextClient",
                 QuicInitSecureContextClient);

  Local<Object> constants = Object::New(env->isolate());
  NODE_DEFINE_CONSTANT(constants, AF_INET);
  NODE_DEFINE_CONSTANT(constants, AF_INET6);
  NODE_DEFINE_CONSTANT(constants, DEFAULT_MAX_STREAM_DATA_BIDI_LOCAL);
  NODE_DEFINE_CONSTANT(constants, DEFAULT_RETRYTOKEN_EXPIRATION);
  NODE_DEFINE_CONSTANT(constants, DEFAULT_MAX_CONNECTIONS_PER_HOST);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATE_CERT_ENABLED);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATE_CLIENT_HELLO_ENABLED);
  NODE_DEFINE_CONSTANT(constants,
                       IDX_QUIC_SESSION_STATE_PATH_VALIDATED_ENABLED);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATE_KEYLOG_ENABLED);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATE_MAX_STREAMS_BIDI);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATE_MAX_STREAMS_UNI);
  NODE_DEFINE_CONSTANT(constants, MAX_RETRYTOKEN_EXPIRATION);
  NODE_DEFINE_CONSTANT(constants, MIN_RETRYTOKEN_EXPIRATION);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_MAX_CIDLEN);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_MIN_CIDLEN);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_NO_ERROR);
  NODE_DEFINE_CONSTANT(constants, QUIC_ERROR_APPLICATION);
  NODE_DEFINE_CONSTANT(constants, QUIC_ERROR_CRYPTO);
  NODE_DEFINE_CONSTANT(constants, QUIC_ERROR_SESSION);
  NODE_DEFINE_CONSTANT(constants, QUIC_PREFERRED_ADDRESS_ACCEPT);
  NODE_DEFINE_CONSTANT(constants, QUIC_PREFERRED_ADDRESS_IGNORE);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_DEFAULT_MAX_ACK_DELAY);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_PATH_VALIDATION_RESULT_FAILURE);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_PATH_VALIDATION_RESULT_SUCCESS);
  NODE_DEFINE_CONSTANT(constants, SSL_OP_ALL);
  NODE_DEFINE_CONSTANT(constants, SSL_OP_CIPHER_SERVER_PREFERENCE);
  NODE_DEFINE_CONSTANT(constants, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
  NODE_DEFINE_CONSTANT(constants, SSL_OP_NO_ANTI_REPLAY);
  NODE_DEFINE_CONSTANT(constants, SSL_OP_SINGLE_ECDH_USE);
  NODE_DEFINE_CONSTANT(constants, TLS1_3_VERSION);
  NODE_DEFINE_CONSTANT(constants, UV_EBADF);
  NODE_DEFINE_CONSTANT(constants, UV_UDP_IPV6ONLY);
  NODE_DEFINE_CONSTANT(constants, UV_UDP_REUSEADDR);

  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_ACTIVE_CONNECTION_ID_LIMIT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_MAX_STREAM_DATA_BIDI_LOCAL);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_MAX_STREAM_DATA_BIDI_REMOTE);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_MAX_STREAM_DATA_UNI);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_MAX_DATA);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_MAX_STREAMS_BIDI);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_MAX_STREAMS_UNI);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_IDLE_TIMEOUT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_MAX_PACKET_SIZE);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_ACK_DELAY_EXPONENT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_DISABLE_MIGRATION);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_MAX_ACK_DELAY);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_MAX_CRYPTO_BUFFER);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_CONFIG_COUNT);

  NODE_DEFINE_CONSTANT(constants, MIN_MAX_CRYPTO_BUFFER);

  NODE_DEFINE_CONSTANT(
      constants,
      QUICSERVERSESSION_OPTION_REJECT_UNAUTHORIZED);
  NODE_DEFINE_CONSTANT(
      constants,
      QUICSERVERSESSION_OPTION_REQUEST_CERT);
  NODE_DEFINE_CONSTANT(
      constants,
      QUICCLIENTSESSION_OPTION_REQUEST_OCSP);
  NODE_DEFINE_CONSTANT(
      constants,
      QUICCLIENTSESSION_OPTION_VERIFY_HOSTNAME_IDENTITY);
  NODE_DEFINE_CONSTANT(
      constants,
      QUICSOCKET_OPTIONS_VALIDATE_ADDRESS);
  NODE_DEFINE_CONSTANT(
      constants,
      QUICSOCKET_OPTIONS_VALIDATE_ADDRESS_LRU);

  target->Set(context,
              env->constants_string(),
              constants).FromJust();
}

}  // namespace quic
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(quic, node::quic::Initialize)
