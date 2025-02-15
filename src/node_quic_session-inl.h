#ifndef SRC_NODE_QUIC_SESSION_INL_H_
#define SRC_NODE_QUIC_SESSION_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "aliased_buffer.h"
#include "debug_utils.h"
#include "env-inl.h"
#include "node_crypto.h"
#include "node_quic_session.h"
#include "ngtcp2/ngtcp2.h"

#include <algorithm>

namespace node {

using crypto::EntropySource;

namespace quic {

inline void SetConfig(Environment* env, int idx, uint64_t* val) {
  AliasedFloat64Array& buffer = env->quic_state()->quicsessionconfig_buffer;
  uint64_t flags = static_cast<uint64_t>(buffer[IDX_QUIC_SESSION_CONFIG_COUNT]);
  if (flags & (1ULL << idx))
    *val = static_cast<uint64_t>(buffer[idx]);
}

// Forwards detailed(verbose) debugging information from ngtcp2. Enabled using
// the NODE_DEBUG_NATIVE=NGTCP2_DEBUG category.
inline void DebugLog(void* user_data, const char* fmt, ...) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  va_list ap;
  va_start(ap, fmt);
  Debug(session->env(), DebugCategory::NGTCP2_DEBUG, fmt, ap);
  va_end(ap);
}

inline void QuicSessionConfig::ResetToDefaults() {
  ngtcp2_settings_default(&settings_);
  settings_.initial_ts = uv_hrtime();
  settings_.log_printf = DebugLog;
  settings_.active_connection_id_limit = DEFAULT_ACTIVE_CONNECTION_ID_LIMIT;
  settings_.max_stream_data_bidi_local = DEFAULT_MAX_STREAM_DATA_BIDI_LOCAL;
  settings_.max_stream_data_bidi_remote = DEFAULT_MAX_STREAM_DATA_BIDI_REMOTE;
  settings_.max_stream_data_uni = DEFAULT_MAX_STREAM_DATA_UNI;
  settings_.max_data = DEFAULT_MAX_DATA;
  settings_.max_streams_bidi = DEFAULT_MAX_STREAMS_BIDI;
  settings_.max_streams_uni = DEFAULT_MAX_STREAMS_UNI;
  settings_.idle_timeout = DEFAULT_IDLE_TIMEOUT;
  settings_.max_packet_size = NGTCP2_MAX_PKT_SIZE;
  settings_.max_ack_delay = NGTCP2_DEFAULT_MAX_ACK_DELAY;
  settings_.disable_migration = 0;
  settings_.preferred_address_present = 0;
  settings_.stateless_reset_token_present = 0;
  max_crypto_buffer_ = DEFAULT_MAX_CRYPTO_BUFFER;
}

// Sets the QuicSessionConfig using an AliasedBuffer for efficiency.
inline void QuicSessionConfig::Set(
    Environment* env,
    const sockaddr* preferred_addr) {
  ResetToDefaults();

  SetConfig(env, IDX_QUIC_SESSION_ACTIVE_CONNECTION_ID_LIMIT,
            &settings_.active_connection_id_limit);
  SetConfig(env, IDX_QUIC_SESSION_MAX_STREAM_DATA_BIDI_LOCAL,
            &settings_.max_stream_data_bidi_local);
  SetConfig(env, IDX_QUIC_SESSION_MAX_STREAM_DATA_BIDI_REMOTE,
            &settings_.max_stream_data_bidi_remote);
  SetConfig(env, IDX_QUIC_SESSION_MAX_STREAM_DATA_UNI,
            &settings_.max_stream_data_uni);
  SetConfig(env, IDX_QUIC_SESSION_MAX_DATA,
            &settings_.max_data);
  SetConfig(env, IDX_QUIC_SESSION_MAX_STREAMS_BIDI,
            &settings_.max_streams_bidi);
  SetConfig(env, IDX_QUIC_SESSION_MAX_STREAMS_UNI,
            &settings_.max_streams_uni);
  SetConfig(env, IDX_QUIC_SESSION_IDLE_TIMEOUT,
            &settings_.idle_timeout);
  SetConfig(env, IDX_QUIC_SESSION_MAX_PACKET_SIZE,
            &settings_.max_packet_size);
  SetConfig(env, IDX_QUIC_SESSION_MAX_ACK_DELAY,
            &settings_.max_ack_delay);

  SetConfig(env, IDX_QUIC_SESSION_MAX_CRYPTO_BUFFER,
            &max_crypto_buffer_);
  max_crypto_buffer_ = std::max(max_crypto_buffer_, MIN_MAX_CRYPTO_BUFFER);

  if (preferred_addr != nullptr) {
    settings_.preferred_address_present = 1;
    switch (preferred_addr->sa_family) {
      case AF_INET: {
        auto& dest = settings_.preferred_address.ipv4_addr;
        memcpy(
            &dest,
            &(reinterpret_cast<const sockaddr_in*>(preferred_addr)->sin_addr),
            sizeof(dest));
        settings_.preferred_address.ipv4_port =
            SocketAddress::GetPort(preferred_addr);
        break;
      }
      case AF_INET6: {
        auto& dest = settings_.preferred_address.ipv6_addr;
        memcpy(
            &dest,
            &(reinterpret_cast<const sockaddr_in6*>(preferred_addr)->sin6_addr),
            sizeof(dest));
        settings_.preferred_address.ipv6_port =
            SocketAddress::GetPort(preferred_addr);
        break;
      }
      default:
        UNREACHABLE();
    }
  }
}

inline void QuicSessionConfig::GenerateStatelessResetToken() {
  settings_.stateless_reset_token_present = 1;
  EntropySource(
      settings_.stateless_reset_token,
      arraysize(settings_.stateless_reset_token));
}

inline void QuicSessionConfig::GeneratePreferredAddressToken(
    ngtcp2_cid* pscid) {
  if (!settings_.preferred_address_present)
    return;
  EntropySource(
      settings_.preferred_address.stateless_reset_token,
      arraysize(settings_.preferred_address.stateless_reset_token));

  pscid->datalen = NGTCP2_SV_SCIDLEN;
  EntropySource(pscid->data, pscid->datalen);
  settings_.preferred_address.cid = *pscid;
}


inline void QuicSession::CheckAllocatedSize(size_t previous_size) {
  CHECK_GE(current_ngtcp2_memory_, previous_size);
}

inline void QuicSession::IncrementAllocatedSize(size_t size) {
  current_ngtcp2_memory_ += size;
}

inline void QuicSession::DecrementAllocatedSize(size_t size) {
  current_ngtcp2_memory_ -= size;
}


inline void QuicSession::OnIdleTimeout(uv_timer_t* timer) {
  QuicSession* session = static_cast<QuicSession*>(timer->data);
  CHECK_NOT_NULL(session);
  session->OnIdleTimeout();
}

// Static ngtcp2 callbacks are registered when ngtcp2 when a new ngtcp2_conn is
// created. These are static functions that, for the most part, simply defer to
// a QuicSession instance that is passed through as user_data.

// Called by ngtcp2 upon creation of a new client connection
// to initiate the TLS handshake.
inline int QuicSession::OnClientInitial(
    ngtcp2_conn* conn,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  return session->TLSHandshake() == 0 ? 0 : NGTCP2_ERR_CALLBACK_FAILURE;
}

// Called by ngtcp2 for a new server connection when the initial
// crypto handshake from the client has been received.
inline int QuicSession::OnReceiveClientInitial(
    ngtcp2_conn* conn,
    const ngtcp2_cid* dcid,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  return session->ReceiveClientInitial(dcid) ?
      0 : NGTCP2_ERR_CALLBACK_FAILURE;
}

// Called by ngtcp2 for both client and server connections when
// TLS handshake data has been received.
inline int QuicSession::OnReceiveCryptoData(
    ngtcp2_conn* conn,
    ngtcp2_crypto_level crypto_level,
    uint64_t offset,
    const uint8_t* data,
    size_t datalen,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  return static_cast<int>(
    session->ReceiveCryptoData(crypto_level, offset, data, datalen));
}

// Called by ngtcp2 for a client connection when the server has
// sent a retry packet.
inline int QuicSession::OnReceiveRetry(
    ngtcp2_conn* conn,
    const ngtcp2_pkt_hd* hd,
    const ngtcp2_pkt_retry* retry,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  return session->ReceiveRetry() ? 0 : NGTCP2_ERR_CALLBACK_FAILURE;
}

// Called by ngtcp2 for both client and server connections
// when a request to extend the maximum number of bidirectional
// streams has been received.
inline int QuicSession::OnExtendMaxStreamsBidi(
    ngtcp2_conn* conn,
    uint64_t max_streams,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->ExtendMaxStreamsBidi(max_streams);
  return 0;
}

// Called by ngtcp2 for both client and server connections
// when a request to extend the maximum number of unidirectional
// streams has been received
inline int QuicSession::OnExtendMaxStreamsUni(
    ngtcp2_conn* conn,
    uint64_t max_streams,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->ExtendMaxStreamsUni(max_streams);
  return 0;
}

inline int QuicSession::OnExtendMaxStreamData(
    ngtcp2_conn* conn,
    int64_t stream_id,
    uint64_t max_data,
    void* user_data,
    void* stream_user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->ExtendMaxStreamData(stream_id, max_data);
  return 0;
}

// Called by ngtcp2 for both client and server connections
// when ngtcp2 has determined that the TLS handshake has
// been completed.
inline int QuicSession::OnHandshakeCompleted(
    ngtcp2_conn* conn,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->HandshakeCompleted();
  return 0;
}

// Called by ngtcp2 when TLS handshake data needs to be
// encrypted prior to sending.
inline ssize_t QuicSession::OnDoHSEncrypt(
    ngtcp2_conn* conn,
    uint8_t* dest,
    size_t destlen,
    const uint8_t* plaintext,
    size_t plaintextlen,
    const uint8_t* key,
    size_t keylen,
    const uint8_t* nonce,
    size_t noncelen,
    const uint8_t* ad,
    size_t adlen,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  return session->DoHSEncrypt(
      dest, destlen,
      plaintext, plaintextlen,
      key, keylen,
      nonce, noncelen,
      ad, adlen);
}

// Called by ngtcp2 when encrypted TLS handshake data has
// been received.
inline ssize_t QuicSession::OnDoHSDecrypt(
    ngtcp2_conn* conn,
    uint8_t* dest,
    size_t destlen,
    const uint8_t* ciphertext,
    size_t ciphertextlen,
    const uint8_t* key,
    size_t keylen,
    const uint8_t* nonce,
    size_t noncelen,
    const uint8_t* ad,
    size_t adlen,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  return session->DoHSDecrypt(
      dest, destlen,
      ciphertext, ciphertextlen,
      key, keylen,
      nonce, noncelen,
      ad, adlen);
}

// Called by ngtcp2 when non-TLS handshake data needs to be
// encrypted prior to sending.
inline ssize_t QuicSession::OnDoEncrypt(
    ngtcp2_conn* conn,
    uint8_t* dest,
    size_t destlen,
    const uint8_t* plaintext,
    size_t plaintextlen,
    const uint8_t* key,
    size_t keylen,
    const uint8_t* nonce,
    size_t noncelen,
    const uint8_t* ad,
    size_t adlen,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  return session->DoEncrypt(
      dest, destlen,
      plaintext, plaintextlen,
      key, keylen,
      nonce, noncelen,
      ad, adlen);
}

// Called by ngtcp2 when encrypted non-TLS handshake data
// has been received.
inline ssize_t QuicSession::OnDoDecrypt(
    ngtcp2_conn* conn,
    uint8_t* dest,
    size_t destlen,
    const uint8_t* ciphertext,
    size_t ciphertextlen,
    const uint8_t* key,
    size_t keylen,
    const uint8_t* nonce,
    size_t noncelen,
    const uint8_t* ad,
    size_t adlen,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  return session->DoDecrypt(
      dest, destlen,
      ciphertext, ciphertextlen,
      key, keylen,
      nonce, noncelen,
      ad, adlen);
}

inline ssize_t QuicSession::OnDoInHPMask(
    ngtcp2_conn* conn,
    uint8_t* dest,
    size_t destlen,
    const uint8_t* key,
    size_t keylen,
    const uint8_t* sample,
    size_t samplelen,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  return session->DoInHPMask(
      dest, destlen,
      key, keylen,
      sample, samplelen);
}

inline ssize_t QuicSession::OnDoHPMask(
    ngtcp2_conn* conn,
    uint8_t* dest,
    size_t destlen,
    const uint8_t* key,
    size_t keylen,
    const uint8_t* sample,
    size_t samplelen,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  return session->DoHPMask(
      dest, destlen,
      key, keylen,
      sample, samplelen);
}

// Called by ngtcp2 when a chunk of stream data has been received.
inline int QuicSession::OnReceiveStreamData(
    ngtcp2_conn* conn,
    int64_t stream_id,
    int fin,
    uint64_t offset,
    const uint8_t* data,
    size_t datalen,
    void* user_data,
    void* stream_user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->ReceiveStreamData(stream_id, fin, data, datalen, offset);
  return 0;
}

// Called by ngtcp2 when a new stream has been opened
inline int QuicSession::OnStreamOpen(
    ngtcp2_conn* conn,
    int64_t stream_id,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  session->StreamOpen(stream_id);
  return 0;
}

// Called by ngtcp2 when an acknowledgement for a chunk of
// TLS handshake data has been received.
inline int QuicSession::OnAckedCryptoOffset(
    ngtcp2_conn* conn,
    ngtcp2_crypto_level crypto_level,
    uint64_t offset,
    size_t datalen,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->AckedCryptoOffset(datalen);
  return 0;
}

// Called by ngtcp2 when an acknowledgement for a chunk of
// stream data has been received.
inline int QuicSession::OnAckedStreamDataOffset(
    ngtcp2_conn* conn,
    int64_t stream_id,
    uint64_t offset,
    size_t datalen,
    void* user_data,
    void* stream_user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->AckedStreamDataOffset(stream_id, offset, datalen);
  return 0;
}

// Called by ngtcp2 for a client connection when the server
// has indicated a preferred address in the transport
// params.
// For now, there are two modes: we can accept the preferred address
// or we can reject it. Later, we may want to implement a callback
// to ask the user if they want to accept the preferred address or
// not.
inline int QuicSession::OnSelectPreferredAddress(
    ngtcp2_conn* conn,
    ngtcp2_addr* dest,
    const ngtcp2_preferred_addr* paddr,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  return session->SelectPreferredAddress(dest, paddr) ?
      0 : NGTCP2_ERR_CALLBACK_FAILURE;
}

// Called by ngtcp2 when a stream has been closed for any reason.
inline int QuicSession::OnStreamClose(
    ngtcp2_conn* conn,
    int64_t stream_id,
    uint64_t app_error_code,
    void* user_data,
    void* stream_user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->StreamClose(stream_id, app_error_code);
  return 0;
}

inline int QuicSession::OnStreamReset(
    ngtcp2_conn* conn,
    int64_t stream_id,
    uint64_t final_size,
    uint64_t app_error_code,
    void* user_data,
    void* stream_user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->StreamReset(stream_id, final_size, app_error_code);
  return 0;
}

// Called by ngtcp2 when it needs to generate some random data
inline int QuicSession::OnRand(
    ngtcp2_conn* conn,
    uint8_t* dest,
    size_t destlen,
    ngtcp2_rand_ctx ctx,
    void* user_data) {
  EntropySource(dest, destlen);
  return 0;
}

// When a new client connection is established, ngtcp2 will call
// this multiple times to generate a pool of connection IDs to use.
inline int QuicSession::OnGetNewConnectionID(
    ngtcp2_conn* conn,
    ngtcp2_cid* cid,
    uint8_t* token,
    size_t cidlen,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->GetNewConnectionID(cid, token, cidlen);
  return 0;
}

// Called by ngtcp2 to trigger a key update for the connection.
inline int QuicSession::OnUpdateKey(
    ngtcp2_conn* conn,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  return session->UpdateKey() ? 0 : NGTCP2_ERR_CALLBACK_FAILURE;
}

// When a connection is closed, ngtcp2 will call this multiple
// times to remove connection IDs.
inline int QuicSession::OnRemoveConnectionID(
    ngtcp2_conn* conn,
    const ngtcp2_cid* cid,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->RemoveConnectionID(cid);
  return 0;
}

// Called by ngtcp2 to perform path validation. Path validation
// is necessary to ensure that a packet is originating from the
// expected source.
inline int QuicSession::OnPathValidation(
    ngtcp2_conn* conn,
    const ngtcp2_path* path,
    ngtcp2_path_validation_result res,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->PathValidation(path, res);
  return 0;
}

inline int QuicSession::OnVersionNegotiation(
    ngtcp2_conn* conn,
    const ngtcp2_pkt_hd* hd,
    const uint32_t* sv,
    size_t nsv,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->VersionNegotiation(hd, sv, nsv);
  return 0;
}

inline void QuicSession::OnKeylog(const SSL* ssl, const char* line) {
  QuicSession* session = static_cast<QuicSession*>(SSL_get_app_data(ssl));
  session->Keylog(line);
}

inline int QuicSession::OnStatelessReset(
    ngtcp2_conn* conn,
    const ngtcp2_pkt_stateless_reset* sr,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  session->SilentClose(true);
  return 0;
}

inline void QuicSession::SetTLSAlert(int err) {
  SetLastError(InitQuicError(QUIC_ERROR_CRYPTO, err));
}

inline void QuicSession::SetLastError(QuicError error) {
  last_error_ = error;
}

inline void QuicSession::SetLastError(QuicErrorFamily family, uint64_t code) {
  last_error_.family = family;
  last_error_.code = code;
}

inline void QuicSession::SetLastError(QuicErrorFamily family, int code) {
  SetLastError(family, ngtcp2_err_infer_quic_transport_error_code(code));
}

inline bool QuicSession::IsInClosingPeriod() {
  return ngtcp2_conn_is_in_closing_period(Connection());
}

inline bool QuicSession::IsInDrainingPeriod() {
  return ngtcp2_conn_is_in_draining_period(Connection());
}

// Locate the QuicStream with the given id or return nullptr
inline QuicStream* QuicSession::FindStream(int64_t id) {
  auto it = streams_.find(id);
  if (it == std::end(streams_))
    return nullptr;
  return (*it).second.get();
}

inline bool QuicSession::HasStream(int64_t id) {
  return streams_.find(id) != std::end(streams_);
}

inline QuicError QuicSession::GetLastError() { return last_error_; }

inline bool QuicSession::IsGracefullyClosing() {
  return IsFlagSet(QUICSESSION_FLAG_GRACEFUL_CLOSING);
}

inline bool QuicSession::IsDestroyed() {
  return IsFlagSet(QUICSESSION_FLAG_DESTROYED);
}

inline void QuicSession::StartGracefulClose() {
  SetFlag(QUICSESSION_FLAG_GRACEFUL_CLOSING);
  session_stats_.closing_at = uv_hrtime();
}

inline void Consume(ngtcp2_vec** pvec, size_t* pcnt, size_t len) {
  ngtcp2_vec* v = *pvec;
  size_t cnt = *pcnt;

  for (; cnt > 0; --cnt, ++v) {
    if (v->len > len) {
      v->len -= len;
      v->base += len;
      break;
    }
    len -= v->len;
  }

  *pvec = v;
  *pcnt = cnt;
}

inline int Empty(const ngtcp2_vec* vec, size_t cnt) {
  size_t i;
  for (i = 0; i < cnt && vec[i].len == 0; ++i) {}
  return i == cnt;
}

inline void QuicSession::OnIdleTimeoutCB(void* data) {
  QuicSession* session = static_cast<QuicSession*>(data);
  session->OnIdleTimeout();
}

inline void QuicSession::OnRetransmitTimeoutCB(void* data) {
  QuicSession* session = static_cast<QuicSession*>(data);
  session->MaybeTimeout();
}

}  // namespace quic
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_QUIC_SESSION_INL_H_
