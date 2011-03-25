/* ChanServ functions.
 *
 * (C) 2003-2011 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

/*************************************************************************/

#include "services.h"
#include "modules.h"

registered_channel_map RegisteredChannelList;

static int def_levels[][2] = {
	{ CA_AUTOOP,		5 },
	{ CA_AUTOVOICE,		3 },
	{ CA_NOJOIN,		-2 },
	{ CA_INVITE,		5 },
	{ CA_AKICK,			10 },
	{ CA_SET,			ACCESS_QOP },
	{ CA_UNBAN,			5 },
	{ CA_OPDEOP,		5 },
	{ CA_ACCESS_LIST,	1 },
	{ CA_ACCESS_CHANGE,	10 },
	{ CA_MEMO,			10 },
	{ CA_ASSIGN,		ACCESS_FOUNDER },
	{ CA_BADWORDS,		10 },
	{ CA_NOKICK,		1 },
	{ CA_FANTASIA,		3 },
	{ CA_SAY,			5 },
	{ CA_GREET,			5 },
	{ CA_VOICEME,		3 },
	{ CA_VOICE,			5 },
	{ CA_GETKEY,		5 },
	{ CA_AUTOHALFOP,	4 },
	{ CA_AUTOPROTECT,	10 },
	{ CA_OPDEOPME,		5 },
	{ CA_HALFOPME,		4 },
	{ CA_HALFOP,		5 },
	{ CA_PROTECTME,		10 },
	{ CA_PROTECT,		ACCESS_QOP },
	{ CA_KICKME,		5 },
	{ CA_KICK,			5 },
	{ CA_SIGNKICK,		ACCESS_FOUNDER },
	{ CA_BANME,			5 },
	{ CA_BAN,			5 },
	{ CA_TOPIC,			ACCESS_FOUNDER },
	{ CA_MODE,			ACCESS_FOUNDER },
	{ CA_INFO,			ACCESS_QOP },
	{ CA_AUTOOWNER,		ACCESS_QOP },
	{ CA_OWNER,			ACCESS_FOUNDER },
	{ CA_OWNERME,		ACCESS_QOP },
	{ CA_FOUNDER,		ACCESS_QOP },
	{ -1 }
};

LevelInfo levelinfo[] = {
	{ CA_AUTOHALFOP,	"AUTOHALFOP",		_("Automatic mode +h") },
	{ CA_AUTOOP,		"AUTOOP",		_("Automatic channel operator status") },
	{ CA_AUTOPROTECT,	"AUTOPROTECT",		_("Automatic mode +a") },
	{ CA_AUTOVOICE,		"AUTOVOICE",		_("Automatic mode +v") },
	{ CA_NOJOIN,		"NOJOIN",		_("Not allowed to join channel") },
	{ CA_SIGNKICK,		"SIGNKICK",		_("No signed kick when SIGNKICK LEVEL is used") },
	{ CA_ACCESS_LIST,	"ACC-LIST",		_("Allowed to view the access list") },
	{ CA_ACCESS_CHANGE,	"ACC-CHANGE",		_("Allowed to modify the access list") },
	{ CA_AKICK,		"AKICK",		_("Allowed to use AKICK command") },
	{ CA_SET,		"SET",			_("Allowed to use SET command (not FOUNDER/PASSWORD)"), },
	{ CA_BAN,		"BAN",			_("Allowed to use BAN command") },
	{ CA_BANME,		"BANME",		_("Allowed to ban him/herself") },
	{ CA_GETKEY,		"GETKEY",		_("Allowed to use GETKEY command") },
	{ CA_HALFOP,		"HALFOP",		_("Allowed to (de)halfop him/herself") },
	{ CA_HALFOPME,		"HALFOPME",		_("Allowed to (de)halfop him/herself") },
	{ CA_INFO,		"INFO",			_("Allowed to use INFO command with ALL option") },
	{ CA_KICK,		"KICK",			_("Allowed to use KICK command") },
	{ CA_KICKME,		"KICKME",		_("Allowed to kick him/herself") },
	{ CA_INVITE,		"INVITE",		_("Allowed to use INVITE command") },
	{ CA_OPDEOP,		"OPDEOP",		_("Allowed to use OP/DEOP commands") },
	{ CA_OPDEOPME,		"OPDEOPME",		_("Allowed to (de)op him/herself") },
	{ CA_PROTECT,		"PROTECT",		_("Allowed to use PROTECT/DEPROTECT commands") },
	{ CA_PROTECTME,		"PROTECTME",		_("Allowed to (de)protect him/herself"), },
	{ CA_TOPIC,		"TOPIC",		_("Allowed to use TOPIC command") },
	{ CA_MODE,		"MODE",			_("Allowed to use MODE command") },
	{ CA_UNBAN,		"UNBAN",		_("Allowed to use UNBAN command") },
	{ CA_VOICE,		"VOICE",		_("Allowed to use VOICE/DEVOICE commands") },
	{ CA_VOICEME,		"VOICEME",		_("Allowed to (de)voice him/herself") },
	{ CA_MEMO,		"MEMO",			_("Allowed to list/read channel memos") },
	{ CA_ASSIGN,		"ASSIGN",		_("Allowed to assign/unassign a bot") },
	{ CA_BADWORDS,		"BADWORDS",		_("Allowed to use BADWORDS command") },
	{ CA_FANTASIA,		"FANTASIA",		_("Allowed to use fantaisist commands") },
	{ CA_GREET,		"GREET",		_("Greet message displayed") },
	{ CA_NOKICK,		"NOKICK",		_("Never kicked by the bot's kickers") },
	{ CA_SAY,		"SAY",			_("Allowed to use SAY and ACT commands") },
	{ CA_AUTOOWNER,		"AUTOOWNER",		_("Automatic mode +q") },
	{ CA_OWNER,		"OWNER",		_("Allowed to use OWNER command") },
	{ CA_OWNERME,		"OWNERME",		_("Allowed to (de)owner him/herself") },
	{ CA_FOUNDER,		"FOUNDER",		_("Allowed to issue commands restricted to channel founders") },
	{ -1 }
};
int levelinfo_maxwidth = 0;

/*************************************************************************/

/*  Returns modes for mlock in a nice way. */

Anope::string get_mlock_modes(ChannelInfo *ci, int complete)
{
	if (!ci)
		return "";

	Anope::string pos = "+", neg = "-", params;

	for (std::multimap<ChannelModeName, ModeLock>::const_iterator it = ci->GetMLock().begin(), it_end = ci->GetMLock().end(); it != it_end; ++it)
	{
		const ModeLock &ml = it->second;
		ChannelMode *cm = ModeManager::FindChannelModeByName(ml.name);
		if (!cm || cm->Type == MODE_LIST || cm->Type == MODE_STATUS)
			continue;

		if (ml.set)
			pos += cm->ModeChar;
		else
			neg += cm->ModeChar;

		if (complete && !ml.param.empty() && cm->Type == MODE_PARAM)
			params += " " + ml.param;
	}

	if (pos.length() == 1)
		pos.clear();
	if (neg.length() == 1)
		neg.clear();

	return pos + neg + params;
}

/*************************************************************************/

/* Return information on memory use.  Assumes pointers are valid. */

void get_chanserv_stats(long *nrec, long *memuse)
{
	long count = 0, mem = 0;
	ModeLock *ml;

	for (registered_channel_map::const_iterator it = RegisteredChannelList.begin(), it_end = RegisteredChannelList.end(); it != it_end; ++it)
	{
		ChannelInfo *ci = it->second;

		++count;
		mem += sizeof(*ci);
		if (!ci->desc.empty())
			mem += ci->desc.length() + 1;
		mem += ci->GetAccessCount() * sizeof(ChanAccess);
		mem += ci->GetAkickCount() * sizeof(AutoKick);

		ml = ci->GetMLock(CMODE_KEY);
		if (ml && !ml->param.empty())
			mem += ml->param.length() + 1;

		ml = ci->GetMLock(CMODE_FLOOD);
		if (ml && !ml->param.empty())
			mem += ml->param.length() + 1;

		ml = ci->GetMLock(CMODE_REDIRECT);
		if (ml && !ml->param.empty())
			mem += ml->param.length() + 1;

		if (!ci->last_topic.empty())
			mem += ci->last_topic.length() + 1;
		if (!ci->forbidby.empty())
			mem += ci->forbidby.length() + 1;
		if (!ci->forbidreason.empty())
			mem += ci->forbidreason.length() + 1;
		if (ci->levels)
			mem += sizeof(*ci->levels) * CA_SIZE;
		unsigned memos = ci->memos.memos.size();
		mem += memos * sizeof(Memo);
		for (unsigned j = 0; j < memos; ++j)
			if (!ci->memos.memos[j]->text.empty())
				mem += ci->memos.memos[j]->text.length() + 1;
		if (ci->ttb)
			mem += sizeof(*ci->ttb) * TTB_SIZE;
		mem += ci->GetBadWordCount() * sizeof(BadWord);
	}
	*nrec = count;
	*memuse = mem;
}

/*************************************************************************/
/*************************************************************************/

/* ChanServ initialization. */

void cs_init()
{
	if (!Config->s_ChanServ.empty())
		ModuleManager::LoadModuleList(Config->ChanServCoreModules);
}

/*************************************************************************/

/* Check the current modes on a channel; if they conflict with a mode lock,
 * fix them.
 */

void check_modes(Channel *c)
{
	if (!c)
	{
		Log() << "check_modes called with NULL values";
		return;
	}

	if (c->bouncy_modes)
		return;

	/* Check for mode bouncing */
	if (c->server_modecount >= 3 && c->chanserv_modecount >= 3)
	{
		ircdproto->SendGlobops(NULL, "Warning: unable to set modes on channel %s. Are your servers' U:lines configured correctly?", c->name.c_str());
		Log() << "Bouncy modes on channel " << c->name;
		c->bouncy_modes = 1;
		return;
	}

	if (c->chanserv_modetime != Anope::CurTime)
	{
		c->chanserv_modecount = 0;
		c->chanserv_modetime = Anope::CurTime;
	}
	c->chanserv_modecount++;

	/* Check if the channel is registered; if not remove mode -r */
	ChannelInfo *ci = c->ci;
	if (!ci)
	{
		if (c->HasMode(CMODE_REGISTERED))
			c->RemoveMode(NULL, CMODE_REGISTERED);
		return;
	}

	for (std::multimap<ChannelModeName, ModeLock>::const_iterator it = ci->GetMLock().begin(), it_end = ci->GetMLock().end(); it != it_end; ++it)
	{
		const ModeLock &ml = it->second;
		ChannelMode *cm = ModeManager::FindChannelModeByName(ml.name);
		if (!cm)
			continue;

		if (cm->Type == MODE_REGULAR)
		{
			if (!c->HasMode(cm->Name) && ml.set)
				c->SetMode(NULL, cm);
			else if (c->HasMode(cm->Name) && !ml.set)
				c->RemoveMode(NULL, cm);
		}
		else if (cm->Type == MODE_PARAM)
		{
			Anope::string param;
			c->GetParam(cm->Name, param);

			/* If the channel doesnt have the mode, or it does and it isn't set correctly */
			if (ml.set)
			{
				if (!c->HasMode(cm->Name) || (!param.empty() && !ml.param.empty() && !param.equals_cs(ml.param)))
					c->SetMode(NULL, cm, ml.param);
			}
			else
			{
				if (c->HasMode(cm->Name))
					c->RemoveMode(NULL, cm);
			}

		}
		else if (cm->Type == MODE_LIST)
		{
			if (ml.set)
				c->SetMode(NULL, cm, ml.param);
			else
				c->RemoveMode(NULL, cm, ml.param);
		}
	}
}

/*************************************************************************/

int check_valid_admin(User *user, Channel *chan, int servermode)
{
	ChannelMode *cm;

	if (!chan || !chan->ci)
		return 1;

	if (!(cm = ModeManager::FindChannelModeByName(CMODE_PROTECT)))
		return 0;

	/* They will be kicked; no need to deop, no need to update our internal struct too */
	if (chan->ci->HasFlag(CI_FORBIDDEN))
		return 0;

	if (servermode && !check_access(user, chan->ci, CA_AUTOPROTECT))
	{
		user->SendMessage(ChanServ, _("This channel has been registered with %s."), Config->s_ChanServ.c_str());
		chan->RemoveMode(NULL, CMODE_PROTECT, user->nick);
		return 0;
	}

	return 1;
}

/*************************************************************************/

/* Check whether a user is allowed to be opped on a channel; if they
 * aren't, deop them.  If serverop is 1, the +o was done by a server.
 * Return 1 if the user is allowed to be opped, 0 otherwise. */

int check_valid_op(User *user, Channel *chan, int servermode)
{
	ChannelMode *owner, *protect, *halfop;
	if (!chan || !chan->ci)
		return 1;

	/* They will be kicked; no need to deop, no need to update our internal struct too */
	if (chan->ci->HasFlag(CI_FORBIDDEN))
		return 0;

	owner = ModeManager::FindChannelModeByName(CMODE_OWNER);
	protect = ModeManager::FindChannelModeByName(CMODE_PROTECT);
	halfop = ModeManager::FindChannelModeByName(CMODE_HALFOP);

	if (servermode && !check_access(user, chan->ci, CA_AUTOOP))
	{
		user->SendMessage(ChanServ, _("This channel has been registered with %s."), Config->s_ChanServ.c_str());

		if (owner)
			chan->RemoveMode(NULL, CMODE_OWNER, user->nick);
		if (protect)
			chan->RemoveMode(NULL, CMODE_PROTECT, user->nick);
		chan->RemoveMode(NULL, CMODE_OP, user->nick);
		if (halfop && !check_access(user, chan->ci, CA_AUTOHALFOP))
			chan->RemoveMode(NULL, CMODE_HALFOP, user->nick);

		return 0;
	}

	return 1;
}

/*************************************************************************/

/* Remove all channels which have expired. */

void expire_chans()
{
	if (!Config->CSExpire)
		return;

	for (registered_channel_map::const_iterator it = RegisteredChannelList.begin(), it_end = RegisteredChannelList.end(); it != it_end; )
	{
		ChannelInfo *ci = it->second;
		++it;

		bool expire = false;
		if (ci->HasFlag(CI_SUSPENDED))
		{
			if (Config->CSSuspendExpire && Anope::CurTime - ci->last_used >= Config->CSSuspendExpire)
				expire = true;
		}
		else if (ci->HasFlag(CI_FORBIDDEN))
		{
			if (Config->CSForbidExpire && Anope::CurTime - ci->last_used >= Config->CSForbidExpire)
				expire = true;
		}
		else if (!ci->c && Anope::CurTime - ci->last_used >= Config->CSExpire)
			expire = true;

		if (ci->HasFlag(CI_NO_EXPIRE))
			expire = false;

		if (expire)
		{
			EventReturn MOD_RESULT;
			FOREACH_RESULT(I_OnPreChanExpire, OnPreChanExpire(ci));
			if (MOD_RESULT == EVENT_STOP)
				continue;

			Anope::string extra;
			if (ci->HasFlag(CI_FORBIDDEN))
				extra = "forbidden ";
			else if (ci->HasFlag(CI_SUSPENDED))
				extra = "suspended ";

			Log(LOG_NORMAL, "chanserv/expire") << "Expiring " << extra  << "channel " << ci->name << " (founder: " << (ci->founder ? ci->founder->display : "(none)") << ")";
			FOREACH_MOD(I_OnChanExpire, OnChanExpire(ci));
			delete ci;
		}
	}
}

/*************************************************************************/

// XXX this is slightly inefficient
void cs_remove_nick(NickCore *nc)
{
	for (registered_channel_map::const_iterator it = RegisteredChannelList.begin(), it_end = RegisteredChannelList.end(); it != it_end; ++it)
	{
		ChannelInfo *ci = it->second;

		ChanAccess *access = ci->GetAccess(nc);
		if (access)
			ci->EraseAccess(access);

		for (unsigned j = ci->GetAkickCount(); j > 0; --j)
		{
			AutoKick *akick = ci->GetAkick(j - 1);
			if (akick->HasFlag(AK_ISNICK) && akick->nc == nc)
				ci->EraseAkick(j - 1);
		}

		if (ci->founder == nc)
		{
			NickCore *newowner = NULL;
			if (ci->successor && (ci->successor->IsServicesOper() || !Config->CSMaxReg || ci->successor->channelcount < Config->CSMaxReg))
				newowner = ci->successor;
			else
			{
				ChanAccess *highest = NULL;
				for (unsigned j = 0; j < ci->GetAccessCount(); ++j)
				{
					ChanAccess *ca = ci->GetAccess(j);
					
					if (!ca->nc || (!ca->nc->IsServicesOper() && Config->CSMaxReg && ca->nc->channelcount >= Config->CSMaxReg) || (ca->nc == nc))
						continue;
					if (!highest || ca->level > highest->level)
						highest = ca;
				}
				if (highest)
					newowner = highest->nc;
			}

			if (newowner)
			{
				Log(LOG_NORMAL, "chanserv/expire") << "Transferring foundership of " << ci->name << " from deleted nick " << nc->display << " to " << newowner->display;
				ci->founder = newowner;
				ci->successor = NULL;
				++newowner->channelcount;
			}
			else
			{
				Log(LOG_NORMAL, "chanserv/expire") << "Deleting channel " << ci->name << " owned by deleted nick " << nc->display;

				if (ModeManager::FindChannelModeByName(CMODE_REGISTERED))
				{
					/* Maybe move this to delchan() ? */
					if (ci->c && ci->c->HasMode(CMODE_REGISTERED))
						ci->c->RemoveMode(NULL, CMODE_REGISTERED, true);
				}

				delete ci;
				continue;
			}
		}

		if (ci->successor == nc)
			ci->successor = NULL;
	}
}

/*************************************************************************/

ChannelInfo *cs_findchan(const Anope::string &chan)
{
	FOREACH_MOD(I_OnFindChan, OnFindChan(chan));

	registered_channel_map::const_iterator it = RegisteredChannelList.find(chan);
	if (it != RegisteredChannelList.end())
		return it->second;

	return NULL;
}

/*************************************************************************/

/* Return 1 if the user's access level on the given channel falls into the
 * given category, 0 otherwise.  Note that this may seem slightly confusing
 * in some cases: for example, check_access(..., CA_NOJOIN) returns true if
 * the user does _not_ have access to the channel (i.e. matches the NOJOIN
 * criterion). */

int check_access(User *user, ChannelInfo *ci, int what)
{
	int level, limit;

	if (!user || !ci)
		return 0;

	ChanAccess *u_access = ci->GetAccess(user);
	level = u_access ? u_access->level : 0;
	limit = ci->levels[what];

	// Set should never be disabled, if it is it is db-converter screwup
	// This all needs rewritten anyway...
	if (what == CA_SET && limit == ACCESS_INVALID)
	{
		ci->levels[what] = ACCESS_FOUNDER;
		limit = ACCESS_FOUNDER;
	}

	/* Resetting the last used time */
	if (level > 0)
		ci->last_used = Anope::CurTime;

	/* Superadmin always wins. Always. */
	if (user->isSuperAdmin)
		return what == CA_NOJOIN ? 0 : 1;
	/* If the access of the level we are checking is disabled, they *always* get denied */
	if (limit == ACCESS_INVALID)
		return 0;
	/* If the level of the user is >= the level for "founder" of this channel and "founder" isn't disabled, they can do anything */
	if (ci->levels[CA_FOUNDER] != ACCESS_INVALID && level >= ci->levels[CA_FOUNDER])
		return what == CA_NOJOIN ? 0 : 1;

	if (what == CA_NOJOIN)
		return level <= ci->levels[what];
	else
		return level >= ci->levels[what];
}

/*************************************************************************/
/*********************** ChanServ private routines ***********************/
/*************************************************************************/

/* Reset channel access level values to their default state. */

void reset_levels(ChannelInfo *ci)
{
	int i;

	if (!ci)
	{
		Log() << "reset_levels() called with NULL values";
		return;
	}

	if (ci->levels)
		delete [] ci->levels;
	ci->levels = new int16[CA_SIZE];
	for (i = 0; def_levels[i][0] >= 0; ++i)
		ci->levels[def_levels[i][0]] = def_levels[i][1];
}

/*************************************************************************/

/** Is the user the real founder?
 * @param user The user
 * @param ci The channel
 * @return true or false
 */
bool IsFounder(User *user, ChannelInfo *ci)
{
	if (!user || !ci)
		return false;

	if (user->isSuperAdmin)
		return true;

	if (user->Account() && user->Account() == ci->founder)
		return true;

	return false;
}

/*************************************************************************/

void update_cs_lastseen(User *user, ChannelInfo *ci)
{
	ChanAccess *access;

	if (!ci || !user || !user->Account())
		return;

	if (IsFounder(user, ci) || user->IsIdentified() || (user->IsRecognized() && !ci->HasFlag(CI_SECURE)))
		if ((access = ci->GetAccess(user)))
			access->last_seen = Anope::CurTime;
}

/*************************************************************************/

/* Returns the best ban possible for a user depending of the bantype
   value. */

int get_idealban(ChannelInfo *ci, User *u, Anope::string &ret)
{
	Anope::string mask;

	if (!ci || !u)
		return 0;

	Anope::string vident = u->GetIdent();

	switch (ci->bantype)
	{
		case 0:
			ret = "*!" + vident + "@" + u->GetDisplayedHost();
			return 1;
		case 1:
			if (vident[0] == '~')
				ret = "*!*" + vident + "@" + u->GetDisplayedHost();
			else
				ret = "*!" + vident + "@" + u->GetDisplayedHost();

			return 1;
		case 2:
			ret = "*!*@" + u->GetDisplayedHost();
			return 1;
		case 3:
			mask = create_mask(u);
			ret = "*!" + mask;
			return 1;

		default:
			return 0;
	}
}

/*************************************************************************/

Anope::string get_xop_level(int level)
{
	ChannelMode *halfop = ModeManager::FindChannelModeByName(CMODE_HALFOP);

	if (level < ACCESS_VOP)
		return "Err";
	else if (halfop && level < ACCESS_HOP)
		return "VOP";
	else if (!halfop && level < ACCESS_AOP)
		return "VOP";
	else if (halfop && level < ACCESS_AOP)
		return "HOP";
	else if (level < ACCESS_SOP)
		return "AOP";
	else if (level < ACCESS_QOP)
		return "SOP";
	else if (level < ACCESS_FOUNDER)
		return "QOP";
	else
		return "Founder";
}

ChanServTimer::ChanServTimer(Channel *chan) : Timer(Config->CSInhabit), c(chan)
{
	if (!ChanServ)
		return;
	if (c->ci)
		c->ci->SetFlag(CI_INHABIT);
	if (!c->ci || !c->ci->bi)
		ChanServ->Join(*c);
	else if (!c->FindUser(c->ci->bi))
		c->ci->bi->Join(*c);
}

void ChanServTimer::Tick(time_t)
{
	if (!c || !c->ci || !ChanServ)
		return;

	c->ci->UnsetFlag(CI_INHABIT);

	if (!c->ci->bi && ChanServ)
		ChanServ->Part(*c);
	else if (c->users.size() == 1 || c->users.size() < Config->BSMinUsers)
		c->ci->bi->Part(*c);
}

