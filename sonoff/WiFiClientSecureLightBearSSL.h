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


#ifndef wificlientlightbearssl_h
#define wificlientlightbearssl_h
#include <vector>
#include "WiFiClient.h"
#include <bearssl/bearssl.h>
#include "BearSSLHelpers.h"
#include "CertStoreBearSSL.h"

namespace BearSSL {

class WiFiClientSecure_light : public WiFiClient {
  public:
    WiFiClientSecure_light(int recv, int xmit);
    ~WiFiClientSecure_light() override;

    void allocateBuffers(void);

    int connect(IPAddress ip, uint16_t port) override;
    int connect(const String& host, uint16_t port) override;
    int connect(const char* name, uint16_t port) override;

    uint8_t connected() override;
    size_t write(const uint8_t *buf, size_t size) override;
    size_t write_P(PGM_P buf, size_t size) override;
    size_t write(const char *buf) {
      return write((const uint8_t*)buf, strlen(buf));
    }
    size_t write_P(const char *buf) {
      return write_P((PGM_P)buf, strlen_P(buf));
    }
    size_t write(Stream& stream); // Note this is not virtual
    int read(uint8_t *buf, size_t size) override;
    int available() override;
    int read() override;
    int peek() override;
    size_t peekBytes(uint8_t *buffer, size_t length) override;
    bool flush(unsigned int maxWaitMs);
    bool stop(unsigned int maxWaitMs);
    void flush() override { (void)flush(0); }
    void stop() override { (void)stop(0); }

    // Don't validate the chain, just accept whatever is given.  VERY INSECURE!
    void setInsecure() {
      _clearAuthenticationSettings();
      _use_insecure = true;
    }
    // Only check SHA1 fingerprint of certificate
    bool setPubKeyFingerprint(const uint8_t fingerprint[20]) {
      _clearAuthenticationSettings();
      _use_fingerprint = true;
      memcpy_P(_fingerprint, fingerprint, 20);
      return true;
    }
    bool setPubKeyFingerprint(const char *fpStr);
    // Install a client certificate for this connection, in case the server requires it (i.e. MQTT)
    void setClientRSACert(const X509List *cert, const PrivateKey *sk);

    // Sets the requested buffer size for transmit and receive
    void setBufferSizes(int recv, int xmit);

    // Returns whether MFLN negotiation for the above buffer sizes succeeded (after connection)
    int getMFLNStatus() {
      return connected() && br_ssl_engine_get_mfln_negotiated(_eng);
    }

    // Return an error code and possibly a text string in a passed-in buffer with last SSL failure
    int getLastSSLError(char *dest = NULL, size_t len = 0);

    // Select specific ciphers (i.e. optimize for speed over security)
    // These may be in PROGMEM or RAM, either will run properly
    bool setCiphers(const uint16_t *cipherAry, int cipherCount);
    bool setCiphers(std::vector<uint16_t> list);
    bool setCiphersLessSecure(); // Only use the limited set of RSA ciphers without EC

    // Check for Maximum Fragment Length support for given len before connection (possibly insecure)
    static bool probeMaxFragmentLength(IPAddress ip, uint16_t port, uint16_t len);
    static bool probeMaxFragmentLength(const char *hostname, uint16_t port, uint16_t len);
    static bool probeMaxFragmentLength(const String& host, uint16_t port, uint16_t len);

  private:
    void _clear();
    void _clearAuthenticationSettings();
    // Only one of the following two should ever be != nullptr!
    std::shared_ptr<br_ssl_client_context> _sc;
    std::shared_ptr<br_ssl_server_context> _sc_svr;
    inline bool ctx_present() {
      return (_sc != nullptr) || (_sc_svr != nullptr);
    }
    br_ssl_engine_context *_eng; // &_sc->eng, to allow for client or server contexts
    //std::shared_ptr<br_x509_minimal_context> _x509_minimal;
    std::shared_ptr<struct br_x509_pubkeyfingerprint_context> _x509_insecure;
    std::shared_ptr<unsigned char> _iobuf_in;
    std::shared_ptr<unsigned char> _iobuf_out;
    time_t _now;
    int _iobuf_in_size;
    int _iobuf_out_size;
    bool _handshake_done;
    bool _oom_err;

    bool _use_insecure;
    bool _use_fingerprint;
    uint8_t _fingerprint[20];
    unsigned _knownkey_usages;

    // Custom cipher list pointer or NULL if default
    std::shared_ptr<uint16_t> _cipher_list;
    uint8_t _cipher_cnt;

    unsigned char *_recvapp_buf;
    size_t _recvapp_len;

    bool _clientConnected(); // Is the underlying socket alive?
    bool _connectSSL(const char *hostName); // Do initial SSL handshake
    void _freeSSL();
    int _run_until(unsigned target, bool blocking = true);
    size_t _write(const uint8_t *buf, size_t size, bool pmem);
    bool _wait_for_handshake(); // Sets and return the _handshake_done after connecting

    // Optional client certificate
    const X509List *_chain;
    const PrivateKey *_sk;
    unsigned _allowed_usages;
    unsigned _cert_issuer_key_type;

    // X.509 validators differ from server to client
    bool _installClientX509Validator(); // Set up X509 validator for a client conn.
    bool _installServerX509Validator(const X509List *client_CA_ta); // Setup X509 client cert validation, if supplied

    uint8_t *_streamLoad(Stream& stream, size_t size);
};

};

#endif
