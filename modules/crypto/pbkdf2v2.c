/*
 * Copyright (C) 2015 Aaron Jones <aaronmdjones@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "atheme.h"

#ifdef HAVE_OPENSSL

DECLARE_MODULE_V1("crypto/pbkdf2v2", false, _modinit, _moddeinit,
                  PACKAGE_VERSION, VENDOR_STRING);

#include <openssl/evp.h>

/*
 * Do not change anything below this line unless you know what you are doing,
 * AND how it will (possibly) break backward-, forward-, or cross-compatibility
 *
 * In particular, the salt length SHOULD NEVER BE CHANGED. 128 bits is more than
 * sufficient.
 */

#define PBKDF2_SALTLEN		16
#define PBKDF2_F_SCAN		"$z$%u$%u$%16[A-Za-z0-9]$"
#define PBKDF2_F_SALT		"$z$%u$%u$%s$"
#define PBKDF2_F_PRINT		"$z$%u$%u$%s$%s"

#define PBKDF2_C_MIN		10000
#define PBKDF2_C_MAX		5000000
#define PBKDF2_C_DEF		64000

static const char salt_chars[62] =
	"AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz0123456789";

static unsigned int pbkdf2v2_digest = 6; /* SHA512 */
static unsigned int pbkdf2v2_rounds = PBKDF2_C_DEF;

static const char *pbkdf2v2_make_salt(void)
{
	char		salt[PBKDF2_SALTLEN + 1];
	static char	result[PASSLEN];

	memset(salt, 0x00, sizeof salt);
	memset(result, 0x00, sizeof result);

	for (int i = 0; i < PBKDF2_SALTLEN; i++)
		salt[i] = salt_chars[arc4random() % sizeof salt_chars];

	(void) snprintf(result, sizeof result, PBKDF2_F_SALT,
	                pbkdf2v2_digest, pbkdf2v2_rounds, salt);

	return result;
}

static const char *pbkdf2v2_crypt(const char *pass, const char *crypt_str)
{
	unsigned int	prf = 0, iter = 0;
	char		salt[PBKDF2_SALTLEN + 1];

	const EVP_MD*	md = NULL;
	unsigned char	digest[EVP_MAX_MD_SIZE];
	char		digest_b64[(EVP_MAX_MD_SIZE * 2) + 5];
	static char	result[PASSLEN];

	/*
	 * Attempt to extract the PRF, iteration count and salt
	 *
	 * If this fails, we're trying to verify a hash not produced by
	 * this module - just bail out, libathemecore can handle NULL
	 */
	if (sscanf(crypt_str, PBKDF2_F_SCAN, &prf, &iter, salt) < 3)
		return NULL;

	/* Look up the digest method corresponding to the PRF */
	switch (prf) {

	case 5:
		md = EVP_sha256();
		break;

	case 6:
		md = EVP_sha512();
		break;

	default:
		/*
		 * Similar to above, trying to verify a password
		 * that we cannot ever verify - bail out here
		 */
		return NULL;
	}

	/* Compute the PBKDF2 digest */
	size_t sl = strlen(salt);
	size_t pl = strlen(pass);
	(void) PKCS5_PBKDF2_HMAC(pass, pl, (unsigned char *) salt, sl,
	                         iter, md, EVP_MD_size(md), digest);

	/* Convert the digest to Base 64 */
	memset(digest_b64, 0x00, sizeof digest_b64);
	(void) base64_encode((const char *) digest, EVP_MD_size(md),
	                     digest_b64, sizeof digest_b64);

	/* Format the result */
	memset(result, 0x00, sizeof result);
	(void) snprintf(result, sizeof result, PBKDF2_F_PRINT,
	                prf, iter, salt, digest_b64);

	return result;
}

static bool pbkdf2v2_needs_param_upgrade(const char *user_pass_string)
{
	unsigned int	prf = 0, iter = 0;
	char		salt[PBKDF2_SALTLEN + 1];

	if (sscanf(user_pass_string, PBKDF2_F_SCAN, &prf, &iter, salt) < 3)
		return 0;

	if (prf != pbkdf2v2_digest)
		return 1;

	if (iter != pbkdf2v2_rounds)
		return 1;

	return 0;
}

static int c_ci_pbkdf2v2_digest(mowgli_config_file_entry_t *ce)
{
	if (ce->vardata == NULL)
	{
		conf_report_warning(ce, "no parameter for configuration option");
		return 0;
	}

	if (!strcasecmp(ce->vardata, "SHA256"))
		pbkdf2v2_digest = 5;
	else if (!strcasecmp(ce->vardata, "SHA512"))
		pbkdf2v2_digest = 6;
	else
		conf_report_warning(ce, "invalid parameter for configuration option");

	return 0;
}

static crypt_impl_t pbkdf2v2_crypt_impl = {
	.id = "pbkdf2v2",
	.crypt = &pbkdf2v2_crypt,
	.salt = &pbkdf2v2_make_salt,
	.needs_param_upgrade = &pbkdf2v2_needs_param_upgrade,
};

static mowgli_list_t conf_pbkdf2v2_table;

void _modinit(module_t* m)
{
	crypt_register(&pbkdf2v2_crypt_impl);

	add_subblock_top_conf("PBKDF2V2", &conf_pbkdf2v2_table);
	add_conf_item("DIGEST", &conf_pbkdf2v2_table, c_ci_pbkdf2v2_digest);
	add_uint_conf_item("ROUNDS", &conf_pbkdf2v2_table, 0, &pbkdf2v2_rounds,
	                             PBKDF2_C_MIN, PBKDF2_C_MAX, PBKDF2_C_DEF);
}

void _moddeinit(module_unload_intent_t intent)
{
	del_conf_item("DIGEST", &conf_pbkdf2v2_table);
	del_conf_item("ROUNDS", &conf_pbkdf2v2_table);
	del_top_conf("PBKDF2V2");

	crypt_unregister(&pbkdf2v2_crypt_impl);
}

#endif /* HAVE_OPENSSL */
