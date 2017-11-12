/*
 * Copyright (c) 2005 Atheme Development Group
 * Rights to this code are as documented in doc/LICENSE.
 *
 * IRCServices's weird password encryption thingy, taken from Anope 1.6.3.
 *
 */
/* Include file for high-level encryption routines.
 *
 * (C) 2003 Anope Team
 * Contact us at info@anope.org
 *
 * Please read COPYING and README for furhter details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 *
 * $\Id: myencrypt.c 5 2004-03-29 01:29:50Z dane $
 *
 */

#include "atheme.h"

/* necessary anope defines */
#define PASSMAX 32
#define ENCRYPT_MD5

/*************************************************************************/

/******** Our own high-level routines. ********/


#define XTOI(c) ((c)>9 ? (c)-'A'+10 : (c)-'0')
/* Result of this:
 * c in [-128,9]  =>  [-183,-46]
 * c in [10,127]  =>  [-38,79]
 */

/* Encrypt `src' of length `len' and store the result in `dest'.  If the
 * resulting string would be longer than `size', return -1 and leave `dest'
 * unchanged; else return 0.
 */
static int myencrypt(const char *src, int len, char *dest, int size)
{

#ifdef ENCRYPT_MD5

    md5_state_t context;
    char digest[33];
    char dest2[16];
    int i;

    if (size < 32)
        return -1;

    memset(&context, 0, sizeof(context));
    memset(&digest, 0, sizeof(digest));

    md5_init(&context);
    md5_append(&context, (unsigned const char *) src, (size_t) len);
    md5_finish(&context, (unsigned char *) digest);

    for (i = 0; i < 32; i += 2)
        dest2[i / 2] = XTOI(digest[i]) << 4 | XTOI(digest[i + 1]);

    /* convert to hex, skipping last 8 bytes (constant) -- jilles */
    strcpy(dest, "$ircservices$");
    for (i = 0; i <= 7; i++)
        sprintf(dest + 13 + 2 * i, "%02x", 255 & dest2[i]);
    return 0;

#else

    return -1;                  /* unknown encryption algorithm */

#endif

}

#if 0 /* unused */
/* Shortcut for encrypting a null-terminated string in place. */
static int encrypt_in_place(char *buf, int size)
{
    return myencrypt(buf, strlen(buf), buf, size);
}
#endif

/* Compare a plaintext string against an encrypted password.  Return 1 if
 * they match, 0 if not, and -1 if something went wrong. */

static int check_password(const char *plaintext, const char *password)
{
    char buf[BUFSIZE];
    int cmp = 0;

    if (myencrypt(plaintext, strlen(plaintext), buf, sizeof(buf)) < 0)
        return -1;
#ifdef ENCRYPT_MD5
    cmp = strcmp(buf, password) == 0;
#endif
    return cmp;
}

/*************************************************************************/

DECLARE_MODULE_V1
(
	"crypto/ircservices", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"Jilles Tjoelker <jilles@stack.nl>"
);

static const char *ircservices_crypt_string(const char *key, const char *salt)
{
	static char output[PASSMAX];
	if (salt[0] == '$' && salt[1] == '1') /* this is a new pw XXX */
	{
		myencrypt(key, strlen(key), output, PASSMAX);
		return output;
	}
	else
	{
		if (check_password(key, salt))
			return salt;
		else
		{
			output[0] = '\0';
			return output;
		}
	}
}

static crypt_impl_t ircservices_crypt_impl = {
	.id = "ircservices",
	.crypt = &ircservices_crypt_string,
};

void _modinit(module_t *m)
{
	crypt_register(&ircservices_crypt_impl);
}

void _moddeinit(module_unload_intent_t intent)
{
	crypt_unregister(&ircservices_crypt_impl);
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
