/*
 * ngtcp2
 *
 * Copyright (c) 2017 ngtcp2 contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef NGTCP2_PKT_H
#define NGTCP2_PKT_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <ngtcp2/ngtcp2.h>

/* QUIC header macros */
#define NGTCP2_HEADER_FORM_BIT 0x80
#define NGTCP2_FIXED_BIT_MASK 0x40
#define NGTCP2_PKT_NUMLEN_MASK 0x03

/* Long header specific macros */
#define NGTCP2_LONG_TYPE_MASK 0x30
#define NGTCP2_LONG_RESERVED_BIT_MASK 0x0c

/* Short header specific macros */
#define NGTCP2_SHORT_SPIN_BIT_MASK 0x20
#define NGTCP2_SHORT_RESERVED_BIT_MASK 0x18
#define NGTCP2_SHORT_KEY_PHASE_BIT 0x04

/* NGTCP2_SR_TYPE is a Type field of Stateless Reset. */
#define NGTCP2_SR_TYPE 0x1f

/* NGTCP2_MIN_LONG_HEADERLEN is the minimum length of long header.
   That is (1|1|TT|RR|PP)<1> + VERSION<4> + DCIL<1> + SCIL<1> +
   LENGTH<1> + PKN<1> */
#define NGTCP2_MIN_LONG_HEADERLEN (1 + 4 + 1 + 1 + 1 + 1)

#define NGTCP2_STREAM_FIN_BIT 0x01
#define NGTCP2_STREAM_LEN_BIT 0x02
#define NGTCP2_STREAM_OFF_BIT 0x04

/* NGTCP2_STREAM_OVERHEAD is the maximum number of bytes required
   other than payload for STREAM frame.  That is from type field to
   the beginning of the payload. */
#define NGTCP2_STREAM_OVERHEAD (1 + 8 + 8 + 8)

/* NGTCP2_CRYPTO_OVERHEAD is the maximum number of bytes required
   other than payload for CRYPTO frame.  That is from type field to
   the beginning of the payload. */
#define NGTCP2_CRYPTO_OVERHEAD (1 + 8 + 8)

/* NGTCP2_MIN_FRAME_PAYLOADLEN is the minimum frame payload length. */
#define NGTCP2_MIN_FRAME_PAYLOADLEN 16

/* NGTCP2_MAX_VARINT is the maximum value which can be encoded in
   variable-length integer encoding */
#define NGTCP2_MAX_VARINT ((1ULL << 62) - 1)

/* NGTCP2_MAX_SERVER_STREAM_ID_BIDI is the maximum bidirectional
   server stream ID. */
#define NGTCP2_MAX_SERVER_STREAM_ID_BIDI ((int64_t)0x3ffffffffffffffdll)
/* NGTCP2_MAX_CLIENT_STREAM_ID_BIDI is the maximum bidirectional
   client stream ID. */
#define NGTCP2_MAX_CLIENT_STREAM_ID_BIDI ((int64_t)0x3ffffffffffffffcll)
/* NGTCP2_MAX_SERVER_STREAM_ID_UNI is the maximum unidirectional
   server stream ID. */
#define NGTCP2_MAX_SERVER_STREAM_ID_UNI ((int64_t)0x3fffffffffffffffll)
/* NGTCP2_MAX_CLIENT_STREAM_ID_UNI is the maximum unidirectional
   client stream ID. */
#define NGTCP2_MAX_CLIENT_STREAM_ID_UNI ((int64_t)0x3ffffffffffffffell)

/* NGTCP2_MAX_NUM_ACK_BLK is the maximum number of Additional ACK
   blocks which this library can create, or decode. */
#define NGTCP2_MAX_ACK_BLKS 255

/* NGTCP2_MAX_PKT_NUM is the maximum packet number. */
#define NGTCP2_MAX_PKT_NUM ((int64_t)((1ll << 62) - 1))

typedef enum {
  NGTCP2_FRAME_PADDING = 0x00,
  NGTCP2_FRAME_PING = 0x01,
  NGTCP2_FRAME_ACK = 0x02,
  NGTCP2_FRAME_ACK_ECN = 0x03,
  NGTCP2_FRAME_RESET_STREAM = 0x04,
  NGTCP2_FRAME_STOP_SENDING = 0x05,
  NGTCP2_FRAME_CRYPTO = 0x06,
  NGTCP2_FRAME_NEW_TOKEN = 0x07,
  NGTCP2_FRAME_STREAM = 0x08,
  NGTCP2_FRAME_MAX_DATA = 0x10,
  NGTCP2_FRAME_MAX_STREAM_DATA = 0x11,
  NGTCP2_FRAME_MAX_STREAMS_BIDI = 0x12,
  NGTCP2_FRAME_MAX_STREAMS_UNI = 0x13,
  NGTCP2_FRAME_DATA_BLOCKED = 0x14,
  NGTCP2_FRAME_STREAM_DATA_BLOCKED = 0x15,
  NGTCP2_FRAME_STREAMS_BLOCKED_BIDI = 0x16,
  NGTCP2_FRAME_STREAMS_BLOCKED_UNI = 0x17,
  NGTCP2_FRAME_NEW_CONNECTION_ID = 0x18,
  NGTCP2_FRAME_RETIRE_CONNECTION_ID = 0x19,
  NGTCP2_FRAME_PATH_CHALLENGE = 0x1a,
  NGTCP2_FRAME_PATH_RESPONSE = 0x1b,
  NGTCP2_FRAME_CONNECTION_CLOSE = 0x1c,
  NGTCP2_FRAME_CONNECTION_CLOSE_APP = 0x1d,
} ngtcp2_frame_type;

typedef struct {
  uint8_t type;
  /**
   * flags of decoded STREAM frame.  This gets ignored when encoding
   * STREAM frame.
   */
  uint8_t flags;
  uint8_t fin;
  int64_t stream_id;
  uint64_t offset;
  /* datacnt is the number of elements that data contains.  Although
     the length of data is 1 in this definition, the library may
     allocate extra bytes to hold more elements. */
  size_t datacnt;
  /* data is the array of ngtcp2_vec which references data. */
  ngtcp2_vec data[1];
} ngtcp2_stream;

typedef struct {
  uint64_t gap;
  uint64_t blklen;
} ngtcp2_ack_blk;

typedef struct {
  uint8_t type;
  int64_t largest_ack;
  uint64_t ack_delay;
  /**
   * ack_delay_unscaled is an ack_delay multiplied by
   * 2**ack_delay_component * NGTCP2_DURATION_TICK /
   * NGTCP2_MICROSECONDS.
   */
  ngtcp2_duration ack_delay_unscaled;
  uint64_t first_ack_blklen;
  size_t num_blks;
  ngtcp2_ack_blk blks[1];
} ngtcp2_ack;

typedef struct {
  uint8_t type;
  /**
   * The length of contiguous PADDING frames.
   */
  size_t len;
} ngtcp2_padding;

typedef struct {
  uint8_t type;
  int64_t stream_id;
  uint64_t app_error_code;
  uint64_t final_size;
} ngtcp2_reset_stream;

typedef struct {
  uint8_t type;
  uint64_t error_code;
  uint64_t frame_type;
  size_t reasonlen;
  uint8_t *reason;
} ngtcp2_connection_close;

typedef struct {
  uint8_t type;
  /**
   * max_data is Maximum Data.
   */
  uint64_t max_data;
} ngtcp2_max_data;

typedef struct {
  uint8_t type;
  int64_t stream_id;
  uint64_t max_stream_data;
} ngtcp2_max_stream_data;

typedef struct {
  uint8_t type;
  uint64_t max_streams;
} ngtcp2_max_streams;

typedef struct {
  uint8_t type;
} ngtcp2_ping;

typedef struct {
  uint8_t type;
  uint64_t offset;
} ngtcp2_data_blocked;

typedef struct {
  uint8_t type;
  int64_t stream_id;
  uint64_t offset;
} ngtcp2_stream_data_blocked;

typedef struct {
  uint8_t type;
  uint64_t stream_limit;
} ngtcp2_streams_blocked;

typedef struct {
  uint8_t type;
  uint64_t seq;
  uint64_t retire_prior_to;
  ngtcp2_cid cid;
  uint8_t stateless_reset_token[NGTCP2_STATELESS_RESET_TOKENLEN];
} ngtcp2_new_connection_id;

typedef struct {
  uint8_t type;
  int64_t stream_id;
  uint64_t app_error_code;
} ngtcp2_stop_sending;

typedef struct {
  uint8_t type;
  uint8_t data[8];
} ngtcp2_path_challenge;

typedef struct {
  uint8_t type;
  uint8_t data[8];
} ngtcp2_path_response;

typedef struct {
  uint8_t type;
  uint64_t offset;
  /* datacnt is the number of elements that data contains.  Although
     the length of data is 1 in this definition, the library may
     allocate extra bytes to hold more elements. */
  size_t datacnt;
  /* data is the array of ngtcp2_vec which references data. */
  ngtcp2_vec data[1];
} ngtcp2_crypto;

typedef struct {
  uint8_t type;
  size_t tokenlen;
  const uint8_t *token;
} ngtcp2_new_token;

typedef struct {
  uint8_t type;
  uint64_t seq;
} ngtcp2_retire_connection_id;

typedef union {
  uint8_t type;
  ngtcp2_stream stream;
  ngtcp2_ack ack;
  ngtcp2_padding padding;
  ngtcp2_reset_stream reset_stream;
  ngtcp2_connection_close connection_close;
  ngtcp2_max_data max_data;
  ngtcp2_max_stream_data max_stream_data;
  ngtcp2_max_streams max_streams;
  ngtcp2_ping ping;
  ngtcp2_data_blocked data_blocked;
  ngtcp2_stream_data_blocked stream_data_blocked;
  ngtcp2_streams_blocked streams_blocked;
  ngtcp2_new_connection_id new_connection_id;
  ngtcp2_stop_sending stop_sending;
  ngtcp2_path_challenge path_challenge;
  ngtcp2_path_response path_response;
  ngtcp2_crypto crypto;
  ngtcp2_new_token new_token;
  ngtcp2_retire_connection_id retire_connection_id;
} ngtcp2_frame;

struct ngtcp2_pkt_chain;
typedef struct ngtcp2_pkt_chain ngtcp2_pkt_chain;

/*
 * ngtcp2_pkt_chain is the chain of incoming packets buffered.
 */
struct ngtcp2_pkt_chain {
  ngtcp2_path_storage path;
  ngtcp2_pkt_chain *next;
  uint8_t *pkt;
  size_t pktlen;
  ngtcp2_tstamp ts;
};

/*
 * ngtcp2_pkt_chain_new allocates ngtcp2_pkt_chain objects, and
 * assigns its pointer to |*ppc|.  The content of buffer pointed by
 * |pkt| of length |pktlen| is copied into |*ppc|.  The packet is
 * obtained via the network |path|.  The values of path->local and
 * path->remote are copied into |*ppc|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 */
int ngtcp2_pkt_chain_new(ngtcp2_pkt_chain **ppc, const ngtcp2_path *path,
                         const uint8_t *pkt, size_t pktlen, ngtcp2_tstamp ts,
                         const ngtcp2_mem *mem);

/*
 * ngtcp2_pkt_chain_del deallocates |pc|.  It also frees the memory
 * pointed by |pc|.
 */
void ngtcp2_pkt_chain_del(ngtcp2_pkt_chain *pc, const ngtcp2_mem *mem);

/*
 * ngtcp2_pkt_hd_init initializes |hd| with the given values.  If
 * |dcid| and/or |scid| is NULL, DCID and SCID of |hd| is empty
 * respectively.  |pkt_numlen| is the number of bytes used to encode
 * |pkt_num| and either 1, 2, or 4.
 */
void ngtcp2_pkt_hd_init(ngtcp2_pkt_hd *hd, uint8_t flags, uint8_t type,
                        const ngtcp2_cid *dcid, const ngtcp2_cid *scid,
                        int64_t pkt_num, size_t pkt_numlen, uint32_t version,
                        size_t len);

/*
 * ngtcp2_pkt_encode_hd_long encodes |hd| as QUIC long header into
 * |out| which has length |outlen|.  It returns the number of bytes
 * written into |outlen| if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOBUF
 *     Buffer is too short
 */
ssize_t ngtcp2_pkt_encode_hd_long(uint8_t *out, size_t outlen,
                                  const ngtcp2_pkt_hd *hd);

/*
 * ngtcp2_pkt_encode_hd_short encodes |hd| as QUIC short header into
 * |out| which has length |outlen|.  It returns the number of bytes
 * written into |outlen| if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOBUF
 *     Buffer is too short
 */
ssize_t ngtcp2_pkt_encode_hd_short(uint8_t *out, size_t outlen,
                                   const ngtcp2_pkt_hd *hd);

/**
 * @function
 *
 * `ngtcp2_pkt_decode_frame` decodes a QUIC frame from the buffer
 * pointed by |payload| whose length is |payloadlen|.
 *
 * This function returns the number of bytes read to decode a single
 * frame if it succeeds, or one of the following negative error codes:
 *
 * :enum:`NGTCP2_ERR_FRAME_ENCODING`
 *     Frame is badly formatted; or frame type is unknown.
 */
ssize_t ngtcp2_pkt_decode_frame(ngtcp2_frame *dest, const uint8_t *payload,
                                size_t payloadlen);

/**
 * @function
 *
 * `ngtcp2_pkt_encode_frame` encodes a frame |fm| into the buffer
 * pointed by |out| of length |outlen|.
 *
 * This function returns the number of bytes written to the buffer, or
 * one of the following negative error codes:
 *
 * :enum:`NGTCP2_ERR_NOBUF`
 *     Buffer does not have enough capacity to write a frame.
 */
ssize_t ngtcp2_pkt_encode_frame(uint8_t *out, size_t outlen, ngtcp2_frame *fr);

/*
 * ngtcp2_pkt_decode_version_negotiation decodes Version Negotiation
 * packet payload |payload| of length |payloadlen|, and stores the
 * result in |dest|.  |dest| must have enough capacity to store the
 * result.  |payloadlen| also must be a multiple of sizeof(uint32_t).
 *
 * This function returns the number of versions written in |dest|.
 */
size_t ngtcp2_pkt_decode_version_negotiation(uint32_t *dest,
                                             const uint8_t *payload,
                                             size_t payloadlen);

/*
 * ngtcp2_pkt_decode_stateless_reset decodes Stateless Reset payload
 * |payload| of length |payloadlen|.  The |payload| must start with
 * Stateless Reset Token.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_INVALID_ARGUMENT
 *     Payloadlen is too short.
 */
int ngtcp2_pkt_decode_stateless_reset(ngtcp2_pkt_stateless_reset *sr,
                                      const uint8_t *payload,
                                      size_t payloadlen);

/*
 * ngtcp2_pkt_decode_retry decodes Retry packet payload |payload| of
 * length |payloadlen|.  The |payload| must start with ODCID Len
 * field.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_INVALID_ARGUMENT
 *     Payloadlen is too short.
 */
int ngtcp2_pkt_decode_retry(ngtcp2_pkt_retry *dest, const uint8_t *payload,
                            size_t payloadlen);

/*
 * ngtcp2_pkt_decode_stream_frame decodes STREAM frame from |payload|
 * of length |payloadlen|.  The result is stored in the object pointed
 * by |dest|.  STREAM frame must start at payload[0].  This function
 * finishes when it decodes one STREAM frame, and returns the exact
 * number of bytes read to decode a frame if it succeeds, or one of
 * the following negative error codes:
 *
 * NGTCP2_ERR_FRAME_ENCODING
 *     Payload is too short to include STREAM frame.
 */
ssize_t ngtcp2_pkt_decode_stream_frame(ngtcp2_stream *dest,
                                       const uint8_t *payload,
                                       size_t payloadlen);

/*
 * ngtcp2_pkt_decode_ack_frame decodes ACK frame from |payload| of
 * length |payloadlen|.  The result is stored in the object pointed by
 * |dest|.  ACK frame must start at payload[0].  This function
 * finishes when it decodes one ACK frame, and returns the exact
 * number of bytes read to decode a frame if it succeeds, or one of
 * the following negative error codes:
 *
 * NGTCP2_ERR_FRAME_ENCODING
 *     Payload is too short to include ACK frame.
 */
ssize_t ngtcp2_pkt_decode_ack_frame(ngtcp2_ack *dest, const uint8_t *payload,
                                    size_t payloadlen);

/*
 * ngtcp2_pkt_decode_padding_frame decodes contiguous PADDING frames
 * from |payload| of length |payloadlen|.  It continues to parse
 * frames as long as the frame type is PADDING.  This finishes when it
 * encounters the frame type which is not PADDING, or all input data
 * is read.  The first byte (payload[0]) must be NGTCP2_FRAME_PADDING.
 * This function returns the exact number of bytes read to decode
 * PADDING frames.
 */
size_t ngtcp2_pkt_decode_padding_frame(ngtcp2_padding *dest,
                                       const uint8_t *payload,
                                       size_t payloadlen);

/*
 * ngtcp2_pkt_decode_reset_stream_frame decodes RESET_STREAM frame
 * from |payload| of length |payloadlen|.  The result is stored in the
 * object pointed by |dest|.  RESET_STREAM frame must start at
 * payload[0].  This function finishes when it decodes one
 * RESET_STREAM frame, and returns the exact number of bytes read to
 * decode a frame if it succeeds, or one of the following negative
 * error codes:
 *
 * NGTCP2_ERR_FRAME_ENCODING
 *     Payload is too short to include RESET_STREAM frame.
 */
ssize_t ngtcp2_pkt_decode_reset_stream_frame(ngtcp2_reset_stream *dest,
                                             const uint8_t *payload,
                                             size_t payloadlen);

/*
 * ngtcp2_pkt_decode_connection_close_frame decodes CONNECTION_CLOSE
 * frame from |payload| of length |payloadlen|.  The result is stored
 * in the object pointed by |dest|.  CONNECTION_CLOSE frame must start
 * at payload[0].  This function finishes it decodes one
 * CONNECTION_CLOSE frame, and returns the exact number of bytes read
 * to decode a frame if it succeeds, or one of the following negative
 * error codes:
 *
 * NGTCP2_ERR_FRAME_ENCODING
 *     Payload is too short to include CONNECTION_CLOSE frame.
 */
ssize_t ngtcp2_pkt_decode_connection_close_frame(ngtcp2_connection_close *dest,
                                                 const uint8_t *payload,
                                                 size_t payloadlen);

/*
 * ngtcp2_pkt_decode_max_data_frame decodes MAX_DATA frame from
 * |payload| of length |payloadlen|.  The result is stored in the
 * object pointed by |dest|.  MAX_DATA frame must start at payload[0].
 * This function finishes when it decodes one MAX_DATA frame, and
 * returns the exact number of bytes read to decode a frame if it
 * succeeds, or one of the following negative error codes:
 *
 * NGTCP2_ERR_FRAME_ENCODING
 *     Payload is too short to include MAX_DATA frame.
 */
ssize_t ngtcp2_pkt_decode_max_data_frame(ngtcp2_max_data *dest,
                                         const uint8_t *payload,
                                         size_t payloadlen);

/*
 * ngtcp2_pkt_decode_max_stream_data_frame decodes MAX_STREAM_DATA
 * frame from |payload| of length |payloadlen|.  The result is stored
 * in the object pointed by |dest|.  MAX_STREAM_DATA frame must start
 * at payload[0].  This function finishes when it decodes one
 * MAX_STREAM_DATA frame, and returns the exact number of bytes read
 * to decode a frame if it succeeds, or one of the following negative
 * error codes:
 *
 * NGTCP2_ERR_FRAME_ENCODING
 *     Payload is too short to include MAX_STREAM_DATA frame.
 */
ssize_t ngtcp2_pkt_decode_max_stream_data_frame(ngtcp2_max_stream_data *dest,
                                                const uint8_t *payload,
                                                size_t payloadlen);

/*
 * ngtcp2_pkt_decode_max_streams_frame decodes MAX_STREAMS frame from
 * |payload| of length |payloadlen|.  The result is stored in the
 * object pointed by |dest|.  MAX_STREAMS frame must start at
 * payload[0].  This function finishes when it decodes one MAX_STREAMS
 * frame, and returns the exact number of bytes read to decode a frame
 * if it succeeds, or one of the following negative error codes:
 *
 * NGTCP2_ERR_FRAME_ENCODING
 *     Payload is too short to include MAX_STREAMS frame.
 */
ssize_t ngtcp2_pkt_decode_max_streams_frame(ngtcp2_max_streams *dest,
                                            const uint8_t *payload,
                                            size_t payloadlen);

/*
 * ngtcp2_pkt_decode_ping_frame decodes PING frame from |payload| of
 * length |payloadlen|.  The result is stored in the object pointed by
 * |dest|.  PING frame must start at payload[0].  This function
 * finishes when it decodes one PING frame, and returns the exact
 * number of bytes read to decode a frame if it succeeds, or one of
 * the following negative error codes:
 *
 * NGTCP2_ERR_FRAME_ENCODING
 *     Payload is too short to include PING frame.
 */
ssize_t ngtcp2_pkt_decode_ping_frame(ngtcp2_ping *dest, const uint8_t *payload,
                                     size_t payloadlen);

/*
 * ngtcp2_pkt_decode_data_blocked_frame decodes DATA_BLOCKED frame
 * from |payload| of length |payloadlen|.  The result is stored in the
 * object pointed by |dest|.  DATA_BLOCKED frame must start at
 * payload[0].  This function finishes when it decodes one
 * DATA_BLOCKED frame, and returns the exact number of bytes read to
 * decode a frame if it succeeds, or one of the following negative
 * error codes:
 *
 * NGTCP2_ERR_FRAME_ENCODING
 *     Payload is too short to include DATA_BLOCKED frame.
 */
ssize_t ngtcp2_pkt_decode_data_blocked_frame(ngtcp2_data_blocked *dest,
                                             const uint8_t *payload,
                                             size_t payloadlen);

/*
 * ngtcp2_pkt_decode_stream_data_blocked_frame decodes
 * STREAM_DATA_BLOCKED frame from |payload| of length |payloadlen|.
 * The result is stored in the object pointed by |dest|.
 * STREAM_DATA_BLOCKED frame must start at payload[0].  This function
 * finishes when it decodes one STREAM_DATA_BLOCKED frame, and returns
 * the exact number of bytes read to decode a frame if it succeeds, or
 * one of the following negative error codes:
 *
 * NGTCP2_ERR_FRAME_ENCODING
 *     Payload is too short to include STREAM_DATA_BLOCKED frame.
 */
ssize_t
ngtcp2_pkt_decode_stream_data_blocked_frame(ngtcp2_stream_data_blocked *dest,
                                            const uint8_t *payload,
                                            size_t payloadlen);

/*
 * ngtcp2_pkt_decode_streams_blocked_frame decodes STREAMS_BLOCKED
 * frame from |payload| of length |payloadlen|.  The result is stored
 * in the object pointed by |dest|.  STREAMS_BLOCKED frame must start
 * at payload[0].  This function finishes when it decodes one
 * STREAMS_BLOCKED frame, and returns the exact number of bytes read
 * to decode a frame if it succeeds, or one of the following negative
 * error codes:
 *
 * NGTCP2_ERR_FRAME_ENCODING
 *     Payload is too short to include STREAMS_BLOCKED frame.
 */
ssize_t ngtcp2_pkt_decode_streams_blocked_frame(ngtcp2_streams_blocked *dest,
                                                const uint8_t *payload,
                                                size_t payloadlen);

/*
 * ngtcp2_pkt_decode_new_connection_id_frame decodes NEW_CONNECTION_ID
 * frame from |payload| of length |payloadlen|.  The result is stored
 * in the object pointed by |dest|.  NEW_CONNECTION_ID frame must
 * start at payload[0].  This function finishes when it decodes one
 * NEW_CONNECTION_ID frame, and returns the exact number of bytes read
 * to decode a frame if it succeeds, or one of the following negative
 * error codes:
 *
 * NGTCP2_ERR_FRAME_ENCODING
 *     Payload is too short to include NEW_CONNECTION_ID frame.
 * NGTCP2_ERR_PROTO
 *     The length of CID is strictly less than 4 or greater than 18.
 */
ssize_t ngtcp2_pkt_decode_new_connection_id_frame(
    ngtcp2_new_connection_id *dest, const uint8_t *payload, size_t payloadlen);

/*
 * ngtcp2_pkt_decode_stop_sending_frame decodes STOP_SENDING frame
 * from |payload| of length |payloadlen|.  The result is stored in the
 * object pointed by |dest|.  STOP_SENDING frame must start at
 * payload[0].  This function finishes when it decodes one
 * STOP_SENDING frame, and returns the exact number of bytes read to
 * decode a frame if it succeeds, or one of the following negative
 * error codes:
 *
 * NGTCP2_ERR_FRAME_ENCODING
 *     Payload is too short to include STOP_SENDING frame.
 */
ssize_t ngtcp2_pkt_decode_stop_sending_frame(ngtcp2_stop_sending *dest,
                                             const uint8_t *payload,
                                             size_t payloadlen);

/*
 * ngtcp2_pkt_decode_path_challenge_frame decodes PATH_CHALLENGE frame
 * from |payload| of length |payloadlen|.  The result is stored in the
 * object pointed by |dest|.  PATH_CHALLENGE frame must start at
 * payload[0].  This function finishes when it decodes one
 * PATH_CHALLENGE frame, and returns the exact number of bytes read to
 * decode a frame if it succeeds, or one of the following negative
 * error codes:
 *
 * NGTCP2_ERR_FRAME_ENCODING
 *     Payload is too short to include PATH_CHALLENGE frame.
 */
ssize_t ngtcp2_pkt_decode_path_challenge_frame(ngtcp2_path_challenge *dest,
                                               const uint8_t *payload,
                                               size_t payloadlen);

/*
 * ngtcp2_pkt_decode_path_response_frame decodes PATH_RESPONSE frame
 * from |payload| of length |payloadlen|.  The result is stored in the
 * object pointed by |dest|.  PATH_RESPONSE frame must start at
 * payload[0].  This function finishes when it decodes one
 * PATH_RESPONSE frame, and returns the exact number of bytes read to
 * decode a frame if it succeeds, or one of the following negative
 * error codes:
 *
 * NGTCP2_ERR_FRAME_ENCODING
 *     Payload is too short to include PATH_RESPONSE frame.
 */
ssize_t ngtcp2_pkt_decode_path_response_frame(ngtcp2_path_response *dest,
                                              const uint8_t *payload,
                                              size_t payloadlen);

/*
 * ngtcp2_pkt_decode_crypto_frame decodes CRYPTO frame from |payload|
 * of length |payloadlen|.  The result is stored in the object pointed
 * by |dest|.  CRYPTO frame must start at payload[0].  This function
 * finishes when it decodes one CRYPTO frame, and returns the exact
 * number of bytes read to decode a frame if it succeeds, or one of
 * the following negative error codes:
 *
 * NGTCP2_ERR_FRAME_ENCODING
 *     Payload is too short to include CRYPTO frame.
 */
ssize_t ngtcp2_pkt_decode_crypto_frame(ngtcp2_crypto *dest,
                                       const uint8_t *payload,
                                       size_t payloadlen);

/*
 * ngtcp2_pkt_decode_new_token_frame decodes NEW_TOKEN frame from
 * |payload| of length |payloadlen|.  The result is stored in the
 * object pointed by |dest|.  NEW_TOKEN frame must start at
 * payload[0].  This function finishes when it decodes one NEW_TOKEN
 * frame, and returns the exact number of bytes read to decode a frame
 * if it succeeds, or one of the following negative error codes:
 *
 * NGTCP2_ERR_FRAME_ENCODING
 *     Payload is too short to include NEW_TOKEN frame.
 */
ssize_t ngtcp2_pkt_decode_new_token_frame(ngtcp2_new_token *dest,
                                          const uint8_t *payload,
                                          size_t payloadlen);

/*
 * ngtcp2_pkt_decode_retire_connection_id_frame decodes RETIRE_CONNECTION_ID
 * frame from |payload| of length |payloadlen|.  The result is stored in the
 * object pointed by |dest|.  RETIRE_CONNECTION_ID frame must start at
 * payload[0].  This function finishes when it decodes one RETIRE_CONNECTION_ID
 * frame, and returns the exact number of bytes read to decode a frame
 * if it succeeds, or one of the following negative error codes:
 *
 * NGTCP2_ERR_FRAME_ENCODING
 *     Payload is too short to include RETIRE_CONNECTION_ID frame.
 */
ssize_t
ngtcp2_pkt_decode_retire_connection_id_frame(ngtcp2_retire_connection_id *dest,
                                             const uint8_t *payload,
                                             size_t payloadlen);

/*
 * ngtcp2_pkt_encode_stream_frame encodes STREAM frame |fr| into the
 * buffer pointed by |out| of length |outlen|.
 *
 * This function assigns <the serialized frame type> &
 * ~NGTCP2_FRAME_STREAM to fr->flags.
 *
 * This function returns the number of bytes written if it succeeds,
 * or one of the following negative error codes:
 *
 * NGTCP2_ERR_NOBUF
 *     Buffer does not have enough capacity to write a frame.
 */
ssize_t ngtcp2_pkt_encode_stream_frame(uint8_t *out, size_t outlen,
                                       ngtcp2_stream *fr);

/*
 * ngtcp2_pkt_encode_ack_frame encodes ACK frame |fr| into the buffer
 * pointed by |out| of length |outlen|.
 *
 * This function assigns <the serialized frame type> &
 * ~NGTCP2_FRAME_ACK to fr->flags.
 *
 * This function returns the number of bytes written if it succeeds,
 * or one of the following negative error codes:
 *
 * NGTCP2_ERR_NOBUF
 *     Buffer does not have enough capacity to write a frame.
 */
ssize_t ngtcp2_pkt_encode_ack_frame(uint8_t *out, size_t outlen,
                                    ngtcp2_ack *fr);

/*
 * ngtcp2_pkt_encode_padding_frame encodes PADDING frame |fr| into the
 * buffer pointed by |out| of length |outlen|.
 *
 * This function encodes consecutive fr->len PADDING frames.
 *
 * This function returns the number of bytes written if it succeeds,
 * or one of the following negative error codes:
 *
 * NGTCP2_ERR_NOBUF
 *     Buffer does not have enough capacity to write frame(s).
 */
ssize_t ngtcp2_pkt_encode_padding_frame(uint8_t *out, size_t outlen,
                                        const ngtcp2_padding *fr);

/*
 * ngtcp2_pkt_encode_reset_stream_frame encodes RESET_STREAM frame
 * |fr| into the buffer pointed by |out| of length |buflen|.
 *
 * This function returns the number of bytes written if it succeeds,
 * or one of the following negative error codes:
 *
 * NGTCP2_ERR_NOBUF
 *     Buffer does not have enough capacity to write a frame.
 */
ssize_t ngtcp2_pkt_encode_reset_stream_frame(uint8_t *out, size_t outlen,
                                             const ngtcp2_reset_stream *fr);

/*
 * ngtcp2_pkt_encode_connection_close_frame encodes CONNECTION_CLOSE
 * frame |fr| into the buffer pointed by |out| of length |outlen|.
 *
 * This function returns the number of bytes written if it succeeds,
 * or one of the following negative error codes:
 *
 * NGTCP2_ERR_NOBUF
 *     Buffer does not have enough capacity to write a frame.
 */
ssize_t
ngtcp2_pkt_encode_connection_close_frame(uint8_t *out, size_t outlen,
                                         const ngtcp2_connection_close *fr);

/*
 * ngtcp2_pkt_encode_max_data_frame encodes MAX_DATA frame |fr| into
 * the buffer pointed by |out| of length |outlen|.
 *
 * This function returns the number of bytes written if it succeeds,
 * or one of the following negative error codes:
 *
 * NGTCP2_ERR_NOBUF
 *     Buffer does not have enough capacity to write a frame.
 */
ssize_t ngtcp2_pkt_encode_max_data_frame(uint8_t *out, size_t outlen,
                                         const ngtcp2_max_data *fr);

/*
 * ngtcp2_pkt_encode_max_stream_data_frame encodes MAX_STREAM_DATA
 * frame |fr| into the buffer pointed by |out| of length |outlen|.
 *
 * This function returns the number of bytes written if it succeeds,
 * or one of the following negative error codes:
 *
 * NGTCP2_ERR_NOBUF
 *     Buffer does not have enough capacity to write a frame.
 */
ssize_t
ngtcp2_pkt_encode_max_stream_data_frame(uint8_t *out, size_t outlen,
                                        const ngtcp2_max_stream_data *fr);

/*
 * ngtcp2_pkt_encode_max_streams_frame encodes MAX_STREAMS
 * frame |fr| into the buffer pointed by |out| of length |outlen|.
 *
 * This function returns the number of bytes written if it succeeds,
 * or one of the following negative error codes:
 *
 * NGTCP2_ERR_NOBUF
 *     Buffer does not have enough capacity to write a frame.
 */
ssize_t ngtcp2_pkt_encode_max_streams_frame(uint8_t *out, size_t outlen,
                                            const ngtcp2_max_streams *fr);

/*
 * ngtcp2_pkt_encode_ping_frame encodes PING frame |fr| into the
 * buffer pointed by |out| of length |outlen|.
 *
 * This function returns the number of bytes written if it succeeds,
 * or one of the following negative error codes:
 *
 * NGTCP2_ERR_NOBUF
 *     Buffer does not have enough capacity to write a frame.
 */
ssize_t ngtcp2_pkt_encode_ping_frame(uint8_t *out, size_t outlen,
                                     const ngtcp2_ping *fr);

/*
 * ngtcp2_pkt_encode_data_blocked_frame encodes DATA_BLOCKED frame
 * |fr| into the buffer pointed by |out| of length |outlen|.
 *
 * This function returns the number of bytes written if it succeeds,
 * or one of the following negative error codes:
 *
 * NGTCP2_ERR_NOBUF
 *     Buffer does not have enough capacity to write a frame.
 */
ssize_t ngtcp2_pkt_encode_data_blocked_frame(uint8_t *out, size_t outlen,
                                             const ngtcp2_data_blocked *fr);

/*
 * ngtcp2_pkt_encode_stream_data_blocked_frame encodes
 * STREAM_DATA_BLOCKED frame |fr| into the buffer pointed by |out| of
 * length |outlen|.
 *
 * This function returns the number of bytes written if it succeeds,
 * or one of the following negative error codes:
 *
 * NGTCP2_ERR_NOBUF
 *     Buffer does not have enough capacity to write a frame.
 */
ssize_t ngtcp2_pkt_encode_stream_data_blocked_frame(
    uint8_t *out, size_t outlen, const ngtcp2_stream_data_blocked *fr);

/*
 * ngtcp2_pkt_encode_streams_blocked_frame encodes STREAMS_BLOCKED
 * frame |fr| into the buffer pointed by |out| of length |outlen|.
 *
 * This function returns the number of bytes written if it succeeds,
 * or one of the following negative error codes:
 *
 * NGTCP2_ERR_NOBUF
 *     Buffer does not have enough capacity to write a frame.
 */
ssize_t
ngtcp2_pkt_encode_streams_blocked_frame(uint8_t *out, size_t outlen,
                                        const ngtcp2_streams_blocked *fr);

/*
 * ngtcp2_pkt_encode_new_connection_id_frame encodes NEW_CONNECTION_ID
 * frame |fr| into the buffer pointed by |out| of length |outlen|.
 *
 * This function returns the number of bytes written if it succeeds,
 * or one of the following negative error codes:
 *
 * NGTCP2_ERR_NOBUF
 *     Buffer does not have enough capacity to write a frame.
 */
ssize_t
ngtcp2_pkt_encode_new_connection_id_frame(uint8_t *out, size_t outlen,
                                          const ngtcp2_new_connection_id *fr);

/*
 * ngtcp2_pkt_encode_stop_sending_frame encodes STOP_SENDING frame
 * |fr| into the buffer pointed by |out| of length |outlen|.
 *
 * This function returns the number of bytes written if it succeeds,
 * or one of the following negative error codes:
 *
 * NGTCP2_ERR_NOBUF
 *     Buffer does not have enough capacity to write a frame.
 */
ssize_t ngtcp2_pkt_encode_stop_sending_frame(uint8_t *out, size_t outlen,
                                             const ngtcp2_stop_sending *fr);

/*
 * ngtcp2_pkt_encode_path_challenge_frame encodes PATH_CHALLENGE frame
 * |fr| into the buffer pointed by |out| of length |outlen|.
 *
 * This function returns the number of bytes written if it succeeds,
 * or one of the following negative error codes:
 *
 * NGTCP2_ERR_NOBUF
 *     Buffer does not have enough capacity to write a frame.
 */
ssize_t ngtcp2_pkt_encode_path_challenge_frame(uint8_t *out, size_t outlen,
                                               const ngtcp2_path_challenge *fr);

/*
 * ngtcp2_pkt_encode_path_response_frame encodes PATH_RESPONSE frame
 * |fr| into the buffer pointed by |out| of length |outlen|.
 *
 * This function returns the number of bytes written if it succeeds,
 * or one of the following negative error codes:
 *
 * NGTCP2_ERR_NOBUF
 *     Buffer does not have enough capacity to write a frame.
 */
ssize_t ngtcp2_pkt_encode_path_response_frame(uint8_t *out, size_t outlen,
                                              const ngtcp2_path_response *fr);

/*
 * ngtcp2_pkt_encode_crypto_frame encodes CRYPTO frame |fr| into the
 * buffer pointed by |out| of length |outlen|.
 *
 * This function returns the number of bytes written if it succeeds,
 * or one of the following negative error codes:
 *
 * NGTCP2_ERR_NOBUF
 *     Buffer does not have enough capacity to write a frame.
 */
ssize_t ngtcp2_pkt_encode_crypto_frame(uint8_t *out, size_t outlen,
                                       const ngtcp2_crypto *fr);

/*
 * ngtcp2_pkt_encode_new_token_frame encodes NEW_TOKEN frame |fr| into
 * the buffer pointed by |out| of length |outlen|.
 *
 * This function returns the number of bytes written if it succeeds,
 * or one of the following negative error codes:
 *
 * NGTCP2_ERR_NOBUF
 *     Buffer does not have enough capacity to write a frame.
 */
ssize_t ngtcp2_pkt_encode_new_token_frame(uint8_t *out, size_t outlen,
                                          const ngtcp2_new_token *fr);

/*
 * ngtcp2_pkt_encode_retire_connection_id_frame encodes RETIRE_CONNECTION_ID
 * frame |fr| into the buffer pointed by |out| of length |outlen|.
 *
 * This function returns the number of bytes written if it succeeds,
 * or one of the following negative error codes:
 *
 * NGTCP2_ERR_NOBUF
 *     Buffer does not have enough capacity to write a frame.
 */
ssize_t ngtcp2_pkt_encode_retire_connection_id_frame(
    uint8_t *out, size_t outlen, const ngtcp2_retire_connection_id *fr);

/*
 * ngtcp2_pkt_adjust_pkt_num find the full 64 bits packet number for
 * |pkt_num|, which is expected to be least significant |n| bits.  The
 * |max_pkt_num| is the highest successfully authenticated packet
 * number.
 */
int64_t ngtcp2_pkt_adjust_pkt_num(int64_t max_pkt_num, int64_t pkt_num,
                                  size_t n);

/*
 * ngtcp2_pkt_validate_ack checks that ack is malformed or not.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_ACK_FRAME
 *     ACK frame is malformed
 */
int ngtcp2_pkt_validate_ack(ngtcp2_ack *fr);

/*
 * ngtcp2_pkt_stream_max_datalen returns the maximum number of bytes
 * which can be sent for stream denoted by |stream_id|.  |offset| is
 * an offset of within the stream.  |len| is the estimated number of
 * bytes to be sent.  |left| is the size of buffer.  If |left| is too
 * small to write STREAM frame, this function returns (size_t)-1.
 */
size_t ngtcp2_pkt_stream_max_datalen(int64_t stream_id, uint64_t offset,
                                     size_t len, size_t left);

/*
 * ngtcp2_pkt_crypto_max_datalen returns the maximum number of bytes
 * which can be sent for crypto stream.  |offset| is an offset of
 * within the crypto stream.  |len| is the estimated number of bytes
 * to be sent.  |left| is the size of buffer.  If |left| is too small
 * to write CRYPTO frame, this function returns (size_t)-1.
 */
size_t ngtcp2_pkt_crypto_max_datalen(uint64_t offset, size_t len, size_t left);

/*
 * ngtcp2_pkt_verify_reserved_bits verifies that the first byte |c| of
 * the packet header has the correct reserved bits.
 *
 * This function returns 0 if it succeeds, or the following negative
 * error codes:
 *
 * NGTCP2_ERR_PROTO
 *     Reserved bits has wrong value.
 */
int ngtcp2_pkt_verify_reserved_bits(uint8_t c);

#endif /* NGTCP2_PKT_H */
