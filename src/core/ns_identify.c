/* NickServ core functions
 *
 * (C) 2003-2009 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 *
 * $Id$
 *
 */
/*************************************************************************/

#include "module.h"

#define TO_COLLIDE 0 /* Collide the user with this nick */
#define TO_RELEASE 1 /* Release a collided nick */

void myNickServHelp(User *u);

class CommandNSIdentify : public Command
{
 public:
	CommandNSIdentify(const std::string &cname) : Command(cname, 1, 1)
	{
	}

	CommandReturn Execute(User *u, std::vector<std::string> &params)
	{
		const char *pass = params[0].c_str();
		NickAlias *na;
		NickRequest *nr;
		int res;
		char tsbuf[16];
		char modes[512];
		int len;

		if (!(na = u->na))
		{
			if ((nr = findrequestnick(u->nick)))
				notice_lang(s_NickServ, u, NICK_IS_PREREG);
			else
				notice_lang(s_NickServ, u, NICK_NOT_REGISTERED);
		}
		else if (na->status & NS_FORBIDDEN)
			notice_lang(s_NickServ, u, NICK_X_FORBIDDEN, na->nick);
		else if (na->nc->flags & NI_SUSPENDED)
			notice_lang(s_NickServ, u, NICK_X_SUSPENDED, na->nick);
		else if (nick_identified(u))
			notice_lang(s_NickServ, u, NICK_ALREADY_IDENTIFIED);
		else if (!(res = enc_check_password(pass, na->nc->pass)))
		{
			alog("%s: Failed IDENTIFY for %s!%s@%s", s_NickServ, u->nick, u->GetIdent().c_str(), u->host);
			notice_lang(s_NickServ, u, PASSWORD_INCORRECT);
			bad_password(u);
		}
		else if (res == -1)
			notice_lang(s_NickServ, u, NICK_IDENTIFY_FAILED);
		else
		{
			if (!(na->status & NS_IDENTIFIED) && !(na->status & NS_RECOGNIZED))
			{
				if (na->last_usermask)
					delete [] na->last_usermask;
				na->last_usermask = new char[u->GetIdent().length() + u->GetDisplayedHost().length() + 2];
				sprintf(na->last_usermask, "%s@%s", u->GetIdent().c_str(), u->GetDisplayedHost().c_str());
				if (na->last_realname)
					delete [] na->last_realname;
				na->last_realname = sstrdup(u->realname);
			}

			na->status |= NS_IDENTIFIED;
			na->last_seen = time(NULL);
			snprintf(tsbuf, sizeof(tsbuf), "%lu", static_cast<unsigned long>(u->timestamp));

			if (ircd->modeonreg)
			{
				len = strlen(ircd->modeonreg);
				strncpy(modes, ircd->modeonreg, 512);
				if (ircd->rootmodeonid && is_services_root(u))
					strncat(modes, ircd->rootmodeonid, 512 - len);
				else if (ircd->adminmodeonid && is_services_admin(u))
					strncat(modes, ircd->adminmodeonid, 512 - len);
				else if (ircd->opermodeonid && is_services_oper(u))
					strncat(modes, ircd->opermodeonid, 512 - len);
				if (ircd->tsonmode)
					common_svsmode(u, modes, tsbuf);
				else
					common_svsmode(u, modes, "");
			}
			send_event(EVENT_NICK_IDENTIFY, 1, u->nick);
			alog("%s: %s!%s@%s identified for nick %s", s_NickServ, u->nick, u->GetIdent().c_str(), u->host, u->nick);
			notice_lang(s_NickServ, u, NICK_IDENTIFY_SUCCEEDED);
			if (ircd->vhost)
				do_on_id(u);
			if (NSModeOnID)
				do_setmodes(u);

			if (NSForceEmail && u->na && !u->na->nc->email)
			{
				notice_lang(s_NickServ, u, NICK_IDENTIFY_EMAIL_REQUIRED);
				notice_help(s_NickServ, u, NICK_IDENTIFY_EMAIL_HOWTO);
			}

			if (!(na->status & NS_RECOGNIZED))
				check_memos(u);

			/* Enable nick tracking if enabled */
			if (NSNickTracking)
				nsStartNickTracking(u);

			/* Clear any timers */
			if (na->nc->flags & NI_KILLPROTECT)
				del_ns_timeout(na, TO_COLLIDE);
		}
		return MOD_CONT;
	}

	bool OnHelp(User *u, const std::string &subcommand)
	{
		notice_help(s_NickServ, u, NICK_HELP_IDENTIFY);
		return true;
	}

	void OnSyntaxError(User *u)
	{
		syntax_error(s_NickServ, u, "IDENTIFY", NICK_IDENTIFY_SYNTAX);
	}
};

class NSIdentify : public Module
{
 public:
	NSIdentify(const std::string &modname, const std::string &creator) : Module(modname, creator)
	{
		this->SetAuthor("Anope");
		this->SetVersion("$Id$");
		this->SetType(CORE);

		this->AddCommand(NICKSERV, new CommandNSIdentify("IDENTIFY"), MOD_UNIQUE);
		this->AddCommand(NICKSERV, new CommandNSIdentify("ID"), MOD_UNIQUE);

		this->SetNickHelp(myNickServHelp);
	}
};

/**
 * Add the help response to anopes /ns help output.
 * @param u The user who is requesting help
 **/
void myNickServHelp(User *u)
{
	notice_lang(s_NickServ, u, NICK_HELP_CMD_IDENTIFY);
}

MODULE_INIT("ns_identify", NSIdentify)
