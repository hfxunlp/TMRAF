/*
 *  nfsidmap_common.c
 *
 *  nfs idmapping library, primarily for nfs4 client/server kernel idmapping
 *  and for userland nfs4 idmapping by acl libraries.
 *
 *  Code common to libnfsidmap and some of its bundled plugins
 *
 */

#include "config.h"

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include "nfsidmap.h"
#include "nfsidmap_private.h"
#include "nfsidmap_plugin.h"
#include "conffile.h"

#pragma GCC visibility push(hidden)

int reformat_group = 0;
int no_strip = 0;

struct conf_list *local_realms;

struct conf_list *get_local_realms(void)
{
	return local_realms;
}

