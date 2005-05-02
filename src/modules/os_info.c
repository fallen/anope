/* os_info.c - Adds oper information lines to nicks/channels
 *
 * (C) 2003-2005 Anope Team
 * Contact us at info@anope.org
 *
 * Based on the original module by Rob <rob@anope.org>
 * Included in the Anope module pack since Anope 1.7.9
 * Anope Coder: DrStein <drstein@anope.org>
 *
 * Please read COPYING and README for further details.
 *
 * Send bug reports to the Anope Coder instead of the module
 * author, because any changes since the inclusion into anope
 * are not supported by the original author.
 *
 */
/*************************************************************************/

#include "module.h"

#define AUTHOR "Rob"
#define VERSION "$Id$"

/* Default database name */
#define DEFAULT_DB_NAME "os_info.db"

/* Multi-language stuff */
#define LANG_NUM_STRINGS   16

#define OINFO_SYNTAX        0
#define OINFO_ADD_SUCCESS   1
#define OINFO_DEL_SUCCESS   2
#define OCINFO_SYNTAX       3
#define OCINFO_ADD_SUCCESS  4
#define OCINFO_DEL_SUCCESS  5
#define OINFO_HELP_1        6
#define OINFO_HELP_2        7
#define OINFO_HELP_3        8
#define OINFO_HELP_4        9
#define OCINFO_HELP_1      10
#define OCINFO_HELP_2      11
#define OCINFO_HELP_3      12
#define OCINFO_HELP_4      13
#define OINFO_HELP         14
#define OCINFO_HELP        15

/*************************************************************************/

char *OSInfoDBName = NULL;

int myAddNickInfo(User * u);
int myAddChanInfo(User * u);
int myNickInfo(User * u);
int myChanInfo(User * u);

int mNickHelp(User * u);
int mChanHelp(User * u);
void mMainChanHelp(User * u);
void mMainNickHelp(User * u);
void m_AddLanguages(void);

int mLoadData(void);
int mSaveData(int argc, char **argv);
int mLoadConfig();
int mEventReload(int argc, char **argv);

/*************************************************************************/

/**
 * AnopeInit is called when the module is loaded
 * @param argc Argument count
 * @param argv Argument list
 * @return MOD_CONT to allow the module, MOD_STOP to stop it
 **/
int AnopeInit(int argc, char **argv)
{
    Command *c;
    EvtHook *hook = NULL;

    int status;

    moduleAddAuthor(AUTHOR);
    moduleAddVersion(VERSION);
    moduleSetType(SUPPORTED);

    alog("os_info: Loading configuration directives...");
    if (mLoadConfig()) {
        return MOD_STOP;
    }

    c = createCommand("oInfo", myAddNickInfo, is_oper, -1, -1, -1, -1, -1);
    moduleAddHelp(c, mNickHelp);
    status = moduleAddCommand(NICKSERV, c, MOD_HEAD);

    c = createCommand("Info", myNickInfo, NULL, -1, -1, -1, -1, -1);
    status = moduleAddCommand(NICKSERV, c, MOD_TAIL);

    c = createCommand("oInfo", myAddChanInfo, is_oper, -1, -1, -1, -1, -1);
    moduleAddHelp(c, mChanHelp);
    status = moduleAddCommand(CHANSERV, c, MOD_HEAD);

    c = createCommand("Info", myChanInfo, NULL, -1, -1, -1, -1, -1);
    status = moduleAddCommand(CHANSERV, c, MOD_TAIL);

    hook = createEventHook(EVENT_DB_SAVING, mSaveData);
    status = moduleAddEventHook(hook);

    hook = createEventHook(EVENT_RELOAD, mEventReload);
    status = moduleAddEventHook(hook);

    moduleSetNickHelp(mMainNickHelp);
    moduleSetChanHelp(mMainChanHelp);

    mLoadData();
    m_AddLanguages();

    return MOD_CONT;
}

/**
 * Unload the module
 **/
void AnopeFini(void)
{
    if (OSInfoDBName)
        free(OSInfoDBName);
}

/*************************************************************************/

/**
 * Provide the user interface to add/remove/update oper information
 * about a nick.
 * We are going to assume that anyone who gets this far is an oper;
 * the createCommand should have handled this checking for us and its 
 * tedious / a waste to do it twice.
 * @param u The user who executed this command
 * @return MOD_CONT if we want to process other commands in this command
 * stack, MOD_STOP if we dont
 **/
int myAddNickInfo(User * u)
{
    char *text = NULL;
    char *cmd = NULL;
    char *nick = NULL;
    char *info = NULL;
    NickAlias *na = NULL;

    /* Get the last buffer anope recived */
    text = moduleGetLastBuffer();
    if (text) {
        cmd = myStrGetToken(text, ' ', 0);
        nick = myStrGetToken(text, ' ', 1);
        info = myStrGetTokenRemainder(text, ' ', 2);
        if (cmd && nick) {
            if (strcasecmp(cmd, "ADD") == 0) {
                /* Syntax error, again! */
                if (info) {
                    /* ok we've found the user */
                    if ((na = findnick(nick))) {
                        /* Add the module data to the user */
                        moduleAddData(&na->nc->moduleData, "info", info);
                        moduleNoticeLang(s_NickServ, u,
                                         OINFO_ADD_SUCCESS, nick);
                        /* NickCore not found! */
                    } else {
                        notice_lang(s_NickServ, u, NICK_X_NOT_REGISTERED,
                                    nick);
                    }
                    free(info);
                }
            } else if (strcasecmp(cmd, "DEL") == 0) {
                /* ok we've found the user */
                if ((na = findnick(nick))) {
                    moduleDelData(&na->nc->moduleData, "info");
                    moduleNoticeLang(s_NickServ, u,
                                     OINFO_DEL_SUCCESS, nick);
                    /* NickCore not found! */
                } else {
                    notice_lang(s_NickServ, u, NICK_X_NOT_REGISTERED,
                                nick);
                }
                /* another syntax error! */
            } else {
                moduleNoticeLang(s_NickServ, u, OINFO_SYNTAX);
            }
            free(cmd);
            free(nick);
            /* Syntax error */
        } else if (cmd) {
            moduleNoticeLang(s_NickServ, u, OINFO_SYNTAX);
            free(cmd);
            /* Syntax error */
        } else {
            moduleNoticeLang(s_NickServ, u, OINFO_SYNTAX);
        }
        free(text);
    }
    return MOD_CONT;
}

/**
 * Provide the user interface to add/remove/update oper information
 * about a channel.
 * We are going to assume that anyone who gets this far is an oper; 
 * the createCommand should have handled this checking for us and 
 * its tedious / a waste to do it twice.
 * @param u The user who executed this command
 * @return MOD_CONT if we want to process other commands in this command
 * stack, MOD_STOP if we dont
 **/
int myAddChanInfo(User * u)
{
    char *text = NULL;
    char *cmd = NULL;
    char *chan = NULL;
    char *info = NULL;
    ChannelInfo *ci = NULL;

    /* Get the last buffer anope recived */
    text = moduleGetLastBuffer();
    if (text) {
        cmd = myStrGetToken(text, ' ', 0);
        chan = myStrGetToken(text, ' ', 1);
        info = myStrGetTokenRemainder(text, ' ', 2);
        if (cmd && chan) {
            if (strcasecmp(cmd, "ADD") == 0) {
                if (info) {
                    if ((ci = cs_findchan(chan))) {
                        /* Add the module data to the channel */
                        moduleAddData(&ci->moduleData, "info", info);
                        moduleNoticeLang(s_ChanServ, u,
                                         OCINFO_ADD_SUCCESS, chan);
                        /* ChanInfo */
                    } else {
                        notice_lang(s_ChanServ, u, CHAN_X_NOT_REGISTERED,
                                    chan);
                    }
                    free(info);
                }
            } else if (strcasecmp(cmd, "DEL") == 0) {
                if ((ci = cs_findchan(chan))) {
                    /* Del the module data from the channel */
                    moduleDelData(&ci->moduleData, "info");
                    moduleNoticeLang(s_ChanServ, u,
                                     OCINFO_DEL_SUCCESS, chan);
                    /* ChanInfo */
                } else {
                    notice_lang(s_ChanServ, u, CHAN_X_NOT_REGISTERED,
                                chan);
                }
                /* another syntax error! */
            } else {
                moduleNoticeLang(s_ChanServ, u, OCINFO_SYNTAX);
            }
            free(cmd);
            free(chan);
            /* Syntax error */
        } else if (cmd) {
            moduleNoticeLang(s_ChanServ, u, OCINFO_SYNTAX);
            free(cmd);
            /* Syntax error */
        } else {
            moduleNoticeLang(s_ChanServ, u, OCINFO_SYNTAX);
        }
        free(text);
    }
    return MOD_CONT;
}

/*************************************************************************/

/**
 * Called after a user does a /msg nickserv info [nick] 
 * @param u The user who requested info
 * @return MOD_CONT to continue processing commands or MOD_STOP to stop
 **/
int myNickInfo(User * u)
{
    char *text = NULL;
    char *nick = NULL;
    NickAlias *na = NULL;

    /* Only show our goodies to opers */
    if (is_oper(u)) {
        /* Get the last buffer anope recived */
        text = moduleGetLastBuffer();
        if (text) {
            nick = myStrGetToken(text, ' ', 0);
            if (nick) {
                /* ok we've found the user */
                if ((na = findnick(nick))) {
                    /* If we have any info on this user */
                    if (moduleGetData(&na->nc->moduleData, "info")) {
                        notice_user(s_NickServ, u, "         OperInfo: %s",
                                    moduleGetData(&na->nc->moduleData,
                                                  "info"));
                    }
                    /* NickCore not found! */
                } else {
                    /* we dont care! */
                }
                free(nick);
            }
            free(text);
        }
    }
    return MOD_CONT;
}

/**
 * Called after a user does a /msg chanserv info chan 
 * @param u The user who requested info
 * @return MOD_CONT to continue processing commands or MOD_STOP to stop
 **/
int myChanInfo(User * u)
{
    char *text = NULL;
    char *chan = NULL;
    ChannelInfo *ci = NULL;

    /* Only show our goodies to opers */
    if (is_oper(u)) {
        /* Get the last buffer anope recived */
        text = moduleGetLastBuffer();
        if (text) {
            chan = myStrGetToken(text, ' ', 0);
            if (chan) {
                if ((ci = cs_findchan(chan))) {
                    /* If we have any info on this channel */
                    if (moduleGetData(&ci->moduleData, "info")) {
                        notice_user(s_ChanServ, u, "       OperInfo: %s",
                                    moduleGetData(&ci->moduleData,
                                                  "info"));
                    }
                }
                free(chan);
            }
            free(text);
        }
    }
    return MOD_CONT;
}

/*************************************************************************/

/** 
 * Load data from the db file, and populate our OperInfo lines
 * @return 0 for success
 **/
int mLoadData(void)
{
    int ret = 0;
    FILE *in;

    char *type = NULL;
    char *name = NULL;
    char *info = NULL;
    int len = 0;

    ChannelInfo *ci = NULL;
    NickAlias *na = NULL;

    /* will _never_ be this big thanks to the 512 limit of a message */
    char buffer[2000];
    if ((in = fopen(OSInfoDBName, "r")) == NULL) {
        alog("os_info: WARNING: can not open the database file! (it might not exist, this is not fatal)");
        ret = 1;
    } else {
        while (!feof(in)) {
            fgets(buffer, 1500, in);
            type = myStrGetToken(buffer, ' ', 0);
            name = myStrGetToken(buffer, ' ', 1);
            info = myStrGetTokenRemainder(buffer, ' ', 2);
            if (type) {
                if (name) {
                    if (info) {
                        len = strlen(info);
                        /* Take the \n from the end of the line */
                        info[len - 1] = '\0';
                        if (stricmp(type, "C") == 0) {
                            if ((ci = cs_findchan(name))) {
                                moduleAddData(&ci->moduleData, "info",
                                              info);
                            }
                        } else if (stricmp(type, "N") == 0) {
                            if ((na = findnick(name))) {
                                moduleAddData(&na->nc->moduleData, "info",
                                              info);
                            }
                        }
                        free(info);
                    }
                    free(name);
                }
                free(type);
            }
        }
    }
    return ret;
}

/** 
 * Save all our data to our db file
 * First walk through the nick CORE list, and any nick core which has
 * oper info attached to it, write to the file.
 * Next do the same again for ChannelInfos
 * @return 0 for success
 **/
int mSaveData(int argc, char **argv)
{
    ChannelInfo *ci = NULL;
    NickCore *nc = NULL;
    int i = 0;
    int ret = 0;
    FILE *out;

    if (argc >= 1) {
        if (!stricmp(argv[0], EVENT_START)) {
            alog("os_info: Saving the databases has started!");
            if ((out = fopen(OSInfoDBName, "w")) == NULL) {
                alog("os_info: ERROR: can not open the database file!");
                anope_cmd_global(s_OperServ,
                                 "os_info: ERROR: can not open the database file!");
                ret = 1;
            } else {
                for (i = 0; i < 1024; i++) {
                    for (nc = nclists[i]; nc; nc = nc->next) {
                        /* If we have any info on this user */
                        if (moduleGetData(&nc->moduleData, "info")) {
                            fprintf(out, "N %s %s\n", nc->display,
                                    moduleGetData(&nc->moduleData,
                                                  "info"));
                        }
                    }
                }


                for (i = 0; i < 256; i++) {
                    for (ci = chanlists[i]; ci; ci = ci->next) {
                        /* If we have any info on this channel */
                        if (moduleGetData(&ci->moduleData, "info")) {
                            fprintf(out, "C %s %s\n", ci->name,
                                    moduleGetData(&ci->moduleData,
                                                  "info"));
                        }
                    }
                }
                fclose(out);
            }
        } else {
            alog("os_info: Saving the databases is complete");
            ret = 0;
        }
    }

    return ret;
}

/** 
 * Load the configuration directives from Services configuration file.
 * @return 0 for success
 **/
int mLoadConfig(void)
{
    char *tmp = NULL;

    Directive directivas[] = {
        {"OSInfoDBName", {{PARAM_SET, PARAM_RELOAD, &tmp}}},
    };

    Directive *d = &directivas[0];
    moduleGetConfigDirective(d);

    if (OSInfoDBName)
        free(OSInfoDBName);

    if (tmp) {
        OSInfoDBName = tmp;
    } else {
        OSInfoDBName = sstrdup(DEFAULT_DB_NAME);
        alog("os_info: OSInfoDBName is not defined in Services configuration file, using default %s", OSInfoDBName);
    }

    alog("os_info: Directive OSInfoDBName loaded (%s)...", OSInfoDBName);

    return 0;
}

/** 
 * Manage the RELOAD EVENT
 * @return MOD_CONT
 **/
int mEventReload(int argc, char **argv)
{
    int ret = 0;
    if (argc >= 1) {
        if (!stricmp(argv[0], EVENT_START)) {
            alog("os_info: Reloading configuration directives...");
            ret = mLoadConfig();
        } else {
            /* Nothing for now */
        }
    }

    if (ret)
        alog("os_info.c: ERROR: An error has occured while reloading the configuration file");

    return MOD_CONT;
}

/*************************************************************************/

/**
 * manages the multilanguage stuff
 **/
void m_AddLanguages(void)
{
    char *langtable_en_us[] = {
        /* OINFO_SYNTAX */
        "Syntax: OINFO [ADD|DEL] nick <info>",
        /* OINFO_ADD_SUCCESS */
        "OperInfo line has been added to nick %s",
        /* OINFO_DEL_SUCCESS */
        "OperInfo line has been removed from nick %s",
        /* OCINFO_SYNTAX */
        "Syntax: OINFO [ADD|DEL] chan <info>",
        /* OCINFO_ADD_SUCCESS */
        "OperInfo line has been added to channel %s",
        /* OCINFO_DEL_SUCCESS */
        "OperInfo line has been removed from channel %s",
        /* OINFO_HELP_1 */
        "Syntax: OINFO [ADD|DEL] nick <info>",
        /* OINFO_HELP_2 */
        "Add or Delete Oper information for the given nick ",
        /* OINFO_HELP_3 */
        "This will show up when any oper /ns info nick's the user.",
        /* OINFO_HELP_4 */
        "and can be used for 'tagging' users etc....",
        /* OCINFO_HELP_1 */
        "Syntax: OINFO [ADD|DEL] chan <info>",
        /* OCINFO_HELP_2 */
        "Add or Delete Oper information for the given channel",
        /* OCINFO_HELP_3 */
        "This will show up when any oper /ns info's  the channel.",
        /* OCINFO_HELP_4 */
        "and can be used for 'tagging' channels etc....",
        /* OINFO_HELP */
        "    OINFO         Add / Del an OperInfo line to a nick",
        /* OCINFO_HELP */
        "    OINFO         Add / Del an OperInfo line to a channel"
    };

    char *langtable_es[] = {
        /* OINFO_SYNTAX */
        "Sintaxis: OINFO [ADD|DEL] nick <info>",
        /* OINFO_ADD_SUCCESS */
        "Una linea OperInfo ha sido agregada al nick %s",
        /* OINFO_DEL_SUCCESS */
        "La linea OperInfo ha sido removida del nick %s",
        /* OCINFO_SYNTAX */
        "Sintaxis: OINFO [ADD|DEL] chan <info>",
        /* OCINFO_ADD_SUCCESS */
        "Linea OperInfo ha sido agregada al canal %s",
        /* OCINFO_DEL_SUCCESS */
        "La linea OperInfo ha sido removida del canal %s",
        /* OINFO_HELP_1 */
        "Sintaxis: OINFO [ADD|DEL] nick <info>",
        /* OINFO_HELP_2 */
        "Agrega o elimina informacion para Operadores al nick dado",
        /* OINFO_HELP_3 */
        "Esto se mostrara cuando cualquier operador haga /ns info nick",
        /* OINFO_HELP_4 */
        "y puede ser usado para 'marcado' de usuarios, etc....",
        /* OCINFO_HELP_1 */
        "Sintaxis: OINFO [ADD|DEL] chan <info>",
        /* OCINFO_HELP_2 */
        "Agrega o elimina informacion para Operadores al canal dado",
        /* OCINFO_HELP_3 */
        "Esto se mostrara cuando cualquier operador haga /cs info canal",
        /* OCINFO_HELP_4 */
        "y puede ser usado para 'marcado' de canales, etc....",
        /* OINFO_HELP */
        "    OINFO         Agrega / Elimina una linea OperInfo al nick",
        /* OCINFO_HELP */
        "    OINFO         Agrega / Elimina una linea OperInfo al canal"
    };

    moduleInsertLanguage(LANG_EN_US, LANG_NUM_STRINGS, langtable_en_us);
    moduleInsertLanguage(LANG_ES, LANG_NUM_STRINGS, langtable_es);
}

/*************************************************************************/

int mNickHelp(User * u)
{
    if (is_oper(u)) {
        moduleNoticeLang(s_NickServ, u, OINFO_HELP_1);
        moduleNoticeLang(s_NickServ, u, OINFO_HELP_2);
        moduleNoticeLang(s_NickServ, u, OINFO_HELP_3);
        moduleNoticeLang(s_NickServ, u, OINFO_HELP_4);
    } else {
        notice_lang(s_NickServ, u, NO_HELP_AVAILABLE, "OINFO");
    }
    return MOD_CONT;
}

int mChanHelp(User * u)
{
    if (is_oper(u)) {
        moduleNoticeLang(s_ChanServ, u, OCINFO_HELP_1);
        moduleNoticeLang(s_ChanServ, u, OCINFO_HELP_2);
        moduleNoticeLang(s_ChanServ, u, OCINFO_HELP_3);
        moduleNoticeLang(s_ChanServ, u, OCINFO_HELP_4);
    } else {
        notice_lang(s_ChanServ, u, NO_HELP_AVAILABLE, "OINFO");
    }
    return MOD_CONT;
}

/* This help will be added to the main NickServ list */
void mMainNickHelp(User * u)
{
    if (is_oper(u)) {
        moduleNoticeLang(s_NickServ, u, OINFO_HELP);
    }
}

/* This help will be added to the main NickServ list */
void mMainChanHelp(User * u)
{
    if (is_oper(u)) {
        moduleNoticeLang(s_ChanServ, u, OCINFO_HELP);
    }
}

/*************************************************************************/

/* EOF */
