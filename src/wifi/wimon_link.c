#include <bits/errno.h>
#include <string.h>
#include <format.h>

#include "wimon.h"

struct uplink uplink;

static int ignored(struct link* ls, int off)
{
	switch(ls->mode) {
		case LM_FREE:
		case LM_NOT:
			return 1;
		case LM_OFF:
			return off;
		default:
			return 0;
	}
}

void set_link_mode(struct link* ls, int mode)
{
	if(ls->mode == mode)
		return;

	ls->mode = mode;

	if(ignored(ls, 1))
		save_link(ls);
	else
		ls->flags |= S_UNSAVED;
}

void link_new(struct link* ls)
{
	int ifi = ls->ifi;

	load_link(ls);

	if(ignored(ls, 0))
		return;

	if(ls->mode == LM_OFF) {
		if(ls->flags & S_ENABLED)
			disable_iface(ifi);
	} else {
		if(ls->flags & S_CARRIER)
			link_carrier(ls);
		else if(ls->flags & S_ENABLED)
			link_enabled(ls);
		else
			enable_iface(ifi);
	}
}

void link_wifi(struct link* ls)
{
	if(ignored(ls, 1))
		return;
	if(ls->flags & S_ENABLED)
		wifi_ready(ls);
}

void link_enabled(struct link* ls)
{
	if(ignored(ls, 1))
		return;
	if(ls->flags & S_NL80211)
		wifi_ready(ls);
}

void link_carrier(struct link* ls)
{
	if(ignored(ls, 1))
		return;
	if(ls->mode == LM_STATIC)
		; /* XXX */
	else if(ls->mode == LM_LOCAL)
		spawn_dhcp(ls, "-g");
	else if(ls->mode == LM_DHCP)
		spawn_dhcp(ls, NULL);
}

void link_ipaddr(struct link* ls)
{
	ls->state = LS_ACTIVE;

	unlatch(ls->ifi, CONF, 0);

	if(ls->flags & S_UNSAVED) {
		save_link(ls);
		ls->flags &= ~S_UNSAVED;
	}
	if(ls->flags & S_NL80211)
		wifi_connected(ls);
}

int any_stopping_links(void)
{
	struct link* ls;

	for(ls = links; ls < links + nlinks; ls++)
		if(ls->state == LS_STOPPING)
			return 1;

	return 0;
}

void link_terminated(struct link* ls)
{
	if(ls->flags & S_NL80211)
		wifi_conn_fail(ls);
}

void recheck_alldown_latches(void)
{
	struct link* ls;

	for(ls = links; ls < links + nlinks; ls++)
		if(ls->state == LS_STOPPING)
			return;

	unlatch(NONE, DOWN, 0);
}

/* Whenever a link goes down, for any reason, all its remaining procs must
   be stopped and its ip configuration must be flushed. Not doing this means
   the link may be left in mid-way state, confusing the usespace. */

static void wait_link_down(struct link* ls)
{
	if(ls->flags & S_CHILDREN)
		return stop_link_procs(ls, 0);
	if(ls->flags & S_IPADDR)
		return del_link_addresses(ls->ifi);

	if(ls->flags & S_ENABLED && ls->mode == LM_OFF)
		disable_iface(ls->ifi);

	ls->state = LS_DOWN;

	unlatch(ls->ifi, DOWN, 0);
	recheck_alldown_latches();

	link_terminated(ls);
}

void terminate_link(struct link* ls)
{
	if(ls->state == LS_DOWN)
		return;
	if(ls->state == LS_STOPPING)
		return;

	ls->state = LS_STOPPING;
	cancel_scheduled(ls->ifi);
	unlatch(ls->ifi, CONF, -EINTR);

	wait_link_down(ls);
}

/* Outside of link-stopping procedure, the only remotely sane case
   for this to happen is expiration of DHCP-set address. If so, try
   to re-run dhcp. And that's about it really. */

void link_ipgone(struct link* ls)
{
	if(ls->state == LS_STOPPING)
		return wait_link_down(ls);
}

void link_down(struct link* ls)
{
	if(ls->state == LS_STOPPING)
		wait_link_down(ls);
	else
		terminate_link(ls);
}

void link_child_exit(struct link* ls, int status)
{
	if(ls->state == LS_STOPPING)
		wait_link_down(ls);
	else if(status)
		terminate_link(ls);
	/* else it's ok */
}

void link_gone(struct link* ls)
{
	stop_link_procs(ls, 1);

	cancel_scheduled(ls->ifi);
	unlatch(ls->ifi, ANY, -ENODEV);
	recheck_alldown_latches();

	if(ls->flags & S_NL80211)
		wifi_gone(ls);
}

/* When wimon exits, remove addresses from managed links.
   This is primarily meant for wireless links but makes some sense
   for ethernet ports as well. */

void finalize_links(void)
{
	struct link* ls;

	for(ls = links; ls < links + nlinks; ls++) {
		if(!ls->ifi || ls->mode == LM_NOT)
			continue;
		if(ls->flags & S_IPADDR)
			del_link_addresses(ls->ifi);
		/* del_link_routes */
	}
}

int stop_all_links(void)
{
	struct link* ls;
	int down = 0;

	for(ls = links; ls < links + nlinks; ls++) {
		if(!ls->ifi)
			continue;
		if(ignored(ls, 1))
			continue;
		if(!(ls->flags & (S_CHILDREN | S_IPADDR)))
			continue;

		terminate_link(ls);

		if(ls->state == LS_STOPPING)
			down++;
	}

	return down;
}

void stop_links_except(int ifi)
{
	struct link* ls;

	for(ls = links; ls < links + nlinks; ls++) {
		if(ls->ifi == ifi)
			continue;
		else if(ignored(ls, 1))
			continue;
		else if(ls->state == LS_DOWN)
			continue;
		else if(ls->state == LS_STOPPING)
			continue;

		terminate_link(ls);
		set_link_mode(ls, LM_OFF);
	}
}

static void check_wired_link(int ifi)
{
	struct link* ls;

	if(!(ls = find_link_slot(ifi)))
		return;

	if(ls->state != LS_STARTING)
		return;

	unlatch(ls->ifi, CONF, -ENETDOWN);
	disable_iface(ls->ifi);

	ls->state = LS_DOWN;
}

int start_wired_link(struct link* ls)
{
	if(ls->state == LS_ACTIVE)
		return 0;
	if(ls->state == LS_STARTING)
		terminate_link(ls);
	if(ls->state == LS_STOPPING)
		return -EAGAIN;

	ls->state = LS_STARTING;

	if(!(ls->flags & S_ENABLED)) {
		enable_iface(ls->ifi);
		schedule(3, check_wired_link, ls->ifi);
		return 0;
	} else if(!(ls->flags & S_CARRIER)) {
		set_link_mode(ls, LM_OFF);
		return -ENETDOWN;
	} else {
		link_carrier(ls);
		return 0;
	}
}
