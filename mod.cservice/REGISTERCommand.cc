/**
 * REGISTERCommand.cc
 *
 * 26/12/2000 - Greg Sikorski <gte@atomicrevs.demon.co.uk>
 * Initial Version.
 *
 * Registers a channel.
 *
 * ChatBox.nu: normal users can also apply on IRC when supporters are required
 * (REGISTER <#channel> REALNAME/DESCRIPTION/SUPPORTERS), from Seven's gnuworld-enhanced.
 *
 * Caveats: None
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 *
 * $Id: REGISTERCommand.cc,v 1.24 2009/07/31 07:29:13 mrbean_ Exp $
 */
#include <cassert>
#include <map>
#include <string>
#include <sstream>
#include <iostream>
#include "StringTokenizer.h"
#include "ELog.h"
#include "cservice.h"
#include "levels.h"
#include "dbHandle.h"
#include "Network.h"
#include "responses.h"

#include "sqlChannel.h"
#include "sqlIncompleteChannel.h"

namespace gnuworld {
using std::endl;
using std::ends;
using std::pair;
using std::string;
using std::stringstream;

using namespace gnuworld;


namespace {
/* Tell the applicant what is still missing in an application being filled in on IRC. */
void showAppProgress(cservice* bot, iClient* theClient, sqlIncompleteChannel* app) {
    const char* x = bot->getNickName().c_str();
    const char* chan = app->chanName.c_str();
    bot->Notice(theClient, "You have an incomplete channel application for %s.", chan);
    if (!app->RealName.empty())
        bot->Notice(theClient, "Your real name: %s", app->RealName.c_str());
    if (!app->Description.empty())
        bot->Notice(theClient, "Description: %s", app->Description.c_str());

    if (app->RealName.empty())
        bot->Notice(theClient, "Now give your real name: /msg %s REGISTER %s REALNAME <your real name>",
                    x, chan);
    else if (app->Description.empty())
        bot->Notice(theClient,
                    "Now describe your channel: /msg %s REGISTER %s DESCRIPTION <description>", x,
                    chan);
    else
        bot->Notice(theClient,
                    "Now name your %u supporters: /msg %s REGISTER %s SUPPORTERS <user1 user2 ...>",
                    bot->getConfRequiredSupporters(), x, chan);
    bot->Notice(theClient, "To cancel your application: /msg %s CANCEL %s YES", x, chan);
}

/* Explain a failed isValidApplicant() check. */
void explainApplicant(cservice* bot, iClient* theClient, bool forOther) {
    const string& why = bot->getCurrentValidResponse();
    if (why == "TOO_NEW")
        bot->Notice(theClient,
                    "%s username must be at least %u days old to apply for a channel.",
                    forOther ? "The target" : "Your", bot->getConfMinDaysBeforeReg());
    else if (why == "ALREADY_HAVE_CHAN")
        bot->Notice(theClient, "%s already a channel registered. Only ONE channel per user.",
                    forOther ? "The target user has" : "You have");
    else if (why == "ALREADY_HAVE_PENDINGCHAN")
        bot->Notice(theClient, "%s already a channel application pending. Only ONE at a time.",
                    forOther ? "The target user has" : "You have");
    else if (!why.empty())
        bot->Notice(theClient, "%s", why.c_str());
    bot->validResponseString.clear();
}
} // namespace

bool REGISTERCommand::Exec(iClient* theClient, const string& Message) {
    StringTokenizer st(Message);

    /*
     *  Fetch the sqlUser record attached to this client. If there isn't one,
     *  they aren't logged in - tell them they should be.
     */
    sqlUser* theUser = bot->isAuthed(theClient, true);
    if (!theUser)
        return false;

    if (st.size() < 2) {
        Usage(theClient);
        return true;
    }

    string::size_type pos = st[1].find_first_of(','); /* Don't allow comma's in channel names. :) */
    if ((st[1][0] != '#') || (string::npos != pos)) {
        bot->Notice(theClient, bot->getResponse(theUser, language::inval_chan_name,
                                                string("Invalid channel name.")));
        return false;
    }

    if (bot->getChannelRecord(st[1])) {
        bot->Notice(theClient,
                    bot->getResponse(theUser, language::chan_already_reg,
                                     string("%s is already registered with me."))
                        .c_str(),
                    st[1].c_str());
        return false;
    }

    int level = bot->getAdminAccessLevel(theUser);
    cservice::incompleteChanRegsType::iterator theApp =
        bot->incompleteChanRegs.find(theUser->getID());
    sqlIncompleteChannel* chanApp =
        (theApp != bot->incompleteChanRegs.end()) ? theApp->second : 0;

    /*
     * Admin registration: REGISTER <#channel> <username>, as in Undernet's GNUworld.
     */
    if ((st.size() > 2) && !chanApp) {
        if (level < level::registercmd) {
            bot->Notice(theClient,
                        bot->getResponse(theUser, language::insuf_access,
                                         string("You have insufficient access to perform that "
                                                "command.")));
            return false;
        }
        sqlUser* tmpUser = bot->getUserRecord(st[2]);
        if (!tmpUser) {
            bot->Notice(theClient,
                        bot->getResponse(theUser, language::not_registered,
                                         string("The user %s doesn't appear to be registered."))
                            .c_str(),
                        st[2].c_str());
            return true;
        }
        if (!bot->isValidChannel(st[1])) {
            if (!bot->validResponseString.empty()) {
                bot->Notice(theClient, "Cannot register, channel is %s",
                            bot->getCurrentValidResponse().c_str());
                bot->validResponseString.clear();
            }
            return false;
        }
        return bot->sqlRegisterChannel(theClient, tmpUser, st[1]);
    }

    /*
     * The user's own application.
     */
    if (chanApp && string_lower(chanApp->chanName) != string_lower(st[1])) {
        showAppProgress(bot, theClient, chanApp);
        return false;
    }

    if (!chanApp) {
        if (!bot->isValidChannel(st[1])) {
            if (!bot->validResponseString.empty()) {
                bot->Notice(theClient, "Cannot register, channel is %s",
                            bot->getCurrentValidResponse().c_str());
                bot->validResponseString.clear();
            }
            return false;
        }
        if (!bot->isValidUser(theUser->getUserName())) {
            if (!bot->validResponseString.empty()) {
                bot->Notice(theClient, "Cannot register, your username is invalid (%s)",
                            bot->getCurrentValidResponse().c_str());
                bot->validResponseString.clear();
            }
            return false;
        }
        if (!bot->isValidApplicant(theUser)) {
            explainApplicant(bot, theClient, false);
            return false;
        }

        /* No supporters needed: register right away. */
        if (bot->getConfRequiredSupporters() == 0)
            return bot->sqlRegisterChannel(theClient, theUser, st[1]);

        /* Otherwise start an application: reuse an unregistered channel record, or add one. */
        unsigned int chanId = bot->getPendingChanId(st[1]);
        stringstream theQuery;
        if (chanId) {
            bot->wipeChannel(chanId);
            theQuery << "UPDATE channels SET name = '" << escapeSQLChars(st[1])
                     << "', mass_deop_pro=0, flood_pro=0, flags=0, limit_offset=3, "
                     << "limit_period=20, limit_grace=1, limit_max=0, userflags=0, "
                     << "url='', description='', keywords='', registered_ts=0, channel_ts=0, "
                     << "channel_mode='', comment='', "
                     << "last_updated=date_part('epoch', CURRENT_TIMESTAMP)::int WHERE id = "
                     << chanId << ends;
        } else {
            theQuery << "INSERT INTO channels (name,url,description,keywords,registered_ts,"
                     << "channel_ts,channel_mode,comment,last_updated,mass_deop_pro,flood_pro,"
                     << "flags,limit_offset,limit_period,limit_grace,limit_max,userflags) VALUES ('"
                     << escapeSQLChars(st[1])
                     << "','','','',0,0,'','',date_part('epoch', CURRENT_TIMESTAMP)::int,"
                     << "0,0,0,3,20,1,0,0)" << ends;
        }
        if (!bot->SQLDb->Exec(theQuery)) {
            LOGSQL_ERROR(bot->SQLDb);
            bot->Notice(theClient, "Sorry, your application could not be stored. Please try again later.");
            return false;
        }
        if (!chanId)
            chanId = bot->getPendingChanId(st[1]);

        sqlIncompleteChannel* newApp = new (std::nothrow) sqlIncompleteChannel(bot->SQLDb);
        assert(newApp != 0);
        newApp->chanName = st[1];
        newApp->chanId = chanId;
        if (!chanId || !newApp->commitNewPending(theUser->getID(), chanId)) {
            delete newApp;
            bot->Notice(theClient, "Sorry, your application could not be stored. Please try again later.");
            return false;
        }
        bot->incompleteChanRegs.insert(
            cservice::incompleteChanRegsType::value_type(theUser->getID(), newApp));
        showAppProgress(bot, theClient, newApp);
        return true;
    }

    /*
     * Filling in the application: REALNAME, DESCRIPTION, SUPPORTERS.
     */
    string option = (st.size() > 2) ? string_upper(st[2]) : string();
    if (option == "DESC")
        option = "DESCRIPTION";

    if (option == "REALNAME") {
        string mngrName = st.assemble(3);
        if (mngrName.size() < 2 || mngrName.size() > 80) {
            bot->Notice(theClient, "Your real name must be 2 to 80 characters long.");
            return true;
        }
        chanApp->RealName = mngrName;
        chanApp->commitRealName();
        showAppProgress(bot, theClient, chanApp);
        return true;
    }

    if (option == "DESCRIPTION") {
        string desc = st.assemble(3);
        if (desc.size() < 2 || desc.size() > 300) {
            bot->Notice(theClient, "Your description must be 2 to 300 characters long.");
            return true;
        }
        chanApp->Description = desc;
        chanApp->commitDesc();
        showAppProgress(bot, theClient, chanApp);
        return true;
    }

    if (option == "SUPPORTERS") {
        if (chanApp->RealName.empty() || chanApp->Description.empty()) {
            showAppProgress(bot, theClient, chanApp);
            return true;
        }
        unsigned int required = bot->getConfRequiredSupporters();
        if (st.size() - 3 != required) {
            bot->Notice(theClient, "You need to name exactly %u supporters.", required);
            return true;
        }

        sqlIncompleteChannel::supporterListType supps;
        for (unsigned int i = 3; i < st.size(); i++) {
            sqlUser* suppUser = bot->getUserRecord(st[i]);
            if (!suppUser) {
                bot->Notice(theClient,
                            "Supporter \002%s\002 does not exist. Check the spelling of the "
                            "username and try again.",
                            st[i].c_str());
                return true;
            }
            const char* suppName = suppUser->getUserName().c_str();
            if (suppUser == theUser) {
                bot->Notice(theClient, "You can't support your own application.");
                return true;
            }
            if (supps.count(suppUser->getID())) {
                bot->Notice(theClient, "Supporter \002%s\002 is named more than once.", suppName);
                return true;
            }
            if (!bot->isValidUser(suppUser->getUserName())) {
                bot->Notice(theClient, "Invalid supporter %s (%s)", suppName,
                            bot->getCurrentValidResponse().c_str());
                bot->validResponseString.clear();
                return true;
            }
            if (!bot->isValidSupporter(suppUser->getUserName())) {
                const string why = bot->getCurrentValidResponse();
                bot->validResponseString.clear();
                if (why == "NEVER_LOGGED")
                    bot->Notice(theClient,
                                "Supporter \002%s\002 has never logged in to %s on IRC.", suppName,
                                bot->getNickName().c_str());
                else if (why == "TOO_NEW")
                    bot->Notice(theClient,
                                "Supporter \002%s\002 is too new: usernames must be at least %u "
                                "days old to support a channel.",
                                suppName, bot->getConfMinDaysBeforeSupport());
                else if (why == "SEEN_LONG_AGO")
                    bot->Notice(theClient,
                                "Supporter \002%s\002 must have logged in to %s within the last "
                                "21 days.",
                                suppName, bot->getNickName().c_str());
                else if (why == "TOO_MANY_SUPPORTS")
                    bot->Notice(theClient,
                                "Supporter \002%s\002 already supports %u other applications.",
                                suppName, bot->getConfMaxConcurrentSupports());
                else
                    bot->Notice(theClient, "Supporter \002%s\002 can't support this application.",
                                suppName);
                return true;
            }
            supps.insert(sqlIncompleteChannel::supporterListType::value_type(suppUser->getID(),
                                                                            suppUser));
        }

        chanApp->supps = supps;
        if (!chanApp->commitSupporters()) {
            bot->Notice(theClient, "Sorry, your application could not be stored. Please try again later.");
            return false;
        }
        string chanName = chanApp->chanName;
        unsigned int chanId = chanApp->chanId;
        bot->removeIncompleteChanReg(theUser->getID());

        sqlChannel logChan(bot);
        logChan.setID(chanId);
        logChan.setName(chanName);
        bot->writeChannelLog(&logChan, theClient, sqlChannel::EV_NEWAPP,
                             "by " + theUser->getUserName());
        bot->logAdminMessage("New channel application %s by %s (%s)", chanName.c_str(),
                             theUser->getUserName().c_str(),
                             theClient->getRealNickUserHost().c_str());
        bot->Notice(theClient, "Your application for %s has been recorded.", chanName.c_str());
        bot->Notice(theClient,
                    "Your supporters will be asked to confirm; then the application is reviewed.");
        return true;
    }

    showAppProgress(bot, theClient, chanApp);
    return true;
}

} // namespace gnuworld.
