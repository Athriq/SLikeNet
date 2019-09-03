/*
 *  Copyright (c) 2018-2019, SLikeSoft UG (haftungsbeschr�nkt)
 *
 *  This source code is  licensed under the MIT-style license found in the license.txt
 *  file in the root directory of this source tree.
 */
#pragma once

#include "ifileencrypter.h"   // used for Crypto::IFileEncrypter
#include "securestring.h"     // used for Crypto::CSecureString
#include "../RakString.h"     // used for RakString
 #include <openssl/ossl_typ.h> // used for RSA

//struct RSA;

namespace SLNet
{
	namespace Experimental
	{
		namespace Crypto
		{
			class CFileEncrypter : public IFileEncrypter
			{
				// member variables
				RSA* m_privateKey;
				RSA* m_publicKey;

				// constructor
			public:
				// #high - drop the default ctor again (provide load from file instead incl. routing through customized file open handlers)
				CFileEncrypter() = default;
				CFileEncrypter(const CSecureString &privateKey, const char *publicKey, size_t publicKeyLength);
				~CFileEncrypter();
			
				// signing methods
			public:
				const unsigned char* SignData(const unsigned char *data, const size_t dataLength) override;
				const char* SignDataBase64(const unsigned char *data, const size_t dataLength) override;
				// #med reconsider/review interface here (char / unsigned char)
				bool VerifyData(const unsigned char *data, const size_t dataLength, const unsigned char *signature, const size_t signatureLength) override;
				bool VerifyDataBase64(const unsigned char *data, const size_t dataLength, const char *signature, const size_t signatureLength) override;

				// internal helpers
			private:
				const char* SetPublicKey(const char *publicKey, size_t publicKeyLength);
			};
		}
	}
}