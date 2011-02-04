/* OperServ core functions
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

#include "module.h"

void defcon_sendlvls(CommandSource &source);
void runDefCon();
void defconParseModeString(const Anope::string &str);
void resetDefCon(int level);
static Anope::string defconReverseModes(const Anope::string &modes);

class DefConTimeout : public Timer
{
	int level;

 public:
	DefConTimeout(int newlevel) : Timer(Config->DefConTimeOut), level(newlevel) { }

	void Tick(time_t)
	{
		if (Config->DefConLevel != level)
		{
			Config->DefConLevel = level;
			FOREACH_MOD(I_OnDefconLevel, OnDefconLevel(level));
			Log(OperServ, "operserv/defcon") << "Defcon level timeout, returning to level " << level;
			ircdproto->SendGlobops(OperServ, GetString(NULL, _("\002%s\002 Changed the DEFCON level to \002%d\002")).c_str(), Config->s_OperServ.c_str(), level);

			if (Config->GlobalOnDefcon)
			{
				if (!Config->DefConOffMessage.empty())
					oper_global("", "%s", Config->DefConOffMessage.c_str());
				else
					oper_global("", GetString(NULL, _("The Defcon Level is now at Level: \002%d\002")).c_str(), Config->DefConLevel);

				if (Config->GlobalOnDefconMore && Config->DefConOffMessage.empty())
					oper_global("", "%s", Config->DefconMessage.c_str());
			}

			runDefCon();
		}
	}
};
static DefConTimeout *timeout;

class CommandOSDefcon : public Command
{
 public:
	CommandOSDefcon() : Command("DEFCON", 1, 1, "operserv/defcon")
	{
	}

	CommandReturn Execute(CommandSource &source, const std::vector<Anope::string> &params)
	{
		User *u = source.u;
		const Anope::string &lvl = params[0];
		int newLevel = 0;

		if (lvl.empty())
		{
			source.Reply(_("Services are now at DEFCON \002%d\002"), Config->DefConLevel);
			defcon_sendlvls(source);
			return MOD_CONT;
		}
		newLevel = lvl.is_number_only() ? convertTo<int>(lvl) : 0;
		if (newLevel < 1 || newLevel > 5)
		{
			this->OnSyntaxError(source, "");
			return MOD_CONT;
		}
		Config->DefConLevel = newLevel;

		FOREACH_MOD(I_OnDefconLevel, OnDefconLevel(newLevel));

		if (timeout)
		{
			delete timeout;
			timeout = NULL;
		}

		if (Config->DefConTimeOut)
			timeout = new DefConTimeout(5);

		source.Reply(_("Services are now at DEFCON \002%d\002"), Config->DefConLevel);
		defcon_sendlvls(source);
		Log(LOG_ADMIN, u, this) << "to change defcon level to " << newLevel;
		ircdproto->SendGlobops(OperServ, GetString(NULL, _("\002%s\002 Changed the DEFCON level to \002%d\002")).c_str(), u->nick.c_str(), newLevel);
		/* Global notice the user what is happening. Also any Message that
		   the Admin would like to add. Set in config file. */
		if (Config->GlobalOnDefcon)
		{
			if (Config->DefConLevel == 5 && !Config->DefConOffMessage.empty())
				oper_global("", "%s", Config->DefConOffMessage.c_str());
			else
				oper_global("", GetString(NULL, _("The Defcon Level is now at Level: \002%d\002")).c_str(), Config->DefConLevel);
		}
		if (Config->GlobalOnDefconMore)
		{
			if (Config->DefConOffMessage.empty() || Config->DefConLevel != 5)
				oper_global("", "%s", Config->DefconMessage.c_str());
		}
		/* Run any defcon functions, e.g. FORCE CHAN MODE */
		runDefCon();
		return MOD_CONT;
	}

	bool OnHelp(CommandSource &source, const Anope::string &subcommand)
	{
		source.Reply(_("Syntax: \002DEFCON\002 [\0021\002|\0022\002|\0023\002|\0024\002|\0025\002]\n"
				"The defcon system can be used to implement a pre-defined\n"
				"set of restrictions to services useful during an attempted\n"
				"attack on the network."));
		return true;
	}

	void OnSyntaxError(CommandSource &source, const Anope::string &subcommand)
	{
		SyntaxError(source, "DEFCON", _("DEFCON [\0021\002|\0022\002|\0023\002|\0024\002|\0025\002]"));
	}

	void OnServHelp(CommandSource &source)
	{
		source.Reply(_("    DEFCON      Manipulate the DefCon system"));
	}
};

class OSDefcon : public Module
{
	CommandOSDefcon commandosdefcon;

 public:
	OSDefcon(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator)
	{
		if (!Config->DefConLevel)
			throw ModuleException("Invalid configuration settings");

		this->SetAuthor("Anope");
		this->SetType(CORE);

		Implementation i[] = { I_OnPreUserConnect, I_OnChannelModeSet, I_OnChannelModeUnset, I_OnPreCommandRun, I_OnPreCommand, I_OnUserConnect, I_OnChannelModeAdd, I_OnChannelCreate };
		ModuleManager::Attach(i, this, 8);

		this->AddCommand(OperServ, &commandosdefcon);

		defconParseModeString(Config->DefConChanModes);
	}

	EventReturn OnPreUserConnect(User *u)
	{
		if (u->server->IsSynced() && CheckDefCon(DEFCON_AKILL_NEW_CLIENTS) && !u->server->IsULined())
		{
			if (CheckDefCon(DEFCON_AKILL_NEW_CLIENTS))
			{
				Log(OperServ, "operserv/defcon") << "DEFCON: adding akill for *@" << u->host;
				XLine *x = SGLine->Add(NULL, NULL, "*@" + u->host, Anope::CurTime + Config->DefConAKILL, !Config->DefConAkillReason.empty() ? Config->DefConAkillReason : "DEFCON AKILL");
				if (x)
					x->By = Config->s_OperServ;
			}

			if (CheckDefCon(DEFCON_NO_NEW_CLIENTS) || CheckDefCon(DEFCON_AKILL_NEW_CLIENTS))
				kill_user(Config->s_OperServ, u, Config->DefConAkillReason);

			return EVENT_STOP;
		}

		return EVENT_CONTINUE;
	}

	EventReturn OnChannelModeSet(Channel *c, ChannelModeName Name, const Anope::string &param)
	{
		ChannelMode *cm = ModeManager::FindChannelModeByName(Name);

		if (CheckDefCon(DEFCON_FORCE_CHAN_MODES) && cm && DefConModesOff.HasFlag(Name))
		{
			c->RemoveMode(OperServ, Name, param);

			return EVENT_STOP;
		}

		return EVENT_CONTINUE;
	}

	EventReturn OnChannelModeUnset(Channel *c, ChannelModeName Name, const Anope::string &)
	{
		ChannelMode *cm = ModeManager::FindChannelModeByName(Name);

		if (CheckDefCon(DEFCON_FORCE_CHAN_MODES) && cm && DefConModesOn.HasFlag(Name))
		{
			Anope::string param;

			if (GetDefConParam(Name, param))
				c->SetMode(OperServ, Name, param);
			else
				c->SetMode(OperServ, Name);

			return EVENT_STOP;

		}

		return EVENT_CONTINUE;
	}

	EventReturn OnPreCommandRun(User *&u, BotInfo *&bi, Anope::string &command, Anope::string &message, ChannelInfo *&ci)
	{
		if (!u->HasMode(UMODE_OPER) && (CheckDefCon(DEFCON_OPER_ONLY) || CheckDefCon(DEFCON_SILENT_OPER_ONLY)))
		{
			if (!CheckDefCon(DEFCON_SILENT_OPER_ONLY))
				u->SendMessage(bi, _("Services are in Defcon mode, Please try again later."));

			return EVENT_STOP;
		}

		return EVENT_CONTINUE;
	}

	EventReturn OnPreCommand(CommandSource &source, Command *command, const std::vector<Anope::string> &params)
	{
		BotInfo *service = source.owner;
		if (service == NickServ)
		{
			if (command->name.equals_ci("REGISTER") || command->name.equals_ci("GROUP"))
			{
				if (CheckDefCon(DEFCON_NO_NEW_NICKS))
				{
					source.Reply(_("Services are in Defcon mode, Please try again later."));
					return EVENT_STOP;
				}
			}
		}
		else if (ChanServ && service == ChanServ)
		{
			if (command->name.equals_ci("SET"))
			{
				if (!params.empty() && params[0].equals_ci("MLOCK") && CheckDefCon(DEFCON_NO_MLOCK_CHANGE))
				{
					source.Reply(_("Services are in Defcon mode, Please try again later."));
					return EVENT_STOP;
				}
			}
			else if (command->name.equals_ci("REGISTER"))
			{
				if (CheckDefCon(DEFCON_NO_NEW_CHANNELS))
				{
					source.Reply(_("Services are in Defcon mode, Please try again later."));
					return EVENT_STOP;
				}
			}
		}
		else if (MemoServ && service == MemoServ)
		{
			if (command->name.equals_ci("SEND") || command->name.equals_ci("SENDALL"))
			{
				if (CheckDefCon(DEFCON_NO_NEW_MEMOS))
				{
					source.Reply(_("Services are in Defcon mode, Please try again later."));
					return EVENT_STOP;
				}
			}
		}

		return EVENT_CONTINUE;
	}

	void OnUserConnect(User *u)
	{
		Session *session = findsession(u->host);
		Exception *exception = find_hostip_exception(u->host, u->ip.addr());

		if (CheckDefCon(DEFCON_REDUCE_SESSION) && !exception)
		{
			if (session && session->count > Config->DefConSessionLimit)
			{
				if (!Config->SessionLimitExceeded.empty())
					ircdproto->SendMessage(OperServ, u->nick, Config->SessionLimitExceeded.c_str(), u->host.c_str());
				if (!Config->SessionLimitDetailsLoc.empty())
					ircdproto->SendMessage(OperServ, u->nick, "%s", Config->SessionLimitDetailsLoc.c_str());

				kill_user(Config->s_OperServ, u, "Session limit exceeded");
				++session->hits;
				if (Config->MaxSessionKill && session->hits >= Config->MaxSessionKill)
				{
					SGLine->Add(NULL, NULL, "*@" + u->host, Anope::CurTime + Config->SessionAutoKillExpiry, "Session limit exceeded");
					ircdproto->SendGlobops(OperServ, "Added a temporary AKILL for \2*@%s\2 due to excessive connections", u->host.c_str());
				}
			}
		}
	}

	void OnChannelModeAdd(ChannelMode *cm)
	{
		if (!Config->DefConChanModes.empty())
		{
			Anope::string modes = Config->DefConChanModes;

			if (modes.find(cm->ModeChar) != Anope::string::npos)
				/* New mode has been added to Anope, check to see if defcon
				 * requires it
				 */
				defconParseModeString(Config->DefConChanModes);
		}
	}

	void OnChannelCreate(Channel *c)
	{
		if (CheckDefCon(DEFCON_FORCE_CHAN_MODES))
			c->SetModes(OperServ, false, "%s", Config->DefConChanModes.c_str());
	}
};

/**
 * Send a message to the oper about which precautions are "active" for this level
 **/
void defcon_sendlvls(CommandSource &source)
{
	if (CheckDefCon(DEFCON_NO_NEW_CHANNELS))
		source.Reply(_("* No new channel registrations"));
	if (CheckDefCon(DEFCON_NO_NEW_NICKS))
		source.Reply(_("* No new nick registrations"));
	if (CheckDefCon(DEFCON_NO_MLOCK_CHANGE))
		source.Reply(_("* No MLOCK changes"));
	if (CheckDefCon(DEFCON_FORCE_CHAN_MODES) && !Config->DefConChanModes.empty())
		source.Reply(_("* Force Chan Modes (%s) to be set on all channels"), Config->DefConChanModes.c_str());
	if (CheckDefCon(DEFCON_REDUCE_SESSION))
		source.Reply(_("* Use the reduced session limit of %d"), Config->DefConSessionLimit);
	if (CheckDefCon(DEFCON_NO_NEW_CLIENTS))
		source.Reply(_("* Kill any NEW clients connecting"));
	if (CheckDefCon(DEFCON_OPER_ONLY))
		source.Reply(_("* Ignore any non-opers with message"));
	if (CheckDefCon(DEFCON_SILENT_OPER_ONLY))
		source.Reply(_("* Silently ignore non-opers"));
	if (CheckDefCon(DEFCON_AKILL_NEW_CLIENTS))
		source.Reply(_("* AKILL any new clients connecting"));
	if (CheckDefCon(DEFCON_NO_NEW_MEMOS))
		source.Reply(_("* No new memos sent"));
}

void runDefCon()
{
	if (CheckDefCon(DEFCON_FORCE_CHAN_MODES))
	{
		if (!Config->DefConChanModes.empty() && !DefConModesSet)
		{
			if (Config->DefConChanModes[0] == '+' || Config->DefConChanModes[0] == '-')
			{
				Log(OperServ, "operserv/defcon") << "DEFCON: setting " << Config->DefConChanModes << " on all channels";
				DefConModesSet = true;
				MassChannelModes(OperServ, Config->DefConChanModes);
			}
		}
	}
	else
	{
		if (!Config->DefConChanModes.empty() && DefConModesSet)
		{
			if (Config->DefConChanModes[0] == '+' || Config->DefConChanModes[0] == '-')
			{
				DefConModesSet = false;
				Anope::string newmodes = defconReverseModes(Config->DefConChanModes);
				if (!newmodes.empty())
				{
					Log(OperServ, "operserv/defcon") << "DEFCON: setting " << newmodes << " on all channels";
					MassChannelModes(OperServ, newmodes);
				}
			}
		}
	}
}

/* Parse the defcon mlock mode string and set the correct global vars.
 *
 * @param str mode string to parse
 * @return 1 if accepted, 0 if failed
 */
void defconParseModeString(const Anope::string &str)
{
	int add = -1; /* 1 if adding, 0 if deleting, -1 if neither */
	unsigned char mode;
	ChannelMode *cm;
	ChannelModeParam *cmp;
	Anope::string modes, param;

	if (str.empty())
		return;

	spacesepstream ss(str);

	DefConModesOn.ClearFlags();
	DefConModesOff.ClearFlags();
	ss.GetToken(modes);

	/* Loop while there are modes to set */
	for (unsigned i = 0, end = modes.length(); i < end; ++i)
	{
		mode = modes[i];

		switch (mode)
		{
			case '+':
				add = 1;
				continue;
			case '-':
				add = 0;
				continue;
			default:
				if (add < 0)
					continue;
		}

		if ((cm = ModeManager::FindChannelModeByChar(mode)))
		{
			if (cm->Type == MODE_STATUS || cm->Type == MODE_LIST || !cm->CanSet(NULL))
			{
				Log() << "DefConChanModes mode character '" << mode << "' cannot be locked";
				continue;
			}
			else if (add)
			{
				DefConModesOn.SetFlag(cm->Name);
				DefConModesOff.UnsetFlag(cm->Name);

				if (cm->Type == MODE_PARAM)
				{
					cmp = debug_cast<ChannelModeParam *>(cm);

					if (!ss.GetToken(param))
					{
						Log() << "DefConChanModes mode character '" << mode << "' has no parameter while one is expected";
						continue;
					}

					if (!cmp->IsValid(param))
						continue;

					SetDefConParam(cmp->Name, param);
				}
			}
			else
			{
				if (DefConModesOn.HasFlag(cm->Name))
				{
					DefConModesOn.UnsetFlag(cm->Name);

					if (cm->Type == MODE_PARAM)
						UnsetDefConParam(cm->Name);
				}
			}
		}
	}

	/* We can't mlock +L if +l is not mlocked as well. */
	if ((cm = ModeManager::FindChannelModeByName(CMODE_REDIRECT)) && DefConModesOn.HasFlag(cm->Name) && !DefConModesOn.HasFlag(CMODE_LIMIT))
	{
		DefConModesOn.UnsetFlag(CMODE_REDIRECT);

		Log() << "DefConChanModes must lock mode +l as well to lock mode +L";
	}

	/* Some ircd we can't set NOKNOCK without INVITE */
	/* So check if we need there is a NOKNOCK MODE and that we need INVITEONLY */
	if (ircd->knock_needs_i && (cm = ModeManager::FindChannelModeByName(CMODE_NOKNOCK)) && DefConModesOn.HasFlag(cm->Name) && !DefConModesOn.HasFlag(CMODE_INVITE))
	{
		DefConModesOn.UnsetFlag(CMODE_NOKNOCK);
		Log() << "DefConChanModes must lock mode +i as well to lock mode +K";
	}
}

static Anope::string defconReverseModes(const Anope::string &modes)
{
	if (modes.empty())
		return "";
	Anope::string newmodes;
	for (unsigned i = 0, end = modes.length(); i < end; ++i)
	{
		if (modes[i] == '+')
			newmodes += '-';
		else if (modes[i] == '-')
			newmodes += '+';
		else
			newmodes += modes[i];
	}
	return newmodes;
}

MODULE_INIT(OSDefcon)
