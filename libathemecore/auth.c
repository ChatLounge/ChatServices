/*
 * ChatServices: A collection of minimalist IRC services
 * auth.c: Authentication.
 *
 * Copyright (c) 2005-2009 Atheme Project (http://www.atheme.org)
 * Copyright (c) 2017 ChatLounge IRC Network Development Team (http://www.chatlounge.net)
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

bool auth_module_loaded = false;
bool (*auth_user_custom)(myuser_t *mu, const char *password);

void set_password(myuser_t *mu, const char *newpassword)
{
	if (mu == NULL || newpassword == NULL)
		return;

	/* if we can, try to crypt it */
	if (crypto_module_loaded)
	{
		mu->flags |= MU_CRYPTPASS;
		mowgli_strlcpy(mu->pass, crypt_string(newpassword, gen_salt()), PASSLEN);
	}
	else
	{
		mu->flags &= ~MU_CRYPTPASS;			/* just in case */
		mowgli_strlcpy(mu->pass, newpassword, PASSLEN);
	}
}

bool verify_password(myuser_t *mu, const char *password)
{
	if (mu == NULL || password == NULL)
		return false;

	if (auth_module_loaded && auth_user_custom)
		return auth_user_custom(mu, password);

	if (mu->flags & MU_CRYPTPASS)
		if (crypto_module_loaded)
		{
			const crypt_impl_t *ci, *ci_default;

			ci = crypt_verify_password(password, mu->pass);
			if (ci == NULL)
				return false;

			if (ci == (ci_default = crypt_get_default_provider()))
			{
				if (ci->needs_param_upgrade != NULL && ci->needs_param_upgrade(mu->pass))
				{
					slog(LG_INFO, "verify_password(): transitioning to newer parameters for crypt scheme '%s' for account '%s'",
							ci->id, entity(mu)->name);

					mowgli_strlcpy(mu->pass, ci->crypt(password, ci->salt()), PASSLEN);
				}
			}
			else
			{
				slog(LG_INFO, "verify_password(): transitioning from crypt scheme '%s' to '%s' for account '%s'",
					      ci->id, ci_default->id, entity(mu)->name);

				mowgli_strlcpy(mu->pass, ci_default->crypt(password, ci_default->salt()), PASSLEN);
			}

			return true;
		}
		else
		{	/* not good!
			 * but don't complain about crypted password '*',
			 * this is supposed to never match
			 */
			if (strcmp(password, "*"))
				slog(LG_ERROR, "check_password(): can't check crypted password -- no crypto module!");
			return false;
		}
	else
		return (strcmp(mu->pass, password) == 0);
}

