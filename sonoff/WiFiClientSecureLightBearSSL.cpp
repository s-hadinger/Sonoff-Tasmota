/*
  WiFiClientBearSSL- SSL client/server for esp8266 using BearSSL libraries
  - Mostly compatible with Arduino WiFi shield library and standard
    WiFiClient/ServerSecure (except for certificate handling).

  Copyright (c) 2018 Earle F. Philhower, III

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#define LWIP_INTERNAL

#include <list>
#include <errno.h>
#include <algorithm>

extern "C" {
#include "osapi.h"
#include "ets_sys.h"
}
#include "debug.h"
#include "ESP8266WiFi.h"
#include "WiFiClient.h"
#include "WiFiClientSecureLightBearSSL.h"
#include "StackThunk.h"
#include "lwip/opt.h"
#include "lwip/ip.h"
#include "lwip/tcp.h"
#include "lwip/inet.h"
#include "lwip/netif.h"
#include <include/ClientContext.h>
#include "c_types.h"
#include "coredecls.h"

#define DEBUG_TLS

#ifdef DEBUG_TLS
void Log_heap_size(const char *msg) {
  Serial.printf("%s %d Thunk %d\n", msg, ESP.getFreeHeap(), stack_thunk_get_max_usage());
}

void Log_fingerprint(const unsigned char *finger) {
  char fingerprint[60];
  fingerprint[0] = '\0';
  for (uint8_t i = 0; i < 20; i++) {
    snprintf_P(fingerprint, sizeof(fingerprint), "%s%s%02X", fingerprint, (i) ? ":" : "", finger[i]);
  }
  Serial.printf("Fingerprint = %s\n", fingerprint);
}
#endif

void Log_buffer(const char *msg, const uint8_t *buf, uint32_t size) {
  char out[200];
  out[0] = '\0';
  if (size > 64) size = 64;
  for (uint8_t i = 0; i < size; i++) {
    snprintf_P(out, sizeof(out), "%s%s%02X", out, (i) ? ":" : "", buf[i]);
  }
  Serial.printf("%s = %s\n", msg, out);
}


#if !CORE_MOCK

// The BearSSL thunks in use for now
#define br_ssl_engine_recvapp_ack thunk_br_ssl_engine_recvapp_ack
#define br_ssl_engine_recvapp_buf thunk_br_ssl_engine_recvapp_buf
#define br_ssl_engine_recvrec_ack thunk_br_ssl_engine_recvrec_ack
#define br_ssl_engine_recvrec_buf thunk_br_ssl_engine_recvrec_buf
#define br_ssl_engine_sendapp_ack thunk_br_ssl_engine_sendapp_ack
#define br_ssl_engine_sendapp_buf thunk_br_ssl_engine_sendapp_buf
#define br_ssl_engine_sendrec_ack thunk_br_ssl_engine_sendrec_ack
#define br_ssl_engine_sendrec_buf thunk_br_ssl_engine_sendrec_buf

#endif

#ifdef DEBUG_ESP_SSL
//#define DEBUG_BSSL(fmt, ...)  DEBUG_ESP_PORT.printf_P((PGM_P)PSTR( "BSSL:" fmt), ## __VA_ARGS__)
#define DEBUG_BSSL(fmt, ...)  Serial.printf("BSSL:" fmt, ## __VA_ARGS__)
#else
#define DEBUG_BSSL(...)
#endif

namespace BearSSL {

void WiFiClientSecure_light::_clear() {
  // TLS handshake may take more than the 5 second default timeout
  _timeout = 10000;   // 10 seconds max, it should never go over 6 seconds

  _sc = nullptr;
  _ctx_present = false;
  _eng = nullptr;
  _x509_insecure = nullptr;
  _iobuf_in = nullptr;
  _iobuf_out = nullptr;
  _now = 0; // You can override or ensure time() is correct w/configTime
  setBufferSizes(1024, 1024); // reasonable minimum
  _handshake_done = false;
  _recvapp_buf = nullptr;
  _recvapp_len = 0;
  _oom_err = false;
  // _cipher_list = nullptr;
  // _cipher_cnt = 0;
}

void WiFiClientSecure_light::_clearAuthenticationSettings() {
  _use_fingerprint = false;
  _sk = nullptr;
}

// Constructor
WiFiClientSecure_light::WiFiClientSecure_light(int recv, int xmit) : WiFiClient() {
  _clear();
  _clearAuthenticationSettings();
  stack_thunk_add_ref();
  // now finish the setup
  setBufferSizes(recv, xmit); // reasonable minimum
  allocateBuffers();
}


WiFiClientSecure_light::~WiFiClientSecure_light() {
  if (_client) {
    _client->unref();
    _client = nullptr;
  }
  //_cipher_list = nullptr; // std::shared will free if last reference
  _freeSSL();
  stack_thunk_del_ref();
}

void WiFiClientSecure_light::allocateBuffers(void) {
  // We prefer to allocate all buffers at start, rather than lazy allocation and deallocation
  // in the long run it avoids heap fragmentation and improves stability
Log_heap_size("allocateBuffers before");
  _sc = std::make_shared<br_ssl_client_context>();
  _iobuf_in = std::shared_ptr<unsigned char>(new unsigned char[_iobuf_in_size], std::default_delete<unsigned char[]>());
  _iobuf_out = std::shared_ptr<unsigned char>(new unsigned char[_iobuf_out_size], std::default_delete<unsigned char[]>());
  _x509_insecure = std::make_shared<struct br_x509_pubkeyfingerprint_context>();
Log_heap_size("allocateBuffers after");
}

void WiFiClientSecure_light::setClientRSACert(const X509List *chain, const PrivateKey *sk) {
  _chain = chain;
  _sk = sk;
}

void WiFiClientSecure_light::setBufferSizes(int recv, int xmit) {
Log_heap_size("setBufferSizes");
  // Following constants taken from bearssl/src/ssl/ssl_engine.c (not exported unfortunately)
  const int MAX_OUT_OVERHEAD = 85;
  const int MAX_IN_OVERHEAD = 325;

  // The data buffers must be between 512B and 16KB
  recv = std::max(512, std::min(16384, recv));
  xmit = std::max(512, std::min(16384, xmit));

  // Add in overhead for SSL protocol
  recv += MAX_IN_OVERHEAD;
  xmit += MAX_OUT_OVERHEAD;
  _iobuf_in_size = recv;
  _iobuf_out_size = xmit;
}

bool WiFiClientSecure_light::stop(unsigned int maxWaitMs) {
  bool ret = WiFiClient::stop(maxWaitMs); // calls our virtual flush()
  _freeSSL();
  return ret;
}

bool WiFiClientSecure_light::flush(unsigned int maxWaitMs) {
  (void) _run_until(BR_SSL_SENDAPP);
  return WiFiClient::flush(maxWaitMs);
}

int WiFiClientSecure_light::connect(IPAddress ip, uint16_t port) {
  if (!WiFiClient::connect(ip, port)) {
    return 0;
  }
  //return _connectSSL(nullptr);
  return _connectSSL("a3pa4ktnq87yfu-ats.iot.eu-central-1.amazonaws.com");  // TODO
}

int WiFiClientSecure_light::connect(const char* name, uint16_t port) {
  IPAddress remote_addr;
  if (!WiFi.hostByName(name, remote_addr)) {
    DEBUG_BSSL("connect: Name loopup failure\n");
    return 0;
  }
  if (!WiFiClient::connect(remote_addr, port)) {
    DEBUG_BSSL("connect: Unable to connect TCP socket\n");
    return 0;
  }
  return _connectSSL(name);
}

int WiFiClientSecure_light::connect(const String& host, uint16_t port) {
  return connect(host.c_str(), port);
}

void WiFiClientSecure_light::_freeSSL() {
  // These are smart pointers and will free if refcnt==0
  //_sc = nullptr; // TODO clean *_sc ?
  _ctx_present = false;
  memset(_sc.get(), 0, sizeof(br_ssl_client_context));
  //_sc_svr = nullptr;
  // _x509_insecure = nullptr; // TODO clean *_sc ?
  // Reset non-allocated ptrs (pointing to bits potentially free'd above)
  _recvapp_buf = nullptr;
  _recvapp_len = 0;
  // This connection is toast
  _handshake_done = false;
}

bool WiFiClientSecure_light::_clientConnected() {
  return (_client && _client->state() == ESTABLISHED);
}

uint8_t WiFiClientSecure_light::connected() {
  if (available() || (_clientConnected() && _handshake_done)) {
    return true;
  }
  return false;
}

size_t WiFiClientSecure_light::_write(const uint8_t *buf, size_t size, bool pmem) {
  size_t sent_bytes = 0;

  if (!connected() || !size || !_handshake_done) {
    return 0;
  }

  do {
    // Ensure we yield if we need multiple fragments to avoid WDT
    if (sent_bytes) {
      optimistic_yield(1000);
    }

//Log_buffer("_Write", buf, size);
    // Get BearSSL to a state where we can send
    if (_run_until(BR_SSL_SENDAPP) < 0) {
      break;
    }
//Serial.printf("_write gate passed\n");
    if (br_ssl_engine_current_state(_eng) & BR_SSL_SENDAPP) {
      size_t sendapp_len;
      unsigned char *sendapp_buf = br_ssl_engine_sendapp_buf(_eng, &sendapp_len);
      int to_send = size > sendapp_len ? sendapp_len : size;
      if (pmem) {
        memcpy_P(sendapp_buf, buf, to_send);
      } else {
        memcpy(sendapp_buf, buf, to_send);
      }
      br_ssl_engine_sendapp_ack(_eng, to_send);
      br_ssl_engine_flush(_eng, 0);
      flush();
      buf += to_send;
      sent_bytes += to_send;
      size -= to_send;
    } else {
      break;
    }
  } while (size);

  return sent_bytes;
}

size_t WiFiClientSecure_light::write(const uint8_t *buf, size_t size) {
  return _write(buf, size, false);
}

size_t WiFiClientSecure_light::write_P(PGM_P buf, size_t size) {
  return _write((const uint8_t *)buf, size, true);
}

// We have to manually read and send individual chunks.
size_t WiFiClientSecure_light::write(Stream& stream) {
  size_t totalSent = 0;
  size_t countRead;
  size_t countSent;

  if (!connected() || !_handshake_done) {
    DEBUG_BSSL("write: Connect/handshake not completed yet\n");
    return 0;
  }

  do {
    uint8_t temp[256]; // Temporary chunk size same as ClientContext
    countSent = 0;
    countRead = stream.readBytes(temp, sizeof(temp));
    if (countRead) {
      countSent = _write((const uint8_t*)temp, countRead, true);
      totalSent += countSent;
    }
    yield(); // Feed the WDT
  } while ((countSent == countRead) && (countSent > 0));
  return totalSent;
}

int WiFiClientSecure_light::read(uint8_t *buf, size_t size) {
  if (!ctx_present() || !_handshake_done) {
    return -1;
  }

  int avail = available();
  bool conn = connected();
  if (!avail && conn) {
    return 0;  // We're still connected, but nothing to read
  }
  if (!avail && !conn) {
    DEBUG_BSSL("read: Not connected, none left available\n");
    return -1;
  }

  if (avail) {
    // Take data from the recvapp buffer
    int to_copy = _recvapp_len < size ? _recvapp_len : size;
    memcpy(buf, _recvapp_buf, to_copy);
//Log_buffer("_Read", buf, to_copy);
    br_ssl_engine_recvapp_ack(_eng, to_copy);
    _recvapp_buf = nullptr;
    _recvapp_len = 0;
    return to_copy;
  }

  if (!conn) {
    DEBUG_BSSL("read: Not connected\n");
    return -1;
  }
  return 0; // If we're connected, no error but no read.
}

int WiFiClientSecure_light::read() {
  uint8_t c;
  if (1 == read(&c, 1)) {
    return c;
  }
  DEBUG_BSSL("read: failed\n");
  return -1;
}

int WiFiClientSecure_light::available() {
  if (_recvapp_buf) {
//Serial.printf("WiFiClientSecure_light::available() _recvapp_buf > 0, len = %d", _recvapp_len);
    return _recvapp_len;  // Anything from last call?
  }
  _recvapp_buf = nullptr;
  _recvapp_len = 0;
  if (!ctx_present() || _run_until(BR_SSL_RECVAPP, false) < 0) {
    return 0;
  }
  int st = br_ssl_engine_current_state(_eng);
  if (st == BR_SSL_CLOSED) {
    return 0;  // Nothing leftover, SSL is closed
  }
  if (st & BR_SSL_RECVAPP) {
    _recvapp_buf = br_ssl_engine_recvapp_buf(_eng, &_recvapp_len);
    return _recvapp_len;
  }

  return 0;
}

int WiFiClientSecure_light::peek() {
  if (!ctx_present() || !available()) {
    DEBUG_BSSL("peek: Not connected, none left available\n");
    return -1;
  }
  if (_recvapp_buf && _recvapp_len) {
    return _recvapp_buf[0];
  }
  DEBUG_BSSL("peek: No data left\n");
  return -1;
}

size_t WiFiClientSecure_light::peekBytes(uint8_t *buffer, size_t length) {
  size_t to_copy = 0;
  if (!ctx_present()) {
    DEBUG_BSSL("peekBytes: Not connected\n");
    return 0;
  }

  _startMillis = millis();
  while ((available() < (int) length) && ((millis() - _startMillis) < 5000)) {
    yield();
  }

  to_copy = _recvapp_len < length ? _recvapp_len : length;
  memcpy(buffer, _recvapp_buf, to_copy);
Log_buffer("_PeekBytes", buffer, to_copy);
  return to_copy;
}

/* --- Copied almost verbatim from BEARSSL SSL_IO.C ---
   Run the engine, until the specified target state is achieved, or
   an error occurs. The target state is SENDAPP, RECVAPP, or the
   combination of both (the combination matches either). When a match is
   achieved, this function returns 0. On error, it returns -1.
*/
int WiFiClientSecure_light::_run_until(unsigned target, bool blocking) {
  if (!ctx_present()) {
    DEBUG_BSSL("_run_until: Not connected\n");
    return -1;
  }
  for (int no_work = 0; blocking || no_work < 2;) {
    if (blocking) {
      // Only for blocking operations can we afford to yield()
      optimistic_yield(100);
    }

    int state;
    state = br_ssl_engine_current_state(_eng);
    if (state & BR_SSL_CLOSED) {
      return -1;
    }

    if (!(_client->state() == ESTABLISHED) && !WiFiClient::available()) {
      return (state & target) ? 0 : -1;
    }

    /*
       If there is some record data to send, do it. This takes
       precedence over everything else.
    */
    if (state & BR_SSL_SENDREC) {
      unsigned char *buf;
      size_t len;
      int wlen;

      buf = br_ssl_engine_sendrec_buf(_eng, &len);
      wlen = WiFiClient::write(buf, len);
      if (wlen <= 0) {
        /*
           If we received a close_notify and we
           still send something, then we have our
           own response close_notify to send, and
           the peer is allowed by RFC 5246 not to
           wait for it.
        */
        return -1;
      }
      if (wlen > 0) {
        br_ssl_engine_sendrec_ack(_eng, wlen);
      }
      no_work = 0;
      continue;
    }

    /*
       If we reached our target, then we are finished.
    */
    if (state & target) {
      return 0;
    }

    /*
       If some application data must be read, and we did not
       exit, then this means that we are trying to write data,
       and that's not possible until the application data is
       read. This may happen if using a shared in/out buffer,
       and the underlying protocol is not strictly half-duplex.
       This is unrecoverable here, so we report an error.
    */
    if (state & BR_SSL_RECVAPP) {
      DEBUG_BSSL("_run_until: Fatal protocol state\n");
      return -1;
    }

    /*
       If we reached that point, then either we are trying
       to read data and there is some, or the engine is stuck
       until a new record is obtained.
    */
    if (state & BR_SSL_RECVREC) {
      if (WiFiClient::available()) {
        unsigned char *buf;
        size_t len;
        int rlen;

        buf = br_ssl_engine_recvrec_buf(_eng, &len);
        rlen = WiFiClient::read(buf, len);
        if (rlen < 0) {
          return -1;
        }
        if (rlen > 0) {
          br_ssl_engine_recvrec_ack(_eng, rlen);
        }
        no_work = 0;
        continue;
      }
    }

    /*
       We can reach that point if the target RECVAPP, and
       the state contains SENDAPP only. This may happen with
       a shared in/out buffer. In that case, we must flush
       the buffered data to "make room" for a new incoming
       record.
    */
    br_ssl_engine_flush(_eng, 0);

    no_work++; // We didn't actually advance here
  }
  // We only get here if we ran through the loop without getting anything done
  return -1;
}

bool WiFiClientSecure_light::_wait_for_handshake() {
  _handshake_done = false;
  while (!_handshake_done && _clientConnected()) {
    int ret = _run_until(BR_SSL_SENDAPP);
    if (ret < 0) {
      DEBUG_BSSL("_wait_for_handshake: failed\n");
      break;
    }
    if (br_ssl_engine_current_state(_eng) & BR_SSL_SENDAPP) {
      _handshake_done = true;
    }
    optimistic_yield(1000);
  }
  return _handshake_done;
}

static uint8_t htoi (unsigned char c)
{
  if (c>='0' && c <='9') return c - '0';
  else if (c>='A' && c<='F') return 10 + c - 'A';
  else if (c>='a' && c<='f') return 10 + c - 'a';
  else return 255;
}

// Set a fingerprint by parsing an ASCII string
bool WiFiClientSecure_light::setPubKeyFingerprint(const char *fpStr) {
  int idx = 0;
  uint8_t c, d;
  uint8_t fp[20];

  while (idx < 20) {
    c = pgm_read_byte(fpStr++);
    if (!c) break; // String ended, done processing
    d = pgm_read_byte(fpStr++);
    if (!d) {
      DEBUG_BSSL("setFingerprint: FP too short\n");
      return false; // Only half of the last hex digit, error
    }
    c = htoi(c);
    d = htoi(d);
    if ((c>15) || (d>15)) {
      DEBUG_BSSL("setFingerprint: Invalid char\n");
      return false; // Error in one of the hex characters
    }
    fp[idx++] = (c<<4)|d;

    // Skip 0 or more spaces or colons
    while ( pgm_read_byte(fpStr) && (pgm_read_byte(fpStr)==' ' || pgm_read_byte(fpStr)==':') ) {
      fpStr++;
    }
  }
  if ((idx != 20) || pgm_read_byte(fpStr)) {
    DEBUG_BSSL("setFingerprint: Garbage at end of fp\n");
    return false; // Garbage at EOL or we didn't have enough hex digits
  }
  return setPubKeyFingerprint(fp);
}

extern "C" {

  // BearSSL doesn't define a true insecure decoder, so we make one ourselves
  // from the simple parser.  It generates the issuer and subject hashes and
  // the SHA1 fingerprint, only one (or none!) of which will be used to
  // "verify" the certificate.

  // Private x509 decoder state
  struct br_x509_pubkeyfingerprint_context {
    const br_x509_class *vtable;
    bool done_cert;             // did we parse the first cert already?
    uint8_t *pubkey_fingerprint;
    unsigned usages;            // pubkey usage
    br_x509_decoder_context ctx;  // defined in BearSSL
  };

  // Callback on the first byte of any certificate
  static void pubkeyfingerprint_start_chain(const br_x509_class **ctx, const char *server_name) {
    br_x509_pubkeyfingerprint_context *xc = (br_x509_pubkeyfingerprint_context *)ctx;
    br_x509_decoder_init(&xc->ctx, nullptr, nullptr, nullptr, nullptr);
    (void)server_name;    // ignore server name
  }

  // Callback for each certificate present in the chain (but only operates
  // on the first one by design).
  static void pubkeyfingerprint_start_cert(const br_x509_class **ctx, uint32_t length) {
    (void) ctx;   // do nothing
    (void) length;
  }

  // Callback for each byte stream in the chain.  Only process first cert.
  static void pubkeyfingerprint_append(const br_x509_class **ctx, const unsigned char *buf, size_t len) {
    br_x509_pubkeyfingerprint_context *xc = (br_x509_pubkeyfingerprint_context *)ctx;
    // Don't process anything but the first certificate in the chain
    if (!xc->done_cert) {
      br_x509_decoder_push(&xc->ctx, (const void*)buf, len);
    }
  }

  // Callback on individual cert end.
  static void pubkeyfingerprint_end_cert(const br_x509_class **ctx) {
    br_x509_pubkeyfingerprint_context *xc = (br_x509_pubkeyfingerprint_context *)ctx;
    xc->done_cert = true;   // first cert already processed
  }

  static void pubkeyfingerprint_pubkey_fingerprint(br_sha1_context *shactx, br_rsa_public_key rsakey) {
    br_sha1_init(shactx);
    br_sha1_update(shactx, "ssh-rsa", 7);           // tag
    br_sha1_update(shactx, rsakey.e, rsakey.elen);  // exponent
    br_sha1_update(shactx, rsakey.n, rsakey.nlen);  // modulus
  }

  // Callback when complete chain has been parsed.
  // Return 0 on validation success, !0 on validation error
  static unsigned pubkeyfingerprint_end_chain(const br_x509_class **ctx) {
    br_x509_pubkeyfingerprint_context *xc = (br_x509_pubkeyfingerprint_context *)ctx;

    br_sha1_context sha1_context;
    pubkeyfingerprint_pubkey_fingerprint(&sha1_context, xc->ctx.pkey.key.rsa);
    br_sha1_out(&sha1_context, xc->pubkey_fingerprint); // copy to fingerprint

//Let's dump the public key and hash
{
  char out[64] = "";
  for (uint8_t i = 0; i<20; i++) {
   snprintf_P(out, sizeof(out), "%s%s%02X", out, (i) ? " " : "", xc->pubkey_fingerprint[i]);
  }
  Serial.printf("Fingerprint = %s\n", out);
}

    // Default (no validation at all) or no errors in prior checks = success.
    return 0;
  }

  // Return the public key from the validator (set by x509_minimal)
  static const br_x509_pkey *pubkeyfingerprint_get_pkey(const br_x509_class *const *ctx, unsigned *usages) {
     const br_x509_pubkeyfingerprint_context *xc = (const br_x509_pubkeyfingerprint_context *)ctx;

    if (usages != NULL) {
      *usages = BR_KEYTYPE_KEYX | BR_KEYTYPE_SIGN; // I said we were insecure!
    }
    return &xc->ctx.pkey;
  }

  //  Set up the x509 insecure data structures for BearSSL core to use.
  void br_x509_pubkeyfingerprint_init(br_x509_pubkeyfingerprint_context *ctx, uint8_t _fingerprint[20]) {
    static const br_x509_class br_x509_pubkeyfingerprint_vtable PROGMEM = {
      sizeof(br_x509_pubkeyfingerprint_context),
      pubkeyfingerprint_start_chain,
      pubkeyfingerprint_start_cert,
      pubkeyfingerprint_append,
      pubkeyfingerprint_end_cert,
      pubkeyfingerprint_end_chain,
      pubkeyfingerprint_get_pkey
    };

    memset(ctx, 0, sizeof * ctx);
    ctx->vtable = &br_x509_pubkeyfingerprint_vtable;
    ctx->done_cert = false;
    ctx->pubkey_fingerprint = _fingerprint;
  }

  // Some constants uses to init the server/client contexts
  // Note that suites_P needs to be copied to RAM before use w/BearSSL!
  // List copied verbatim from BearSSL/ssl_client_full.c
  /*
   * The "full" profile supports all implemented cipher suites.
   *
   * Rationale for suite order, from most important to least
   * important rule:
   *
   * -- Don't use 3DES if AES or ChaCha20 is available.
   * -- Try to have Forward Secrecy (ECDHE suite) if possible.
   * -- When not using Forward Secrecy, ECDH key exchange is
   *    better than RSA key exchange (slightly more expensive on the
   *    client, but much cheaper on the server, and it implies smaller
   *    messages).
   * -- ChaCha20+Poly1305 is better than AES/GCM (faster, smaller code).
   * -- GCM is better than CCM and CBC. CCM is better than CBC.
   * -- CCM is preferable over CCM_8 (with CCM_8, forgeries may succeed
   *    with probability 2^(-64)).
   * -- AES-128 is preferred over AES-256 (AES-128 is already
   *    strong enough, and AES-256 is 40% more expensive).
   */
  static const uint16_t suites_P[] PROGMEM = {
    BR_TLS_RSA_WITH_AES_128_CBC_SHA256,
    BR_TLS_RSA_WITH_AES_128_CBC_SHA
  };

  // Default initializion for our SSL clients
  static void br_ssl_client_base_init(br_ssl_client_context *cc, const uint16_t *cipher_list, int cipher_cnt) {
    uint16_t suites[cipher_cnt];
    memcpy_P(suites, cipher_list, cipher_cnt * sizeof(cipher_list[0]));
    br_ssl_client_zero(cc);
    br_ssl_engine_set_versions(&cc->eng, BR_TLS12, BR_TLS12);
    br_ssl_engine_set_suites(&cc->eng, suites, (sizeof suites) / (sizeof suites[0]));
    br_ssl_client_set_default_rsapub(cc);
    br_ssl_engine_set_default_rsavrfy(&cc->eng);

    // install hashes
    br_ssl_engine_set_hash(&cc->eng, br_sha1_ID, &br_sha1_vtable);
    br_ssl_engine_set_hash(&cc->eng, br_sha256_ID, &br_sha256_vtable);
    br_ssl_engine_set_prf_sha256(&cc->eng, &br_tls12_sha256_prf);
    br_ssl_engine_set_default_aes_cbc(&cc->eng);
  }

}

// Called by connect() to do the actual SSL setup and handshake.
// Returns if the SSL handshake succeeded.
bool WiFiClientSecure_light::_connectSSL(const char* hostName) {
  DEBUG_BSSL("_connectSSL: start connection\n");
  _freeSSL();
  _oom_err = false;

  _ctx_present = true;
  _eng = &_sc->eng; // Allocation/deallocation taken care of by the _sc shared_ptr

  br_ssl_client_base_init(_sc.get(), suites_P, sizeof(suites_P) / sizeof(suites_P[0]));

  // Only failure possible in the installation is OOM
  br_x509_pubkeyfingerprint_init(_x509_insecure.get(), _fingerprint);
  br_ssl_engine_set_x509(_eng, &_x509_insecure->vtable);

  br_ssl_engine_set_buffers_bidi(_eng, _iobuf_in.get(), _iobuf_in_size, _iobuf_out.get(), _iobuf_out_size);

  br_ssl_client_set_single_rsa(_sc.get(), _chain ? _chain->getX509Certs() : nullptr, _chain ? _chain->getCount() : 0,
                               _sk->getRSA(), br_rsa_pkcs1_sign_get_default());

  if (!br_ssl_client_reset(_sc.get(), hostName, 0)) {
    _freeSSL();
    DEBUG_BSSL("_connectSSL: Can't reset client\n");
    return false;
  }

  auto ret = _wait_for_handshake();
#ifdef DEBUG_ESP_SSL
  if (!ret) {
    char err[256];
    getLastSSLError(err, sizeof(err));
    DEBUG_BSSL("Couldn't connect. Error = '%s'\n", err);
  } else {
    DEBUG_BSSL("Connected!\n");
  }
#endif
  return ret;
}

// Returns an error ID and possibly a string (if dest != null) of the last
// BearSSL reported error.
int WiFiClientSecure_light::getLastSSLError(char *dest, size_t len) {
  int err = 0;
  const char *t = PSTR("OK");
  err = br_ssl_engine_last_error(_eng);
  if (_oom_err) {
    err = -1000;
  }
  switch (err) {
    case -1000: t = PSTR("Unable to allocate memory for SSL structures and buffers."); break;
    case BR_ERR_BAD_PARAM: t = PSTR("Caller-provided parameter is incorrect."); break;
    case BR_ERR_BAD_STATE: t = PSTR("Operation requested by the caller cannot be applied with the current context state (e.g. reading data while outgoing data is waiting to be sent)."); break;
    case BR_ERR_UNSUPPORTED_VERSION: t = PSTR("Incoming protocol or record version is unsupported."); break;
    case BR_ERR_BAD_VERSION: t = PSTR("Incoming record version does not match the expected version."); break;
    case BR_ERR_BAD_LENGTH: t = PSTR("Incoming record length is invalid."); break;
    case BR_ERR_TOO_LARGE: t = PSTR("Incoming record is too large to be processed, or buffer is too small for the handshake message to send."); break;
    case BR_ERR_BAD_MAC: t = PSTR("Decryption found an invalid padding, or the record MAC is not correct."); break;
    case BR_ERR_NO_RANDOM: t = PSTR("No initial entropy was provided, and none can be obtained from the OS."); break;
    case BR_ERR_UNKNOWN_TYPE: t = PSTR("Incoming record type is unknown."); break;
    case BR_ERR_UNEXPECTED: t = PSTR("Incoming record or message has wrong type with regards to the current engine state."); break;
    case BR_ERR_BAD_CCS: t = PSTR("ChangeCipherSpec message from the peer has invalid contents."); break;
    case BR_ERR_BAD_ALERT: t = PSTR("Alert message from the peer has invalid contents (odd length)."); break;
    case BR_ERR_BAD_HANDSHAKE: t = PSTR("Incoming handshake message decoding failed."); break;
    case BR_ERR_OVERSIZED_ID: t = PSTR("ServerHello contains a session ID which is larger than 32 bytes."); break;
    case BR_ERR_BAD_CIPHER_SUITE: t = PSTR("Server wants to use a cipher suite that we did not claim to support. This is also reported if we tried to advertise a cipher suite that we do not support."); break;
    case BR_ERR_BAD_COMPRESSION: t = PSTR("Server wants to use a compression that we did not claim to support."); break;
    case BR_ERR_BAD_FRAGLEN: t = PSTR("Server's max fragment length does not match client's."); break;
    case BR_ERR_BAD_SECRENEG: t = PSTR("Secure renegotiation failed."); break;
    case BR_ERR_EXTRA_EXTENSION: t = PSTR("Server sent an extension type that we did not announce, or used the same extension type several times in a single ServerHello."); break;
    case BR_ERR_BAD_SNI: t = PSTR("Invalid Server Name Indication contents (when used by the server, this extension shall be empty)."); break;
    case BR_ERR_BAD_HELLO_DONE: t = PSTR("Invalid ServerHelloDone from the server (length is not 0)."); break;
    case BR_ERR_LIMIT_EXCEEDED: t = PSTR("Internal limit exceeded (e.g. server's public key is too large)."); break;
    case BR_ERR_BAD_FINISHED: t = PSTR("Finished message from peer does not match the expected value."); break;
    case BR_ERR_RESUME_MISMATCH: t = PSTR("Session resumption attempt with distinct version or cipher suite."); break;
    case BR_ERR_INVALID_ALGORITHM: t = PSTR("Unsupported or invalid algorithm (ECDHE curve, signature algorithm, hash function)."); break;
    case BR_ERR_BAD_SIGNATURE: t = PSTR("Invalid signature in ServerKeyExchange or CertificateVerify message."); break;
    case BR_ERR_WRONG_KEY_USAGE: t = PSTR("Peer's public key does not have the proper type or is not allowed for the requested operation."); break;
    case BR_ERR_NO_CLIENT_AUTH: t = PSTR("Client did not send a certificate upon request, or the client certificate could not be validated."); break;
    case BR_ERR_IO: t = PSTR("I/O error or premature close on transport stream."); break;
    case BR_ERR_X509_INVALID_VALUE: t = PSTR("Invalid value in an ASN.1 structure."); break;
    case BR_ERR_X509_TRUNCATED: t = PSTR("Truncated certificate or other ASN.1 object."); break;
    case BR_ERR_X509_EMPTY_CHAIN: t = PSTR("Empty certificate chain (no certificate at all)."); break;
    case BR_ERR_X509_INNER_TRUNC: t = PSTR("Decoding error: inner element extends beyond outer element size."); break;
    case BR_ERR_X509_BAD_TAG_CLASS: t = PSTR("Decoding error: unsupported tag class (application or private)."); break;
    case BR_ERR_X509_BAD_TAG_VALUE: t = PSTR("Decoding error: unsupported tag value."); break;
    case BR_ERR_X509_INDEFINITE_LENGTH: t = PSTR("Decoding error: indefinite length."); break;
    case BR_ERR_X509_EXTRA_ELEMENT: t = PSTR("Decoding error: extraneous element."); break;
    case BR_ERR_X509_UNEXPECTED: t = PSTR("Decoding error: unexpected element."); break;
    case BR_ERR_X509_NOT_CONSTRUCTED: t = PSTR("Decoding error: expected constructed element, but is primitive."); break;
    case BR_ERR_X509_NOT_PRIMITIVE: t = PSTR("Decoding error: expected primitive element, but is constructed."); break;
    case BR_ERR_X509_PARTIAL_BYTE: t = PSTR("Decoding error: BIT STRING length is not multiple of 8."); break;
    case BR_ERR_X509_BAD_BOOLEAN: t = PSTR("Decoding error: BOOLEAN value has invalid length."); break;
    case BR_ERR_X509_OVERFLOW: t = PSTR("Decoding error: value is off-limits."); break;
    case BR_ERR_X509_BAD_DN: t = PSTR("Invalid distinguished name."); break;
    case BR_ERR_X509_BAD_TIME: t = PSTR("Invalid date/time representation."); break;
    case BR_ERR_X509_UNSUPPORTED: t = PSTR("Certificate contains unsupported features that cannot be ignored."); break;
    case BR_ERR_X509_LIMIT_EXCEEDED: t = PSTR("Key or signature size exceeds internal limits."); break;
    case BR_ERR_X509_WRONG_KEY_TYPE: t = PSTR("Key type does not match that which was expected."); break;
    case BR_ERR_X509_BAD_SIGNATURE: t = PSTR("Signature is invalid."); break;
    case BR_ERR_X509_TIME_UNKNOWN: t = PSTR("Validation time is unknown."); break;
    case BR_ERR_X509_EXPIRED: t = PSTR("Certificate is expired or not yet valid."); break;
    case BR_ERR_X509_DN_MISMATCH: t = PSTR("Issuer/Subject DN mismatch in the chain."); break;
    case BR_ERR_X509_BAD_SERVER_NAME: t = PSTR("Expected server name was not found in the chain."); break;
    case BR_ERR_X509_CRITICAL_EXTENSION: t = PSTR("Unknown critical extension in certificate."); break;
    case BR_ERR_X509_NOT_CA: t = PSTR("Not a CA, or path length constraint violation."); break;
    case BR_ERR_X509_FORBIDDEN_KEY_USAGE: t = PSTR("Key Usage extension prohibits intended usage."); break;
    case BR_ERR_X509_WEAK_PUBLIC_KEY: t = PSTR("Public key found in certificate is too small."); break;
    case BR_ERR_X509_NOT_TRUSTED: t = PSTR("Chain could not be linked to a trust anchor."); break;
    default: t = PSTR("Unknown error code."); break;
  }
  if (dest) {
    strncpy_P(dest, t, len);
    dest[len - 1] = 0;
  }
  return err;
}

};
