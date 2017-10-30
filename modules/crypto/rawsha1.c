/*
 * Copyright (c) 2009 Atheme Development Group
 * Rights to this code are as documented in doc/LICENSE.
 *
 * Raw SHA1 password encryption, as used by e.g. Anope 1.8.
 * Hash functions are not designed to encrypt passwords directly,
 * but we need this to convert some Anope databases.
 */

#include "atheme.h"

#ifdef HAVE_OPENSSL

#include <openssl/sha.h>

DECLARE_MODULE_V1
(
	"crypto/rawsha1", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	VENDOR_STRING
);

#define RAWSHA1_PREFIX "$rawsha1$"

static const char *rawsha1_crypt_string(const char *key, const char *salt)
{
	static char output[2 * SHA_DIGEST_LENGTH + sizeof(RAWSHA1_PREFIX)];
	SHA_CTX ctx;
	unsigned char digest[SHA_DIGEST_LENGTH];
	int i;

	SHA1_Init(&ctx);
	SHA1_Update(&ctx, key, strlen(key));
	SHA1_Final(digest, &ctx);

	strcpy(output, RAWSHA1_PREFIX);
	for (i = 0; i < SHA_DIGEST_LENGTH; i++)
		sprintf(output + sizeof(RAWSHA1_PREFIX) - 1 + i * 2, "%02x",
				255 & digest[i]);

	return output;
}

static crypt_impl_t rawsha1_crypt_impl = {
	.id = "rawsha1",
	.crypt = &rawsha1_crypt_string,
};

void _modinit(module_t *m)
{
	crypt_register(&rawsha1_crypt_impl);
}

void _moddeinit(module_unload_intent_t intent)
{
	crypt_unregister(&rawsha1_crypt_impl);
}

#endif /* HAVE_OPENSSL */

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
