/* ChanServ core functions
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

class CommandCSHelp : public Command
{
 public:
	CommandCSHelp() : Command("HELP", 1, 1)
	{
		this->SetFlag(CFLAG_ALLOW_UNREGISTERED);
		this->SetFlag(CFLAG_STRIP_CHANNEL);
	}

	CommandReturn Execute(CommandSource &source, const std::vector<Anope::string> &params)
	{
		mod_help_cmd(ChanServ, source.u, NULL, params[0]);

		return MOD_CONT;
	}

	void OnSyntaxError(CommandSource &source, const Anope::string &subcommand)
	{
		User *u = source.u;
		source.Reply(_("\002%S\002 allows you to register and control various\n"
				"aspects of channels. %S can often prevent\n"
				"malicious users from \"taking over\" channels by limiting\n"
				"who is allowed channel operator privileges. Available\n"
				"commands are listed below; to use them, type\n"
				"\002%R%S \037command\037\002. For more information on a\n"
				"specific command, type \002%R%S HELP \037command\037\002."));
		for (CommandMap::const_iterator it = ChanServ->Commands.begin(); it != ChanServ->Commands.end(); ++it)
			if (!Config->HidePrivilegedCommands || it->second->permission.empty() || (u->Account() && u->Account()->HasCommand(it->second->permission)))
				it->second->OnServHelp(source);
		if (Config->CSExpire >= 86400)
			source.Reply(_("Note that any channel which is not used for %d days\n"
			"(i.e. which no user on the channel's access list enters\n"
			"for that period of time) will be automatically dropped."), Config->CSExpire / 86400);
		if (u->Account() && u->Account()->IsServicesOper())
			source.Reply(_(" \n"
					"Services Operators can also drop any channel without needing\n"
					"to identify via password, and may view the access, AKICK,\n"
					"and level setting lists for any channel."));
	}
};

class CSHelp : public Module
{
	CommandCSHelp commandcshelp;

 public:
	CSHelp(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator)
	{
		this->SetAuthor("Anope");
		this->SetType(CORE);

		this->AddCommand(ChanServ, &commandcshelp);
	}
};

MODULE_INIT(CSHelp)
