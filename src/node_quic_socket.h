#ifndef SRC_NODE_QUIC_SOCKET_H_
#define SRC_NODE_QUIC_SOCKET_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include "node_crypto.h"  // SSLWrap
#include "node_internals.h"
#include "ngtcp2/ngtcp2.h"
#include "node_quic_session.h"
#include "node_quic_util.h"
#include "env.h"
#include "handle_wrap.h"
#include "v8.h"
#include "uv.h"

#include <deque>
#include <map>
#include <string>
#include <vector>

namespace node {

using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Local;
using v8::Object;
using v8::Value;

namespace quic {

static constexpr size_t MAX_VALIDATE_ADDRESS_LRU = 10;

typedef enum QuicSocketOptions : uint32_t {
  // When enabled the QuicSocket will validate the address
  // using a RETRY packet to the peer.
  QUICSOCKET_OPTIONS_VALIDATE_ADDRESS = 0x1,

  // When enabled, and the VALIDATE_ADDRESS option is also
  // set, the QuicSocket will use an LRU cache to track
  // validated addresses. Address validation will be skipped
  // if the address is currently in the cache.
  QUICSOCKET_OPTIONS_VALIDATE_ADDRESS_LRU = 0x2,
} QuicSocketOptions;

class QuicSocket : public HandleWrap,
                   public mem::Tracker {
 public:
  static void Initialize(
      Environment* env,
      Local<Object> target,
      Local<Context> context);

  QuicSocket(
      Environment* env,
      Local<Object> wrap,
      uint64_t retry_token_expiration,
      size_t max_connections_per_host,
      uint32_t options = 0);
  ~QuicSocket() override;

  SocketAddress* GetLocalAddress() { return &local_address_; }

  void Close(
      v8::Local<v8::Value> close_callback = v8::Local<v8::Value>()) override;

  void MaybeClose();

  int AddMembership(
      const char* address,
      const char* iface);
  void AddSession(
      QuicCID* cid,
      std::shared_ptr<QuicSession> session);
  void AssociateCID(
      QuicCID* cid,
      QuicCID* scid);
  int Bind(
      const char* address,
      uint32_t port,
      uint32_t flags,
      int family);
  void DisassociateCID(
      QuicCID* cid);
  int DropMembership(
      const char* address,
      const char* iface);
  void Listen(
      crypto::SecureContext* context,
      const sockaddr* preferred_address = nullptr,
      const std::string& alpn = NGTCP2_ALPN_H3,
      uint32_t options = 0);
  int ReceiveStart();
  int ReceiveStop();
  void RemoveSession(
      QuicCID* cid,
      const sockaddr* addr);
  void ReportSendError(
      int error);
  int SetBroadcast(
      bool on);
  int SetMulticastInterface(
      const char* iface);
  int SetMulticastLoopback(
      bool on);
  int SetMulticastTTL(
      int ttl);
  int SetTTL(
      int ttl);
  int SendPacket(
      const sockaddr* dest,
      QuicBuffer* buf,
      std::shared_ptr<QuicSession> session,
      const char* diagnostic_label = nullptr);
  void SetServerBusy(bool on);
  void SetDiagnosticPacketLoss(double rx = 0.0, double tx = 0.0);
  void StopListening();

  crypto::SecureContext* GetServerSecureContext() {
    return server_secure_context_;
  }

  const uv_udp_t* operator*() const { return &handle_; }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(QuicSocket)
  SET_SELF_SIZE(QuicSocket)

  // Implementation for mem::Tracker
  inline void CheckAllocatedSize(size_t previous_size) override;
  inline void IncrementAllocatedSize(size_t size) override;
  inline void DecrementAllocatedSize(size_t size) override;

 private:
  static void OnAlloc(
      uv_handle_t* handle,
      size_t suggested_size,
      uv_buf_t* buf);

  static void OnRecv(
      uv_udp_t* handle,
      ssize_t nread,
      const uv_buf_t* buf,
      const struct sockaddr* addr,
      unsigned int flags);

  void Receive(
      ssize_t nread,
      const uv_buf_t* buf,
      const struct sockaddr* addr,
      unsigned int flags);

  void SendInitialConnectionClose(
      uint32_t version,
      uint64_t error_code,
      QuicCID* dcid,
      const sockaddr* addr);

  void SendVersionNegotiation(
      uint32_t version,
      QuicCID* dcid,
      QuicCID* scid,
      const sockaddr* addr);

  void OnSend(
      int status,
      size_t length,
      const char* diagnostic_label);

  void SetValidatedAddress(const sockaddr* addr);

  bool IsValidatedAddress(const sockaddr* addr);

  std::shared_ptr<QuicSession> AcceptInitialPacket(
      uint32_t version,
      QuicCID* dcid,
      QuicCID* scid,
      ssize_t nread,
      const uint8_t* data,
      const struct sockaddr* addr,
      unsigned int flags);
  ssize_t SendRetry(
      uint32_t version,
      QuicCID* dcid,
      QuicCID* scid,
      const sockaddr* addr);

  void IncrementSocketAddressCounter(const sockaddr* addr);
  void DecrementSocketAddressCounter(const sockaddr* addr);
  size_t GetCurrentSocketAddressCounter(const sockaddr* addr);

  void IncrementPendingCallbacks() { pending_callbacks_++; }
  void DecrementPendingCallbacks() { pending_callbacks_--; }
  bool HasPendingCallbacks() { return pending_callbacks_ > 0; }

  template <typename T,
            int (*F)(const typename T::HandleType*, sockaddr*, int*)>
  friend void node::GetSockOrPeerName(
      const v8::FunctionCallbackInfo<v8::Value>&);

  // Returns true if, and only if, diagnostic packet loss is enabled
  // and the current packet should be artificially considered lost.
  bool IsDiagnosticPacketLoss(double prob);

  // Fields and TypeDefs
  typedef uv_udp_t HandleType;

  typedef enum QuicSocketFlags : uint32_t {
    QUICSOCKET_FLAGS_NONE = 0x0,

    // Indicates that the QuicSocket has entered a graceful
    // closing phase, indicating that no additional
    QUICSOCKET_FLAGS_GRACEFUL_CLOSE = 0x1,
    QUICSOCKET_FLAGS_PENDING_CLOSE = 0x2,
    QUICSOCKET_FLAGS_SERVER_LISTENING = 0x4,
    QUICSOCKET_FLAGS_SERVER_BUSY = 0x8,
  } QuicSocketFlags;

  void SetFlag(QuicSocketFlags flag, bool on = true) {
    if (on)
      flags_ |= flag;
    else
      flags_ &= ~flag;
  }

  bool IsFlagSet(QuicSocketFlags flag) {
    return flags_ & flag;
  }

  void SetOption(QuicSocketOptions option, bool on = true) {
    if (on)
      options_ |= option;
    else
      options_ &= ~option;
  }

  bool IsOptionSet(QuicSocketOptions option) {
    return options_ & option;
  }

  uv_udp_t handle_;
  uint32_t flags_;
  uint32_t options_;
  uint32_t server_options_;

  size_t pending_callbacks_;
  size_t max_connections_per_host_;
  size_t current_ngtcp2_memory_;

  uint64_t retry_token_expiration_;

  // Used to specify diagnostic packet loss probabilities
  double rx_loss_;
  double tx_loss_;

  SocketAddress local_address_;
  QuicSessionConfig server_session_config_;
  crypto::SecureContext* server_secure_context_;
  std::string server_alpn_;
  std::unordered_map<std::string, std::shared_ptr<QuicSession>> sessions_;
  std::unordered_map<std::string, std::string> dcid_to_scid_;
  std::array<uint8_t, TOKEN_SECRETLEN> token_secret_;

  // Counts the number of active connections per remote
  // address. A custom std::hash specialization for
  // sockaddr instances is used. Values are incremented
  // when a QuicSession is added to the socket, and
  // decremented when the QuicSession is removed. If the
  // value reaches the value of max_connections_per_host_,
  // attempts to create new connections will be ignored
  // until the value falls back below the limit.
  std::unordered_map<const sockaddr*, size_t, SocketAddress::Hash>
    addr_counts_;

  // The validated_addrs_ vector is used as an LRU cache for
  // validated addresses only when the VALIDATE_ADDRESS_LRU
  // option is set.
  typedef size_t SocketAddressHash;
  std::deque<SocketAddressHash> validated_addrs_;

  struct socket_stats {
    // The timestamp at which the socket was created
    uint64_t created_at;
    // The timestamp at which the socket was bound
    uint64_t bound_at;
    // The timestamp at which the socket began listening
    uint64_t listen_at;
    // The total number of bytes received (and not ignored)
    // by this QuicSocket instance.
    uint64_t bytes_received;

    // The total number of bytes successfully sent by this
    // QuicSocket instance.
    uint64_t bytes_sent;

    // The total number of packets received (and not ignored)
    // by this QuicSocket instance.
    uint64_t packets_received;

    // The total number of packets ignored by this QuicSocket
    // instance. Packets are ignored if they are invalid in
    // some way. A high number of ignored packets could signal
    // a buggy or malicious peer.
    uint64_t packets_ignored;

    // The total number of packets successfully sent by this
    // QuicSocket instance.
    uint64_t packets_sent;

    // The total number of QuicServerSessions that have been
    // associated with this QuicSocket instance.
    uint64_t server_sessions;

    // The total number of QuicClientSessions that have been
    // associated with this QuicSocket instance.
    uint64_t client_sessions;
  };
  socket_stats socket_stats_{0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  AliasedBigUint64Array stats_buffer_;

  template <typename... Members>
  void IncrementSocketStat(
      uint64_t amount,
      socket_stats* a,
      Members... mems) {
    static uint64_t max = std::numeric_limits<uint64_t>::max();
    uint64_t current = access(a, mems...);
    uint64_t delta = std::min(amount, max - current);
    access(a, mems...) += delta;
  }

  class SendWrapBase {
   public:
    SendWrapBase(
        QuicSocket* socket,
        const sockaddr* dest,
        const char* diagnostic_label = nullptr);

    virtual ~SendWrapBase() = default;

    virtual void Done(int status);

    virtual int Send() = 0;

    uv_udp_send_t* operator*() { return &req_; }

    uv_udp_send_t* req() { return &req_; }

    QuicSocket* Socket() { return socket_; }

    SocketAddress* Address() { return &address_; }

    const char* diagnostic_label() const { return diagnostic_label_; }

    static void OnSend(
        uv_udp_send_t* req,
        int status);

    virtual size_t Length() = 0;

    bool IsDiagnosticPacketLoss();

   private:
    uv_udp_send_t req_;
    QuicSocket* socket_;
    SocketAddress address_;
    const char* diagnostic_label_;
  };

  // The SendWrap drains the given QuicBuffer and sends it to the
  // uv_udp_t handle. When the async operation completes, the done_cb
  // is invoked with the status and the user_data forwarded on.
  class SendWrap : public SendWrapBase {
   public:
    SendWrap(
        QuicSocket* socket,
        SocketAddress* dest,
        QuicBuffer* buffer,
        std::shared_ptr<QuicSession> session,
        const char* diagnostic_label = nullptr);

    SendWrap(
        QuicSocket* socket,
        const sockaddr* dest,
        QuicBuffer* buffer,
        std::shared_ptr<QuicSession> session,
        const char* diagnostic_label = nullptr);

    void Done(int status) override;

    int Send() override;

    size_t Length() override { return length_; }

   private:
    QuicBuffer* buffer_;
    std::shared_ptr<QuicSession> session_;
    size_t length_ = 0;
  };

  class SendWrapStack : public SendWrapBase {
   public:
    SendWrapStack(
        QuicSocket* socket,
        const sockaddr* dest,
        size_t len,
        const char* diagnostic_label = nullptr);

    int Send() override;

    uint8_t* buffer() { return *buf_; }

    void SetLength(size_t len) {
      buf_.SetLength(len);
    }

    size_t Length() override { return buf_.length(); }

   private:
    MaybeStackBuffer<uint8_t> buf_;
  };
};

}  // namespace quic
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_QUIC_SOCKET_H_
