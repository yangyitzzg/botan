/*
* Serpent
* (C) 1999-2007 Jack Lloyd
*
* Distributed under the terms of the Botan license
*/

#ifndef BOTAN_SERPENT_H__
#define BOTAN_SERPENT_H__

#include <botan/block_cipher.h>

namespace Botan {

/*
* Serpent
*/
class BOTAN_DLL Serpent : public BlockCipher
   {
   public:
      void encrypt_n(const byte in[], byte out[], u32bit blocks) const;
      void decrypt_n(const byte in[], byte out[], u32bit blocks) const;

      void clear() { round_key.clear(); }
      std::string name() const { return "Serpent"; }
      BlockCipher* clone() const { return new Serpent; }
      Serpent() : BlockCipher(16, 16, 32, 8) {}
   protected:
      void key_schedule(const byte[], u32bit);

      SecureVector<u32bit, 132> round_key;
   };

}

#endif
