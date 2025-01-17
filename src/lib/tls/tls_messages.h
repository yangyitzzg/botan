/*
* TLS Messages
* (C) 2004-2011,2015 Jack Lloyd
*     2016 Matthias Gierlings
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#ifndef BOTAN_TLS_MESSAGES_H_
#define BOTAN_TLS_MESSAGES_H_

#include <vector>
#include <string>
#include <set>
#include <memory>
#include <optional>
#include <variant>

#include <botan/tls_extensions.h>
#include <botan/tls_handshake_msg.h>
#include <botan/tls_session.h>
#include <botan/tls_policy.h>
#include <botan/tls_ciphersuite.h>
#include <botan/pk_keys.h>
#include <botan/x509cert.h>

#if defined(BOTAN_HAS_CECPQ1)
  #include <botan/cecpq1.h>
#endif

namespace Botan {

class Public_Key;
class Credentials_Manager;

namespace OCSP {
class Response;
}

namespace TLS {

class Session;
class Handshake_IO;
class Handshake_State;
class Hello_Retry_Request;
class Callbacks;
class Cipher_State;

std::vector<uint8_t> make_hello_random(RandomNumberGenerator& rng,
                                       Callbacks& cb,
                                       const Policy& policy);

/**
* DTLS Hello Verify Request
*/
class BOTAN_UNSTABLE_API Hello_Verify_Request final : public Handshake_Message
   {
   public:
      std::vector<uint8_t> serialize() const override;
      Handshake_Type type() const override { return HELLO_VERIFY_REQUEST; }

      const std::vector<uint8_t>& cookie() const { return m_cookie; }

      explicit Hello_Verify_Request(const std::vector<uint8_t>& buf);

      Hello_Verify_Request(const std::vector<uint8_t>& client_hello_bits,
                           const std::string& client_identity,
                           const SymmetricKey& secret_key);

   private:
      std::vector<uint8_t> m_cookie;
   };

/**
* Client Hello Message
*/
class BOTAN_UNSTABLE_API Client_Hello : public Handshake_Message
   {
   public:
      Handshake_Type type() const override;

      /**
       * Return the version indicated in the ClientHello.
       * This may differ from the version indicated in the supported_versions extension.
       *
       * See RFC 8446 4.1.2:
       *   TLS 1.3, the client indicates its version preferences in the
       *   "supported_versions" extension (Section 4.2.1) and the
       *   legacy_version field MUST be set to 0x0303, which is the version
       *   number for TLS 1.2.
       */
      Protocol_Version legacy_version() const;

      const std::vector<uint8_t>& random() const;

      const std::vector<uint8_t>& session_id() const;

      const std::vector<uint16_t>& ciphersuites() const;

      bool offered_suite(uint16_t ciphersuite) const;

      std::vector<Signature_Scheme> signature_schemes() const;

      std::vector<Group_Params> supported_ecc_curves() const;

      std::vector<Group_Params> supported_dh_groups() const;

      std::vector<Protocol_Version> supported_versions() const;

      std::string sni_hostname() const;

      bool supports_alpn() const;

      bool sent_signature_algorithms() const;

      std::vector<std::string> next_protocols() const;

      std::vector<uint16_t> srtp_profiles() const;

      std::vector<uint8_t> serialize() const override;


      const std::vector<uint8_t>& cookie() const;

      std::vector<uint8_t> cookie_input_data() const;

      std::set<Handshake_Extension_Type> extension_types() const;

      const Extensions& extensions() const;

   protected:
      Client_Hello() : m_comp_methods({ 0 }) {}

      explicit Client_Hello(const std::vector<uint8_t>& buf);

      const std::vector<uint8_t>& compression_methods() const;

   protected:
      Protocol_Version m_legacy_version;
      std::vector<uint8_t> m_session_id;
      std::vector<uint8_t> m_random;
      std::vector<uint16_t> m_suites;
      std::vector<uint8_t> m_comp_methods;
      Extensions m_extensions;

      std::vector<uint8_t> m_hello_cookie; // DTLS only
      std::vector<uint8_t> m_cookie_input_bits; // DTLS only
   };

class BOTAN_UNSTABLE_API Client_Hello_12 final : public Client_Hello
   {
   public:
      class Settings final
         {
         public:
            Settings(const Protocol_Version version,
                     const std::string& hostname = ""):
               m_new_session_version(version),
               m_hostname(hostname) {}

            const Protocol_Version protocol_version() const { return m_new_session_version; }
            const std::string& hostname() const { return m_hostname; }

         private:
            const Protocol_Version m_new_session_version;
            const std::string m_hostname;
         };

   public:
      explicit Client_Hello_12(const std::vector<uint8_t>& buf) : Client_Hello(buf) {}

      Client_Hello_12(Handshake_IO& io,
                      Handshake_Hash& hash,
                      const Policy& policy,
                      Callbacks& cb,
                      RandomNumberGenerator& rng,
                      const std::vector<uint8_t>& reneg_info,
                      const Settings& client_settings,
                      const std::vector<std::string>& next_protocols);

      Client_Hello_12(Handshake_IO& io,
                      Handshake_Hash& hash,
                      const Policy& policy,
                      Callbacks& cb,
                      RandomNumberGenerator& rng,
                      const std::vector<uint8_t>& reneg_info,
                      const Session& session,
                      const std::vector<std::string>& next_protocols);

      using Client_Hello::random;
      using Client_Hello::compression_methods;

      bool prefers_compressed_ec_points() const;

      bool secure_renegotiation() const;

      std::vector<uint8_t> renegotiation_info() const;

      bool supports_session_ticket() const;

      std::vector<uint8_t> session_ticket() const;

      bool supports_extended_master_secret() const;

      bool supports_cert_status_message() const;

      bool supports_encrypt_then_mac() const;

      void update_hello_cookie(const Hello_Verify_Request& hello_verify);
   };

/**
* Server Hello Message
*/
class BOTAN_UNSTABLE_API Server_Hello : public Handshake_Message
   {
   public:
      std::vector<uint8_t> serialize() const override;

      Handshake_Type type() const override;

      // methods available in both subclasses' interface
      uint16_t ciphersuite() const;
      const Extensions& extensions() const;
      const std::vector<uint8_t>& session_id() const;

      virtual Protocol_Version selected_version() const = 0;

   protected:
      /**
       * Version-agnostic internal server hello data container that allows
       * parsing Server_Hello messages without prior knowledge of the contained
       * protocol version.
       */
      class Internal
         {
         public:
            Internal(const std::vector<uint8_t>& buf);

            Internal(Protocol_Version legacy_version,
                     std::vector<uint8_t> session_id,
                     std::vector<uint8_t> random,
                     const uint16_t ciphersuite,
                     const uint8_t comp_method);

            Protocol_Version version() const;

         public:
            Protocol_Version legacy_version;
            std::vector<uint8_t> session_id;
            std::vector<uint8_t> random;
            bool is_hello_retry_request;
            uint16_t ciphersuite;
            uint8_t  comp_method;

            Extensions  extensions;
         };

   protected:
      explicit Server_Hello(std::unique_ptr<Internal> data)
         : m_data(std::move(data)) {}

      // methods used internally and potentially exposed by one of the subclasses
      std::set<Handshake_Extension_Type> extension_types() const;
      const std::vector<uint8_t>& random() const;
      uint8_t compression_method() const;
      Protocol_Version legacy_version() const;

   protected:
      std::unique_ptr<Internal> m_data;
   };

class BOTAN_UNSTABLE_API Server_Hello_12 final : public Server_Hello
   {
   public:
      class Settings final
         {
         public:
            Settings(const std::vector<uint8_t> new_session_id,
                     Protocol_Version new_session_version,
                     uint16_t ciphersuite,
                     bool offer_session_ticket) :
               m_new_session_id(new_session_id),
               m_new_session_version(new_session_version),
               m_ciphersuite(ciphersuite),
               m_offer_session_ticket(offer_session_ticket) {}

            const std::vector<uint8_t>& session_id() const { return m_new_session_id; }
            Protocol_Version protocol_version() const { return m_new_session_version; }
            uint16_t ciphersuite() const { return m_ciphersuite; }
            bool offer_session_ticket() const { return m_offer_session_ticket; }

         private:
            const std::vector<uint8_t> m_new_session_id;
            Protocol_Version m_new_session_version;
            uint16_t m_ciphersuite;
            bool m_offer_session_ticket;
         };

      Server_Hello_12(Handshake_IO& io,
                      Handshake_Hash& hash,
                      const Policy& policy,
                      Callbacks& cb,
                      RandomNumberGenerator& rng,
                      const std::vector<uint8_t>& secure_reneg_info,
                      const Client_Hello_12& client_hello,
                      const Settings& settings,
                      const std::string& next_protocol);

      Server_Hello_12(Handshake_IO& io,
                      Handshake_Hash& hash,
                      const Policy& policy,
                      Callbacks& cb,
                      RandomNumberGenerator& rng,
                      const std::vector<uint8_t>& secure_reneg_info,
                      const Client_Hello_12& client_hello,
                      Session& resumed_session,
                      bool offer_session_ticket,
                      const std::string& next_protocol);

      explicit Server_Hello_12(const std::vector<uint8_t> &buf);

   protected:
      friend class Server_Hello_13;  // to allow construction by Server_Hello_13::parse()
      explicit Server_Hello_12(std::unique_ptr<Server_Hello::Internal> data);

   public:
      using Server_Hello::random;
      using Server_Hello::compression_method;
      using Server_Hello::extension_types;
      using Server_Hello::legacy_version;

      /**
       * @returns the selected version as indicated in the legacy_version field
       */
      Protocol_Version selected_version() const override;

      bool secure_renegotiation() const;

      std::vector<uint8_t> renegotiation_info() const;

      std::string next_protocol() const;

      bool supports_extended_master_secret() const;

      bool supports_encrypt_then_mac() const;

      bool supports_certificate_status_message() const;

      bool supports_session_ticket() const;

      uint16_t srtp_profile() const;
      bool prefers_compressed_ec_points() const;

      /**
       * Return desired downgrade version indicated by hello random, if any.
       */
      std::optional<Protocol_Version> random_signals_downgrade() const;
   };

/**
* Client Key Exchange Message
*/
class BOTAN_UNSTABLE_API Client_Key_Exchange final : public Handshake_Message
   {
   public:
      Handshake_Type type() const override { return CLIENT_KEX; }

      const secure_vector<uint8_t>& pre_master_secret() const
         { return m_pre_master; }

      Client_Key_Exchange(Handshake_IO& io,
                          Handshake_State& state,
                          const Policy& policy,
                          Credentials_Manager& creds,
                          const Public_Key* server_public_key,
                          const std::string& hostname,
                          RandomNumberGenerator& rng);

      Client_Key_Exchange(const std::vector<uint8_t>& buf,
                          const Handshake_State& state,
                          const Private_Key* server_rsa_kex_key,
                          Credentials_Manager& creds,
                          const Policy& policy,
                          RandomNumberGenerator& rng);

   private:
      std::vector<uint8_t> serialize() const override
         { return m_key_material; }

      std::vector<uint8_t> m_key_material;
      secure_vector<uint8_t> m_pre_master;
   };

/**
* Certificate Message of TLS 1.2
*/
class BOTAN_UNSTABLE_API Certificate_12 final : public Handshake_Message
   {
   public:
      Handshake_Type type() const override { return CERTIFICATE; }
      const std::vector<X509_Certificate>& cert_chain() const { return m_certs; }

      size_t count() const { return m_certs.size(); }
      bool empty() const { return m_certs.empty(); }

      Certificate_12(Handshake_IO& io,
                     Handshake_Hash& hash,
                     const std::vector<X509_Certificate>& certs);

      Certificate_12(const std::vector<uint8_t>& buf, const Policy& policy);

      std::vector<uint8_t> serialize() const override;

   private:
      std::vector<X509_Certificate> m_certs;
   };

/**
* Certificate Status (RFC 6066)
*/
class BOTAN_UNSTABLE_API Certificate_Status final : public Handshake_Message
   {
   public:
      Handshake_Type type() const override { return CERTIFICATE_STATUS; }

      //std::shared_ptr<const OCSP::Response> response() const { return m_response; }

      const std::vector<uint8_t>& response() const { return m_response; }

      explicit Certificate_Status(const std::vector<uint8_t>& buf);

      Certificate_Status(Handshake_IO& io,
                         Handshake_Hash& hash,
                         const OCSP::Response& response);

      /*
       * Create a Certificate_Status message using an already DER encoded OCSP response.
       */
      Certificate_Status(Handshake_IO& io,
                         Handshake_Hash& hash,
                         const std::vector<uint8_t>& raw_response_bytes);

   private:
      std::vector<uint8_t> serialize() const override;
      std::vector<uint8_t> m_response;
   };

/**
* Certificate Request Message
* TODO: this is 1.2 only
*/
class BOTAN_UNSTABLE_API Certificate_Req final : public Handshake_Message
   {
   public:
      Handshake_Type type() const override;

      const std::vector<std::string>& acceptable_cert_types() const;

      const std::vector<X509_DN>& acceptable_CAs() const;

      const std::vector<Signature_Scheme>& signature_schemes() const;

      Certificate_Req(Handshake_IO& io,
                      Handshake_Hash& hash,
                      const Policy& policy,
                      const std::vector<X509_DN>& allowed_cas);

      explicit Certificate_Req(const std::vector<uint8_t>& buf);

      std::vector<uint8_t> serialize() const override;

   private:
      std::vector<X509_DN> m_names;
      std::vector<std::string> m_cert_key_types;
      std::vector<Signature_Scheme> m_schemes;
   };

class BOTAN_UNSTABLE_API Certificate_Verify : public Handshake_Message
   {
   public:
      Handshake_Type type() const override { return CERTIFICATE_VERIFY; }

      Certificate_Verify(Handshake_IO& io,
                         Handshake_State& state,
                         const Policy& policy,
                         RandomNumberGenerator& rng,
                         const Private_Key* key);

      Certificate_Verify(const std::vector<uint8_t>& buf);

      std::vector<uint8_t> serialize() const override;

   protected:
      std::vector<uint8_t> m_signature;
      Signature_Scheme m_scheme = Signature_Scheme::NONE;
   };

/**
* Certificate Verify Message
*/
class BOTAN_UNSTABLE_API Certificate_Verify_12 final : public Certificate_Verify
   {
   public:
      using Certificate_Verify::Certificate_Verify;

      /**
      * Check the signature on a certificate verify message
      * @param cert the purported certificate
      * @param state the handshake state
      * @param policy the TLS policy
      */
      bool verify(const X509_Certificate& cert,
                  const Handshake_State& state,
                  const Policy& policy) const;
   };

/**
* Finished Message
*/
class BOTAN_UNSTABLE_API Finished : public Handshake_Message
   {
   public:
      explicit Finished(const std::vector<uint8_t>& buf);

      Handshake_Type type() const override { return FINISHED; }

      std::vector<uint8_t> verify_data() const;

      std::vector<uint8_t> serialize() const override;

   protected:
      using Handshake_Message::Handshake_Message;
      std::vector<uint8_t> m_verification_data;
   };

class BOTAN_UNSTABLE_API Finished_12 final : public Finished
   {
   public:
      using Finished::Finished;
      Finished_12(Handshake_IO& io,
                  Handshake_State& state,
                  Connection_Side side);

      bool verify(const Handshake_State& state, Connection_Side side) const;
   };

/**
* Hello Request Message
*/
class BOTAN_UNSTABLE_API Hello_Request final : public Handshake_Message
   {
   public:
      Handshake_Type type() const override { return HELLO_REQUEST; }

      explicit Hello_Request(Handshake_IO& io);
      explicit Hello_Request(const std::vector<uint8_t>& buf);

   private:
      std::vector<uint8_t> serialize() const override;
   };

/**
* Server Key Exchange Message
*/
class BOTAN_UNSTABLE_API Server_Key_Exchange final : public Handshake_Message
   {
   public:
      Handshake_Type type() const override { return SERVER_KEX; }

      const std::vector<uint8_t>& params() const { return m_params; }

      bool verify(const Public_Key& server_key,
                  const Handshake_State& state,
                  const Policy& policy) const;

      // Only valid for certain kex types
      const Private_Key& server_kex_key() const;

#if defined(BOTAN_HAS_CECPQ1)
      // Only valid for CECPQ1 negotiation
      const CECPQ1_key& cecpq1_key() const
         {
         BOTAN_ASSERT_NONNULL(m_cecpq1_key);
         return *m_cecpq1_key;
         }
#endif

      Server_Key_Exchange(Handshake_IO& io,
                          Handshake_State& state,
                          const Policy& policy,
                          Credentials_Manager& creds,
                          RandomNumberGenerator& rng,
                          const Private_Key* signing_key = nullptr);

      Server_Key_Exchange(const std::vector<uint8_t>& buf,
                          Kex_Algo kex_alg,
                          Auth_Method sig_alg,
                          Protocol_Version version);

   private:
      std::vector<uint8_t> serialize() const override;

#if defined(BOTAN_HAS_CECPQ1)
      std::unique_ptr<CECPQ1_key> m_cecpq1_key;
#endif

      std::unique_ptr<Private_Key> m_kex_key;

      std::vector<uint8_t> m_params;

      std::vector<uint8_t> m_signature;
      Signature_Scheme m_scheme = Signature_Scheme::NONE;
   };

/**
* Server Hello Done Message
*/
class BOTAN_UNSTABLE_API Server_Hello_Done final : public Handshake_Message
   {
   public:
      Handshake_Type type() const override { return SERVER_HELLO_DONE; }

      explicit Server_Hello_Done(Handshake_IO& io, Handshake_Hash& hash);
      explicit Server_Hello_Done(const std::vector<uint8_t>& buf);

   private:
      std::vector<uint8_t> serialize() const override;
   };

/**
* New Session Ticket Message
*/
class BOTAN_UNSTABLE_API New_Session_Ticket_12 final : public Handshake_Message
   {
   public:
      Handshake_Type type() const override { return NEW_SESSION_TICKET; }

      uint32_t ticket_lifetime_hint() const { return m_ticket_lifetime_hint; }
      const std::vector<uint8_t>& ticket() const { return m_ticket; }

      New_Session_Ticket_12(Handshake_IO& io,
                            Handshake_Hash& hash,
                            const std::vector<uint8_t>& ticket,
                            uint32_t lifetime);

      New_Session_Ticket_12(Handshake_IO& io,
                            Handshake_Hash& hash);

      explicit New_Session_Ticket_12(const std::vector<uint8_t>& buf);

      std::vector<uint8_t> serialize() const override;

   private:
      uint32_t m_ticket_lifetime_hint = 0;
      std::vector<uint8_t> m_ticket;
   };

/**
* Change Cipher Spec
*/
class BOTAN_UNSTABLE_API Change_Cipher_Spec final : public Handshake_Message
   {
   public:
      Handshake_Type type() const override { return HANDSHAKE_CCS; }

      std::vector<uint8_t> serialize() const override
         { return std::vector<uint8_t>(1, 1); }
   };

}

}

#endif
