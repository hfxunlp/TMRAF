/*
 *  Adapted in part from MIT Kerberos 5-1.2.1 slave/kprop.c and from
 *  http://docs.sun.com/?p=/doc/816-1331/6m7oo9sms&a=view
 *
 *  Copyright (c) 2002-2004 The Regents of the University of Michigan.
 *  All rights reserved.
 *
 *  Andy Adamson <andros@umich.edu>
 *  J. Bruce Fields <bfields@umich.edu>
 *  Marius Aamodt Eriksen <marius@umich.edu>
 *  Kevin Coffman <kwc@umich.edu>
 */

/*
 * slave/kprop.c
 *
 * Copyright 1990,1991 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/*
 * Copyright 1994 by OpenVision Technologies, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OpenVision not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission. OpenVision makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * OPENVISION DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL OPENVISION BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/*
  krb5_util.c

  Copyright (c) 2004 The Regents of the University of Michigan.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of the University nor the names of its
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif	/* HAVE_CONFIG_H */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/param.h>
#include <rpc/rpc.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <netdb.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <gssapi/gssapi.h>
#ifdef USE_PRIVATE_KRB5_FUNCTIONS
#include <gssapi/gssapi_krb5.h>
#endif
#include <krb5.h>
#include <rpc/auth_gss.h>

#include <sys/types.h>
#include <fcntl.h>

#include "nfslib.h"
#include "gssd.h"
#include "err_util.h"
#include "gss_util.h"
#include "krb5_util.h"

/*
 * List of principals from our keytab that we
 * will try to use to obtain credentials
 * (known as a principal list entry (ple))
 */
struct gssd_k5_kt_princ {
	struct gssd_k5_kt_princ *next;
	// Only protect against deletion, not modification
	int refcount;
	// Only set during creation in new_ple()
	krb5_principal princ;
	char *realm;
	// Modified during usage by gssd_get_single_krb5_cred()
	char *ccname;
	krb5_timestamp endtime;
};


/* Global list of principals/cache file names for machine credentials */
static struct gssd_k5_kt_princ *gssd_k5_kt_princ_list = NULL;
/* This mutex protects list modification & ple->ccname */
static pthread_mutex_t ple_lock = PTHREAD_MUTEX_INITIALIZER;

#ifdef HAVE_SET_ALLOWABLE_ENCTYPES
int limit_to_legacy_enctypes = 0;
#endif

/*==========================*/
/*===  Internal routines ===*/
/*==========================*/

static int select_krb5_ccache(const struct dirent *d);
static int gssd_find_existing_krb5_ccache(uid_t uid, char *dirname,
		const char **cctype, struct dirent **d);
static int gssd_get_single_krb5_cred(krb5_context context,
		krb5_keytab kt, struct gssd_k5_kt_princ *ple);
static int query_krb5_ccache(const char* cred_cache, char **ret_princname,
		char **ret_realm);

static void release_ple_locked(krb5_context context,
			       struct gssd_k5_kt_princ *ple)
{
	if (--ple->refcount)
		return;

	printerr(3, "freeing cached principal (ccname=%s, realm=%s)\n",
		 ple->ccname, ple->realm);
	krb5_free_principal(context, ple->princ);
	free(ple->ccname);
	free(ple->realm);
	free(ple);
}

static void release_ple(krb5_context context, struct gssd_k5_kt_princ *ple)
{
	pthread_mutex_lock(&ple_lock);
	release_ple_locked(context, ple);
	pthread_mutex_unlock(&ple_lock);
}


/*
 * Called from the scandir function to weed out potential krb5
 * credentials cache files
 *
 * Returns:
 *	0 => don't select this one
 *	1 => select this one
 */
static int
select_krb5_ccache(const struct dirent *d)
{
	/*
	 * Note: We used to check d->d_type for DT_REG here,
	 * but apparenlty reiser4 always has DT_UNKNOWN.
	 * Check for IS_REG after stat() call instead.
	 */
	if (strstr(d->d_name, GSSD_DEFAULT_CRED_PREFIX))
		return 1;
	else
		return 0;
}

/*
 * Look in directory "dirname" for files that look like they
 * are Kerberos Credential Cache files for a given UID.
 *
 * Returns 0 if a valid-looking entry is found.  "*cctype" is
 * set to the name of the cache type.  A pointer to the dirent
 * is planted in "*d".  Caller must free "*d" with free(3).
 *
 * Otherwise, a negative errno is returned.
 */
static int
gssd_find_existing_krb5_ccache(uid_t uid, char *dirname,
			       const char **cctype, struct dirent **d)
{
	struct dirent **namelist;
	int n;
	int i;
	int found = 0;
	struct dirent *best_match_dir = NULL;
	struct stat best_match_stat, tmp_stat;
	/* dirname + cctype + d_name + NULL */
	char buf[PATH_MAX+5+256+1];
	char *princname = NULL;
	char *realm = NULL;
	int score, best_match_score = 0, err = -EACCES;

	memset(&best_match_stat, 0, sizeof(best_match_stat));
	*cctype = NULL;
	*d = NULL;
	n = scandir(dirname, &namelist, select_krb5_ccache, 0);
	if (n < 0) {
		printerr(1, "Error doing scandir on directory '%s': %s\n",
			dirname, strerror(errno));
	}
	else if (n > 0) {
		for (i = 0; i < n; i++) {
			snprintf(buf, sizeof(buf),
				 "%s/%s", dirname, namelist[i]->d_name);
			printerr(3, "CC '%s' being considered, "
				 "with preferred realm '%s'\n",
				 buf, preferred_realm ?
					preferred_realm : "<none selected>");
			if (lstat(buf, &tmp_stat)) {
				printerr(0, "Error doing stat on '%s'\n", buf);
				free(namelist[i]);
				continue;
			}
			/* Only pick caches owned by the user (uid) */
			if (tmp_stat.st_uid != uid) {
				printerr(3, "CC '%s' owned by %u, not %u\n",
					 buf, tmp_stat.st_uid, uid);
				free(namelist[i]);
				continue;
			}
			if (!S_ISREG(tmp_stat.st_mode) &&
			    !S_ISDIR(tmp_stat.st_mode)) {
				printerr(3, "CC '%s' is not a regular "
					 "file or directory\n", buf);
				free(namelist[i]);
				continue;
			}
			if (uid == 0 && !root_uses_machine_creds && 
				strstr(namelist[i]->d_name, "machine_")) {
				printerr(3, "CC '%s' not available to root\n", buf);
				free(namelist[i]);
				continue;
			}
			if (S_ISDIR(tmp_stat.st_mode)) {
				*cctype = "DIR";
			} else
			if (S_ISREG(tmp_stat.st_mode)) {
				*cctype = "FILE";
			} else {
				continue;
			}
			snprintf(buf, sizeof(buf), "%s:%s/%s", *cctype,
				 dirname, namelist[i]->d_name);
			if (!query_krb5_ccache(buf, &princname, &realm)) {
				printerr(3, "CC '%s' is expired or corrupt\n",
					 buf);
				free(namelist[i]);
				err = -EKEYEXPIRED;
				continue;
			}

			score = 0;
			if (preferred_realm &&
					strcmp(realm, preferred_realm) == 0) 
				score++;

			printerr(3, "CC '%s'(%s@%s) passed all checks and"
				    " has mtime of %u\n",
				 buf, princname, realm, 
				 tmp_stat.st_mtime);
			/*
			 * if more than one match is found, return the most
			 * recent (the one with the latest mtime), and
			 * don't free the dirent
			 */
			if (!found) {
				best_match_dir = namelist[i];
				best_match_stat = tmp_stat;
				best_match_score = score;
				found++;
			}
			else {
				/*
				 * If current score is higher than best match 
				 * score, we use the current match. Otherwise,
				 * if the current match has an mtime later
				 * than the one we are looking at, then use
				 * the current match.  Otherwise, we still
				 * have the best match.
				 */
				if (best_match_score < score ||
				    (best_match_score == score && 
				       tmp_stat.st_mtime >
					    best_match_stat.st_mtime)) {
					free(best_match_dir);
					best_match_dir = namelist[i];
					best_match_stat = tmp_stat;
					best_match_score = score;
				}
				else {
					free(namelist[i]);
				}
				printerr(3, "CC '%s:%s/%s' is our "
					    "current best match "
					    "with mtime of %u\n",
					 cctype, dirname,
					 best_match_dir->d_name,
					 best_match_stat.st_mtime);
			}
			free(princname);
			free(realm);
		}
		free(namelist);
	}
	if (found) {
		*d = best_match_dir;
		return 0;
	}

	return err;
}

/* check if the ticket cache exists, if not set nocache=1 so that new
 * tgt is gotten
 */
static int
gssd_check_if_cc_exists(struct gssd_k5_kt_princ *ple)
{
	int fd;
	char cc_name[BUFSIZ];

	snprintf(cc_name, sizeof(cc_name), "%s/%s%s_%s",
		ccachesearch[0], GSSD_DEFAULT_CRED_PREFIX,
		GSSD_DEFAULT_MACHINE_CRED_SUFFIX, ple->realm);
	fd = open(cc_name, O_RDONLY);
	if (fd < 0)
		return 1;
	close(fd);
	return 0;
}

/*
 * Obtain credentials via a key in the keytab given
 * a keytab handle and a gssd_k5_kt_princ structure.
 * Checks to see if current credentials are expired,
 * if not, uses the keytab to obtain new credentials.
 *
 * Returns:
 *	0 => success (or credentials have not expired)
 *	nonzero => error
 */
static int
gssd_get_single_krb5_cred(krb5_context context,
			  krb5_keytab kt,
			  struct gssd_k5_kt_princ *ple)
{
#ifdef HAVE_KRB5_GET_INIT_CREDS_OPT_SET_ADDRESSLESS
	krb5_get_init_creds_opt *init_opts = NULL;
#else
	krb5_get_init_creds_opt options;
#endif
	krb5_get_init_creds_opt *opts;
	krb5_creds my_creds;
	krb5_ccache ccache = NULL;
	char kt_name[BUFSIZ];
	char cc_name[BUFSIZ];
	int code;
	time_t now = time(0);
	char *cache_type;
	char *pname = NULL;
	char *k5err = NULL;
	int nocache = 0;
	pthread_t tid = pthread_self();

	memset(&my_creds, 0, sizeof(my_creds));

	if (!use_memcache)
		nocache = gssd_check_if_cc_exists(ple);
	/*
	 * Workaround for clock skew among NFS server, NFS client and KDC
	 * 300 because clock skew must be within 300sec for kerberos
	 */
	now += 300;
	pthread_mutex_lock(&ple_lock);
	if (ple->ccname && ple->endtime > now && !nocache) {
		printerr(3, "%s(0x%lx): Credentials in CC '%s' are good until %s",
			 __func__, tid, ple->ccname, ctime((time_t *)&ple->endtime));
		code = 0;
		pthread_mutex_unlock(&ple_lock);
		goto out;
	}
	pthread_mutex_unlock(&ple_lock);

	if ((code = krb5_kt_get_name(context, kt, kt_name, BUFSIZ))) {
		printerr(0, "ERROR: Unable to get keytab name in "
			    "gssd_get_single_krb5_cred\n");
		goto out;
	}

	if ((krb5_unparse_name(context, ple->princ, &pname)))
		pname = NULL;

#ifdef HAVE_KRB5_GET_INIT_CREDS_OPT_SET_ADDRESSLESS
	code = krb5_get_init_creds_opt_alloc(context, &init_opts);
	if (code) {
		k5err = gssd_k5_err_msg(context, code);
		printerr(0, "ERROR: %s allocating gic options\n", k5err);
		goto out;
	}
	if (krb5_get_init_creds_opt_set_addressless(context, init_opts, 1))
		printerr(1, "WARNING: Unable to set option for addressless "
			 "tickets.  May have problems behind a NAT.\n");
#ifdef TEST_SHORT_LIFETIME
	/* set a short lifetime (for debugging only!) */
	printerr(1, "WARNING: Using (debug) short machine cred lifetime!\n");
	krb5_get_init_creds_opt_set_tkt_life(init_opts, 5*60);
#endif
	opts = init_opts;

#else	/* HAVE_KRB5_GET_INIT_CREDS_OPT_SET_ADDRESSLESS */

	krb5_get_init_creds_opt_init(&options);
	krb5_get_init_creds_opt_set_address_list(&options, NULL);
#ifdef TEST_SHORT_LIFETIME
	/* set a short lifetime (for debugging only!) */
	printerr(0, "WARNING: Using (debug) short machine cred lifetime!\n");
	krb5_get_init_creds_opt_set_tkt_life(&options, 5*60);
#endif
	opts = &options;
#endif

	if ((code = krb5_get_init_creds_keytab(context, &my_creds, ple->princ,
					       kt, 0, NULL, opts))) {
		k5err = gssd_k5_err_msg(context, code);
		printerr(1, "WARNING: %s while getting initial ticket for "
			 "principal '%s' using keytab '%s'\n", k5err,
			 pname ? pname : "<unparsable>", kt_name);
		goto out;
	}

	/*
	 * Initialize cache file which we're going to be using
	 */

	pthread_mutex_lock(&ple_lock);
	if (use_memcache)
	    cache_type = "MEMORY";
	else
	    cache_type = "FILE";
	snprintf(cc_name, sizeof(cc_name), "%s:%s/%s%s_%s",
		cache_type,
		ccachesearch[0], GSSD_DEFAULT_CRED_PREFIX,
		GSSD_DEFAULT_MACHINE_CRED_SUFFIX, ple->realm);
	ple->endtime = my_creds.times.endtime;
	if (ple->ccname == NULL || strcmp(ple->ccname, cc_name) != 0) {
		free(ple->ccname);
		ple->ccname = strdup(cc_name);
		if (ple->ccname == NULL) {
			printerr(0, "ERROR: no storage to duplicate credentials "
				    "cache name '%s'\n", cc_name);
			code = ENOMEM;
			pthread_mutex_unlock(&ple_lock);
			goto out;
		}
	}
	pthread_mutex_unlock(&ple_lock);
	if ((code = krb5_cc_resolve(context, cc_name, &ccache))) {
		k5err = gssd_k5_err_msg(context, code);
		printerr(0, "ERROR: %s while opening credential cache '%s'\n",
			 k5err, cc_name);
		goto out;
	}
	if ((code = krb5_cc_initialize(context, ccache, ple->princ))) {
		k5err = gssd_k5_err_msg(context, code);
		printerr(0, "ERROR: %s while initializing credential "
			 "cache '%s'\n", k5err, cc_name);
		goto out;
	}
	if ((code = krb5_cc_store_cred(context, ccache, &my_creds))) {
		k5err = gssd_k5_err_msg(context, code);
		printerr(0, "ERROR: %s while storing credentials in '%s'\n",
			 k5err, cc_name);
		goto out;
	}

	code = 0;
	printerr(2, "%s(0x%lx): principal '%s' ccache:'%s'\n", 
		__func__, tid, pname, cc_name);
  out:
#ifdef HAVE_KRB5_GET_INIT_CREDS_OPT_SET_ADDRESSLESS
	if (init_opts)
		krb5_get_init_creds_opt_free(context, init_opts);
#endif
	if (pname)
		k5_free_unparsed_name(context, pname);
	if (ccache)
		krb5_cc_close(context, ccache);
	krb5_free_cred_contents(context, &my_creds);
	free(k5err);
	return (code);
}

/*
 * Given a principal, find a matching ple structure
 * Called with mutex held
 */
static struct gssd_k5_kt_princ *
find_ple_by_princ(krb5_context context, krb5_principal princ)
{
	struct gssd_k5_kt_princ *ple;

	for (ple = gssd_k5_kt_princ_list; ple != NULL; ple = ple->next) {
		if (krb5_principal_compare(context, ple->princ, princ))
			return ple;
	}
	/* no match found */
	return NULL;
}

/*
 * Create, initialize, and add a new ple structure to the global list
 * Called with mutex held
 */
static struct gssd_k5_kt_princ *
new_ple(krb5_context context, krb5_principal princ)
{
	struct gssd_k5_kt_princ *ple = NULL, *p;
	krb5_error_code code;
	char *default_realm;
	int is_default_realm = 0;

	ple = malloc(sizeof(struct gssd_k5_kt_princ));
	if (ple == NULL)
		goto outerr;
	memset(ple, 0, sizeof(*ple));

#ifdef HAVE_KRB5
	ple->realm = strndup(princ->realm.data,
			     princ->realm.length);
#else
	ple->realm = strdup(princ->realm);
#endif
	if (ple->realm == NULL)
		goto outerr;
	code = krb5_copy_principal(context, princ, &ple->princ);
	if (code)
		goto outerr;

	/*
	 * Add new entry onto the list (if this is the default
	 * realm, always add to the front of the list)
	 */

	code = krb5_get_default_realm(context, &default_realm);
	if (code == 0) {
		if (strcmp(ple->realm, default_realm) == 0)
			is_default_realm = 1;
		k5_free_default_realm(context, default_realm);
	}

	if (is_default_realm) {
		ple->next = gssd_k5_kt_princ_list;
		gssd_k5_kt_princ_list = ple;
	} else {
		p = gssd_k5_kt_princ_list;
		while (p != NULL && p->next != NULL)
			p = p->next;
		if (p == NULL)
			gssd_k5_kt_princ_list = ple;
		else
			p->next = ple;
	}

	ple->refcount = 1;
	return ple;
outerr:
	if (ple) {
		if (ple->realm)
			free(ple->realm);
		free(ple);
	}
	return NULL;
}

/*
 * Given a principal, find an existing ple structure, or create one
 */
static struct gssd_k5_kt_princ *
get_ple_by_princ(krb5_context context, krb5_principal princ)
{
	struct gssd_k5_kt_princ *ple;

	pthread_mutex_lock(&ple_lock);
	ple = find_ple_by_princ(context, princ);
	if (ple == NULL) {
		ple = new_ple(context, princ);
	}
	if (ple != NULL) {
		ple->refcount++;
	}
	pthread_mutex_unlock(&ple_lock);

	return ple;
}

/*
 * Given a (possibly unqualified) hostname,
 * return the fully qualified (lower-case!) hostname
 */
static int
get_full_hostname(const char *inhost, char *outhost, int outhostlen)
{
	struct addrinfo *addrs = NULL;
	struct addrinfo hints;
	int retval;
	char *c;
	pthread_t tid = pthread_self();

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = PF_UNSPEC;
	hints.ai_flags = AI_CANONNAME;

	/* Get full target hostname */
	retval = getaddrinfo(inhost, NULL, &hints, &addrs);
	if (retval) {
		printerr(1, "%s(0x%lx): getaddrinfo(%s) failed: %s\n",
			 __func__, tid, inhost, gai_strerror(retval));
		goto out;
	}
	strncpy(outhost, addrs->ai_canonname, outhostlen);
	nfs_freeaddrinfo(addrs);
	for (c = outhost; *c != '\0'; c++)
	    *c = tolower(*c);

	if (get_verbosity() && strcmp(inhost, outhost))
		printerr(1, "%s(0x%0lx): inhost '%s' different than outhost '%s'\n", 
			 __func__, tid, inhost, outhost);

	retval = 0;
out:
	return retval;
}

/* 
 * If principal matches the given realm and service name,
 * and has *any* instance (hostname), return 1.
 * Otherwise return 0, indicating no match.
 */
#ifdef HAVE_KRB5
static int
realm_and_service_match(krb5_principal p, const char *realm, const char *service)
{
	/* Must have two components */
	if (p->length != 2)
		return 0;

	if ((strlen(realm) == p->realm.length)
	    && (strncmp(realm, p->realm.data, p->realm.length) == 0)
	    && (strlen(service) == p->data[0].length)
	    && (strncmp(service, p->data[0].data, p->data[0].length) == 0))
		return 1;

	return 0;
}
#else
static int
realm_and_service_match(krb5_context context, krb5_principal p,
			const char *realm, const char *service)
{
	const char *name, *inst;

	if (p->name.name_string.len != 2)
		return 0;

	name = krb5_principal_get_comp_string(context, p, 0);
	inst = krb5_principal_get_comp_string(context, p, 1);
	if (name == NULL || inst == NULL)
		return 0;
	if ((strcmp(realm, p->realm) == 0)
	    && (strcmp(service, name) == 0))
		return 1;

	return 0;
}
#endif

/*
 * Search the given keytab file looking for an entry with the given
 * service name and realm, ignoring hostname (instance).
 *
 * Returns:
 *	0 => No error
 *	non-zero => An error occurred
 *
 * If a keytab entry is found, "found" is set to one, and the keytab
 * entry is returned in "kte".  Otherwise, "found" is zero, and the
 * value of "kte" is unpredictable.
 */
static int
gssd_search_krb5_keytab(krb5_context context, krb5_keytab kt,
			const char *realm, const char *service,
			int *found, krb5_keytab_entry *kte)
{
	krb5_kt_cursor cursor;
	krb5_error_code code;
	struct gssd_k5_kt_princ *ple;
	int retval = -1, status;
	char kt_name[BUFSIZ];
	char *pname;
	char *k5err = NULL;

	if (found == NULL) {
		retval = EINVAL;
		goto out;
	}
	*found = 0;

	/*
	 * Look through each entry in the keytab file and determine
	 * if we might want to use it as machine credentials.  If so,
	 * save info in the global principal list (gssd_k5_kt_princ_list).
	 */
	if ((code = krb5_kt_get_name(context, kt, kt_name, BUFSIZ))) {
		k5err = gssd_k5_err_msg(context, code);
		printerr(0, "ERROR: %s attempting to get keytab name\n", k5err);
		retval = code;
		goto out;
	}
	if ((code = krb5_kt_start_seq_get(context, kt, &cursor))) {
		k5err = gssd_k5_err_msg(context, code);
		printerr(0, "ERROR: %s while beginning keytab scan "
			    "for keytab '%s'\n", k5err, kt_name);
		retval = code;
		goto out;
	}

	printerr(4, "Scanning keytab for %s/*@%s\n", service, realm);
	while ((code = krb5_kt_next_entry(context, kt, kte, &cursor)) == 0) {
		if ((code = krb5_unparse_name(context, kte->principal,
					      &pname))) {
			k5err = gssd_k5_err_msg(context, code);
			printerr(0, "WARNING: Skipping keytab entry because "
				 "we failed to unparse principal name: %s\n",
				 k5err);
			k5_free_kt_entry(context, kte);
			free(k5err);
			k5err = NULL;
			continue;
		}
		printerr(4, "Processing keytab entry for principal '%s'\n",
			 pname);
		/* Use the first matching keytab entry found */
#ifdef HAVE_KRB5
		status = realm_and_service_match(kte->principal, realm, service);
#else
		status = realm_and_service_match(context, kte->principal, realm, service);
#endif
		if (status) {
			printerr(4, "We WILL use this entry (%s)\n", pname);
			ple = get_ple_by_princ(context, kte->principal);
			/*
			 * Return, don't free, keytab entry if
			 * we were successful!
			 */
			if (ple == NULL) {
				retval = ENOMEM;
				k5_free_kt_entry(context, kte);
			} else {
				release_ple(context, ple);
				ple = NULL;
				retval = 0;
				*found = 1;
			}
			k5_free_unparsed_name(context, pname);
			break;
		}
		else {
			printerr(4, "We will NOT use this entry (%s)\n",
				pname);
		}
		k5_free_unparsed_name(context, pname);
		k5_free_kt_entry(context, kte);
	}

	if ((code = krb5_kt_end_seq_get(context, kt, &cursor))) {
		k5err = gssd_k5_err_msg(context, code);
		printerr(0, "WARNING: %s while ending keytab scan for "
			    "keytab '%s'\n", k5err, kt_name);
	}

	/* Only clear the retval if has not been set */
	if (retval < 0)
		retval = 0;
  out:
	free(k5err);
	return retval;
}

/*
 * Find a keytab entry to use for a given target realm.
 * Tries to find the most appropriate keytab to use given the
 * name of the host we are trying to connect with.
 *
 * Note: the tgtname contains a hostname in the realm that we
 * are authenticating to. It may, or may not be the same as
 * the server hostname.
 */
static int
find_keytab_entry(krb5_context context, krb5_keytab kt,
		  const char *srchost, const char *tgtname,
		  krb5_keytab_entry *kte, const char **svcnames)
{
	krb5_error_code code;
	char **realmnames = NULL;
	char myhostname[NI_MAXHOST], targethostname[NI_MAXHOST];
	char myhostad[NI_MAXHOST+1];
	int i, j, k, retval;
	char *default_realm = NULL;
	char *realm;
	char *k5err = NULL;
	int tried_all = 0, tried_default = 0, tried_upper = 0;
	krb5_principal princ;
	const char *notsetstr = "not set";
	char *adhostoverride = NULL;
	pthread_t tid = pthread_self();


	/* Get full target hostname */
	retval = get_full_hostname(tgtname, targethostname,
				   sizeof(targethostname));
	if (retval)
		goto out;

	/* Get full local hostname */
	if (srchost) {
		strcpy(myhostname, srchost);
	        strcpy(myhostad, myhostname);
	} else {
		/* Borrow myhostad for gethostname(), we need it later anyways */
		if (gethostname(myhostad, sizeof(myhostad)-1) == -1) {
			retval = errno;
			k5err = gssd_k5_err_msg(context, retval);
			printerr(1, "%s while getting local hostname\n", k5err);
			goto out;
		}
		retval = get_full_hostname(myhostad, myhostname, sizeof(myhostname));
		if (retval) {
			/* Don't use myhostname */
			myhostname[0] = 0;
		}
	}

	/* Compute the active directory machine name HOST$ */
	krb5_appdefault_string(context, "nfs", NULL, "ad_principal_name",
		notsetstr, &adhostoverride);
	if (adhostoverride && strcmp(adhostoverride, notsetstr) != 0) {
		printerr(1,
			 "AD host string overridden with \"%s\" from appdefaults\n",
			 adhostoverride);
		/* No overflow: Windows cannot handle strings longer than 19 chars */
		strcpy(myhostad, adhostoverride);
	} else {
		/* In this case, it's been pre-filled above */
		for (i = 0; myhostad[i] != 0; ++i) {
			if (myhostad[i] == '.') break;
		}
		myhostad[i] = '$';
		myhostad[i+1] = 0;
	}
	if (adhostoverride)
		krb5_free_string(context, adhostoverride);

	code = krb5_get_default_realm(context, &default_realm);
	if (code) {
		retval = code;
		k5err = gssd_k5_err_msg(context, code);
		printerr(1, "%s while getting default realm name\n", k5err);
		goto out;
	}

	/*
	 * Get the realm name(s) for the target hostname.
	 * In reality, this function currently only returns a
	 * single realm, but we code with the assumption that
	 * someday it may actually return a list.
	 */
	code = krb5_get_host_realm(context, targethostname, &realmnames);
	if (code) {
		k5err = gssd_k5_err_msg(context, code);
		printerr(0, "ERROR: %s while getting realm(s) for host '%s'\n",
			 k5err, targethostname);
		retval = code;
		goto out;
	}

	/*
	 * Make sure the preferred_realm, which may have been explicitly set
	 * on the command line, is tried first. If nothing is found go on with
	 * the host and local default realm (if that hasn't already been tried).
	 */
	i = 0;
	realm = realmnames[i];

	if (preferred_realm && strcmp (realm, preferred_realm) != 0) {
		realm = preferred_realm;
		/* resetting the realmnames index */
		i = -1;
	}

	while (1) {
		if (realm == NULL) {
			tried_all = 1;
			if (!tried_default)
				realm = default_realm;
		}
		if (tried_all && tried_default)
			break;
		if (strcmp(realm, default_realm) == 0)
			tried_default = 1;
		for (j = 0; svcnames[j] != NULL; j++) {
			char spn[NI_MAXHOST+2];

			/*
			 * The special svcname "$" means 'try the active
			 * directory machine account'
			 */
			if (strcmp(svcnames[j],"$") == 0) {
				snprintf(spn, sizeof(spn), "%s@%s", myhostad, realm);
				code = krb5_build_principal_ext(context, &princ,
								strlen(realm),
								realm,
								strlen(myhostad),
								myhostad,
								NULL);
			} else {
				if (!myhostname[0])
					continue;
				snprintf(spn, sizeof(spn), "%s/%s@%s",
					 svcnames[j], myhostname, realm);
				code = krb5_build_principal_ext(context, &princ,
								strlen(realm),
								realm,
								strlen(svcnames[j]),
								svcnames[j],
								strlen(myhostname),
								myhostname,
								NULL);
			}

			if (code) {
				k5err = gssd_k5_err_msg(context, code);
				printerr(1, "%s while building principal for '%s'\n",
					 k5err, spn);
				free(k5err);
				k5err = NULL;
				continue;
			}
			code = krb5_kt_get_entry(context, kt, princ, 0, 0, kte);
			krb5_free_principal(context, princ);
			if (code) {
				k5err = gssd_k5_err_msg(context, code);
				printerr(3, "%s while getting keytab entry for '%s'\n",
					 k5err, spn);
				free(k5err);
				k5err = NULL;
				/*
				 * We tried the active directory machine account
				 * with the hostname part as-is and failed...
				 * convert it to uppercase and try again before
				 * moving on to the svcname
				 */
				if (strcmp(svcnames[j],"$") == 0 && !tried_upper) {
					for (k = 0; myhostad[k] != '$'; ++k) {
						myhostad[k] = toupper(myhostad[k]);
					}
					j--;
					tried_upper = 1;
				}
			} else {
				printerr(2, "find_keytab_entry(0x%lx): Success getting keytab entry for '%s'\n",tid, spn);
				retval = 0;
				goto out;
			}
			retval = code;
		}
		/*
		 * Nothing found with our hostname instance, now look for
		 * names with any instance (they must have an instance)
		 */
		for (j = 0; svcnames[j] != NULL; j++) {
			int found = 0;
			if (strcmp(svcnames[j],"$") == 0)
				continue;
			code = gssd_search_krb5_keytab(context, kt, realm,
						       svcnames[j], &found, kte);
			if (!code && found) {
				printerr(3, "Success getting keytab entry for "
					 "%s/*@%s\n", svcnames[j], realm);
				retval = 0;
				goto out;
			}
		}
		if (!tried_all) {
			i++;
			realm = realmnames[i];
		}
	}
out:
	if (default_realm)
		k5_free_default_realm(context, default_realm);
	if (realmnames)
		krb5_free_host_realm(context, realmnames);
	free(k5err);
	return retval;
}


static inline int data_is_equal(krb5_data d1, krb5_data d2)
{
	return (d1.length == d2.length
		&& memcmp(d1.data, d2.data, d1.length) == 0);
}

static int
check_for_tgt(krb5_context context, krb5_ccache ccache,
	      krb5_principal principal)
{
	krb5_error_code ret;
	krb5_creds creds;
	krb5_cc_cursor cur;
	int found = 0;

	ret = krb5_cc_start_seq_get(context, ccache, &cur);
	if (ret) 
		return 0;

	while (!found &&
		(ret = krb5_cc_next_cred(context, ccache, &cur, &creds)) == 0) {
		if (creds.server->length == 2 &&
				data_is_equal(creds.server->realm,
					      principal->realm) &&
				creds.server->data[0].length == 6 &&
				memcmp(creds.server->data[0].data,
						"krbtgt", 6) == 0 &&
				data_is_equal(creds.server->data[1],
					      principal->realm) &&
				creds.times.endtime > time(NULL))
			found = 1;
		krb5_free_cred_contents(context, &creds);
	}
	krb5_cc_end_seq_get(context, ccache, &cur);

	return found;
}

static int
query_krb5_ccache(const char* cred_cache, char **ret_princname,
		  char **ret_realm)
{
	krb5_error_code ret;
	krb5_context context;
	krb5_ccache ccache;
	krb5_principal principal;
	int found = 0;
	char *str = NULL;
	char *princstring;

	*ret_princname = *ret_realm = NULL;

	ret = krb5_init_context(&context);
	if (ret) 
		return 0;

	if(!cred_cache || krb5_cc_resolve(context, cred_cache, &ccache))
		goto err_cache;

	if (krb5_cc_set_flags(context, ccache, 0))
		goto err_princ;

	ret = krb5_cc_get_principal(context, ccache, &principal);
	if (ret) 
		goto err_princ;

	found = check_for_tgt(context, ccache, principal);
	if (found) {
		ret = krb5_unparse_name(context, principal, &princstring);
		if (ret == 0) {
		    if ((str = strchr(princstring, '@')) != NULL) {
			    *str = '\0';
			    *ret_princname = strdup(princstring);
			    *ret_realm = strdup(str+1);
			    if (!*ret_princname || !*ret_realm) {
				    free(*ret_princname);
				    free(*ret_realm);
				    *ret_princname = NULL;
				    *ret_realm = NULL;
			    }
		    }
		    k5_free_unparsed_name(context, princstring);
		}
	}
	krb5_free_principal(context, principal);
err_princ:
	krb5_cc_set_flags(context, ccache,  KRB5_TC_OPENCLOSE);
	krb5_cc_close(context, ccache);
err_cache:
	krb5_free_context(context);
	return (*ret_princname && *ret_realm);
}

/*
 * Obtain (or refresh if necessary) Kerberos machine credentials
 * If a ple is passed in, it's reference will be released
 */
static int
gssd_refresh_krb5_machine_credential_internal(char *hostname,
				     struct gssd_k5_kt_princ *ple,
				     char *service, char *srchost)
{
	krb5_error_code code = 0;
	krb5_context context;
	krb5_keytab kt = NULL;;
	int retval = 0;
	char *k5err = NULL;
	const char *svcnames[] = { "$", "root", "nfs", "host", NULL };

	/*
	 * If a specific service name was specified, use it.
	 * Otherwise, use the default list.
	 */
	if (service != NULL && strcmp(service, "*") != 0) {
		svcnames[0] = service;
		svcnames[1] = NULL;
	}
	if (hostname == NULL && ple == NULL) {
		printerr(0, "ERROR: %s: Invalid args\n", __func__);
		return EINVAL;
	}
	code = krb5_init_context(&context);
	if (code) {
		k5err = gssd_k5_err_msg(NULL, code);
		printerr(0, "ERROR: %s: %s while initializing krb5 context\n",
			 __func__, k5err);
		retval = code;
		goto out;
	}

	if ((code = krb5_kt_resolve(context, keytabfile, &kt))) {
		k5err = gssd_k5_err_msg(context, code);
		printerr(0, "ERROR: %s: %s while resolving keytab '%s'\n",
			 __func__, k5err, keytabfile);
		goto out_free_context;
	}

	if (ple == NULL) {
		krb5_keytab_entry kte;

		code = find_keytab_entry(context, kt, srchost, hostname,
					 &kte, svcnames);
		if (code) {
			printerr(0, "ERROR: %s: no usable keytab entry found "
				 "in keytab %s for connection with host %s\n",
				 __FUNCTION__, keytabfile, hostname);
			retval = code;
			goto out_free_kt;
		}

		ple = get_ple_by_princ(context, kte.principal);
		k5_free_kt_entry(context, &kte);
		if (ple == NULL) {
			char *pname;
			if ((krb5_unparse_name(context, kte.principal, &pname))) {
				pname = NULL;
			}
			printerr(0, "ERROR: %s: Could not locate or create "
				 "ple struct for principal %s for connection "
				 "with host %s\n",
				 __FUNCTION__, pname ? pname : "<unparsable>",
				 hostname);
			if (pname) k5_free_unparsed_name(context, pname);
			goto out_free_kt;
		}
	}
	retval = gssd_get_single_krb5_cred(context, kt, ple);
out_free_kt:
	krb5_kt_close(context, kt);
out_free_context:
	if (ple)
		release_ple(context, ple);
	krb5_free_context(context);
out:
	free(k5err);
	return retval;
}

/*==========================*/
/*===  External routines ===*/
/*==========================*/

/*
 * Attempt to find the best match for a credentials cache file
 * given only a UID.  We really need more information, but we
 * do the best we can.
 *
 * Returns 0 if a ccache was found, or a negative errno otherwise.
 */
int
gssd_setup_krb5_user_gss_ccache(uid_t uid, char *servername, char *dirpattern)
{
				/* dirname + cctype + d_name + NULL */
	char			buf[PATH_MAX+5+256+1], dirname[PATH_MAX];
	const char		*cctype;
	struct dirent		*d;
	int			err, i, j;
	u_int			maj_stat, min_stat;

	printerr(3, "looking for client creds with uid %u for "
		    "server %s in %s\n", uid, servername, dirpattern);

	for (i = 0, j = 0; dirpattern[i] != '\0'; i++) {
		switch (dirpattern[i]) {
		case '%':
			switch (dirpattern[i + 1]) {
			case '%':
				dirname[j++] = dirpattern[i];
				i++;
				break;
			case 'U':
				j += sprintf(dirname + j, "%lu",
					     (unsigned long) uid);
				i++;
				break;
			}
			break;
		default:
			dirname[j++] = dirpattern[i];
			break;
		}
	}
	dirname[j] = '\0';

	err = gssd_find_existing_krb5_ccache(uid, dirname, &cctype, &d);
	if (err)
		return err;

	snprintf(buf, sizeof(buf), "%s:%s/%s", cctype, dirname, d->d_name);
	free(d);

	printerr(2, "using %s as credentials cache for client with "
		    "uid %u for server %s\n", buf, uid, servername);

	printerr(3, "using gss_krb5_ccache_name to select krb5 ccache %s\n",
		 buf);
	maj_stat = gss_krb5_ccache_name(&min_stat, buf, NULL);
	if (maj_stat != GSS_S_COMPLETE) {
		printerr(0, "ERROR: unable to get user cred cache '%s' "
			 "failed (%s)\n", buf, error_message(min_stat));
		return maj_stat;
	}
	return 0;
}

/*
 * Return an array of pointers to names of credential cache files
 * which can be used to try to create gss contexts with a server.
 *
 * Returns:
 *	0 => list is attached
 *	nonzero => error
 */
int
gssd_get_krb5_machine_cred_list(char ***list)
{
	char **l;
	int listinc = 10;
	int listsize = listinc;
	int i = 0;
	int retval;
	struct gssd_k5_kt_princ *ple;

	/* Assume failure */
	retval = -1;
	*list = (char **) NULL;

	if ((l = (char **) malloc(listsize * sizeof(char *))) == NULL) {
		retval = ENOMEM;
		goto out;
	}

	pthread_mutex_lock(&ple_lock);
	for (ple = gssd_k5_kt_princ_list; ple; ple = ple->next) {
		if (!ple->ccname)
			continue;

		/* Take advantage of the fact we only remove the ple
		 * from the list during shutdown. If it's modified
		 * concurrently at worst we'll just miss a new entry
		 * before the current ple
		 *
		 * gssd_refresh_krb5_machine_credential_internal() will
		 * release the ple refcount
		 */
		ple->refcount++;
		pthread_mutex_unlock(&ple_lock);
		/* Make sure cred is up-to-date before returning it */
		retval = gssd_refresh_krb5_machine_credential_internal(NULL, ple,
								       NULL, NULL);
		pthread_mutex_lock(&ple_lock);
		if (gssd_k5_kt_princ_list == NULL) {
			/* Looks like we did shutdown... abort */
			l[i] = NULL;
			gssd_free_krb5_machine_cred_list(l);
			retval = ENOMEM;
			goto out_lock;
		}
		if (retval)
			continue;
		if (i + 1 > listsize) {
			char **tmplist;
			listsize += listinc;
			tmplist = (char **)
				realloc(l, listsize * sizeof(char *));
			if (tmplist == NULL) {
				gssd_free_krb5_machine_cred_list(l);
				retval = ENOMEM;
				goto out_lock;
			}
			l = tmplist;
		}
		if ((l[i++] = strdup(ple->ccname)) == NULL) {
			gssd_free_krb5_machine_cred_list(l);
			retval = ENOMEM;
			goto out_lock;
		}
	}
	if (i > 0) {
		l[i] = NULL;
		*list = l;
		retval = 0;
	} else
		free((void *)l);
out_lock:
	pthread_mutex_unlock(&ple_lock);
  out:
	return retval;
}

/*
 * Frees the list of names returned in get_krb5_machine_cred_list()
 */
void
gssd_free_krb5_machine_cred_list(char **list)
{
	char **n;

	if (list == NULL)
		return;
	for (n = list; n && *n; n++) {
		free(*n);
	}
	free(list);
}

/*
 * Called upon exit.  Destroys machine credentials.
 */
void
gssd_destroy_krb5_principals(int destroy_machine_creds)
{
	krb5_context context;
	krb5_error_code code = 0;
	krb5_ccache ccache;
	struct gssd_k5_kt_princ *ple;
	char *k5err = NULL;

	code = krb5_init_context(&context);
	if (code) {
		k5err = gssd_k5_err_msg(NULL, code);
		printerr(0, "ERROR: %s while initializing krb5\n", k5err);
		free(k5err);
		return;
	}

	pthread_mutex_lock(&ple_lock);
	while (gssd_k5_kt_princ_list) {
		ple = gssd_k5_kt_princ_list;
		gssd_k5_kt_princ_list = ple->next;

		if (destroy_machine_creds && ple->ccname) {
			if ((code = krb5_cc_resolve(context, ple->ccname, &ccache))) {
				k5err = gssd_k5_err_msg(context, code);
				printerr(0, "WARNING: %s while resolving credential "
					    "cache '%s' for destruction\n", k5err,
					    ple->ccname);
				free(k5err);
				k5err = NULL;
			}

			if (!code && (code = krb5_cc_destroy(context, ccache))) {
				k5err = gssd_k5_err_msg(context, code);
				printerr(0, "WARNING: %s while destroying credential "
					    "cache '%s'\n", k5err, ple->ccname);
				free(k5err);
				k5err = NULL;
			}
		}

		release_ple_locked(context, ple);
	}
	pthread_mutex_unlock(&ple_lock);
	krb5_free_context(context);
}

/*
 * Obtain (or refresh if necessary) Kerberos machine credentials
 */
int
gssd_refresh_krb5_machine_credential(char *hostname,
				     char *service, char *srchost)
{
    return gssd_refresh_krb5_machine_credential_internal(hostname, NULL,
							 service, srchost);
}

/*
 * A common routine for getting the Kerberos error message
 */
char *
gssd_k5_err_msg(krb5_context context, krb5_error_code code)
{
	const char *origmsg;
	char *msg = NULL;

#if HAVE_KRB5_GET_ERROR_MESSAGE
	if (context != NULL) {
		origmsg = krb5_get_error_message(context, code);
		msg = strdup(origmsg);
		krb5_free_error_message(context, origmsg);
	}
#endif
	if (msg != NULL)
		return msg;
#if HAVE_KRB5
	return strdup(error_message(code));
#else
	if (context != NULL)
		return strdup(krb5_get_err_text(context, code));
	else
		return strdup(error_message(code));
#endif
}

/*
 * Return default Kerberos realm
 */
void
gssd_k5_get_default_realm(char **def_realm)
{
	krb5_context context;

	if (krb5_init_context(&context))
		return;

	krb5_get_default_realm(context, def_realm);

	krb5_free_context(context);
}

static int
gssd_acquire_krb5_cred(gss_cred_id_t *gss_cred)
{
	OM_uint32 maj_stat, min_stat;
	gss_OID_set_desc desired_mechs = { 1, &krb5oid };

	maj_stat = gss_acquire_cred(&min_stat, GSS_C_NO_NAME, GSS_C_INDEFINITE,
				    &desired_mechs, GSS_C_INITIATE,
				    gss_cred, NULL, NULL);

	if (maj_stat != GSS_S_COMPLETE) {
		if (get_verbosity() > 0)
			pgsserr("gss_acquire_cred",
				maj_stat, min_stat, &krb5oid);
		return -1;
	}

	return 0;
}

int
gssd_acquire_user_cred(gss_cred_id_t *gss_cred)
{
	OM_uint32 maj_stat, min_stat;
	int ret;

	ret = gssd_acquire_krb5_cred(gss_cred);
	if (ret)
		return ret;

	/* force validation of cred to check for expiry */
	maj_stat = gss_inquire_cred(&min_stat, *gss_cred,
			NULL, NULL, NULL, NULL);
	if (maj_stat != GSS_S_COMPLETE) {
		if (get_verbosity() > 0)
			pgsserr("gss_inquire_cred",
				maj_stat, min_stat, &krb5oid);
		ret = -1;
	}

	return ret;
}

#ifdef HAVE_SET_ALLOWABLE_ENCTYPES
/*
 * this routine obtains a credentials handle via gss_acquire_cred()
 * then calls gss_krb5_set_allowable_enctypes() to limit the encryption
 * types negotiated.
 *
 * XXX Should call some function to determine the enctypes supported
 * by the kernel. (Only need to do that once!)
 *
 * Returns:
 *	0 => all went well
 *     -1 => there was an error
 */

int
limit_krb5_enctypes(struct rpc_gss_sec *sec)
{
	u_int maj_stat, min_stat;
	krb5_enctype enctypes[] = { ENCTYPE_DES_CBC_CRC,
				    ENCTYPE_DES_CBC_MD5,
				    ENCTYPE_DES_CBC_MD4 };
	int num_enctypes = sizeof(enctypes) / sizeof(enctypes[0]);
	extern int num_krb5_enctypes;
	extern krb5_enctype *krb5_enctypes;
	int err = -1;

	if (sec->cred == GSS_C_NO_CREDENTIAL) {
		err = gssd_acquire_krb5_cred(&sec->cred);
		if (err)
			return -1;
	}

	/*
	 * If we failed for any reason to produce global
	 * list of supported enctypes, use local default here.
	 */
	if (krb5_enctypes == NULL || limit_to_legacy_enctypes)
		maj_stat = gss_set_allowable_enctypes(&min_stat, sec->cred,
					&krb5oid, num_enctypes, enctypes);
	else
		maj_stat = gss_set_allowable_enctypes(&min_stat, sec->cred,
					&krb5oid, num_krb5_enctypes, krb5_enctypes);

	if (maj_stat != GSS_S_COMPLETE) {
		pgsserr("gss_set_allowable_enctypes",
			maj_stat, min_stat, &krb5oid);
		return -1;
	}

	return 0;
}
#endif	/* HAVE_SET_ALLOWABLE_ENCTYPES */
