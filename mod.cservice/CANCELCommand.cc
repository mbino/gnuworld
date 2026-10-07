/**
 * CANCELCommand.cc
 *
 * CANCEL <#channel> YES
 * The applicant withdraws their own open channel application.
 * From Seven's gnuworld-enhanced, ported for ChatBox.nu.
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
 */

#include <string>

#include "StringTokenizer.h"
#include "cservice.h"
#include "sqlChannel.h"

namespace gnuworld {
using std::string;

bool CANCELCommand::Exec(iClient* theClient, const string& Message) {
    StringTokenizer st(Message);
    if (st.size() < 2) {
        Usage(theClient);
        return true;
    }

    sqlUser* theUser = bot->isAuthed(theClient, true);
    if (!theUser)
        return false;

    cservice::openApplication app;
    if (!bot->findOpenApplication(st[1], app) || app.managerId != theUser->getID()) {
        bot->Notice(theClient, "You have no open application for %s.", st[1].c_str());
        return true;
    }

    if (st.size() < 3 || string_upper(st[2]) != "YES") {
        bot->Notice(theClient, "To really cancel your application: /msg %s CANCEL %s YES",
                    bot->getNickName().c_str(), app.chanName.c_str());
        return true;
    }

    if (!bot->closeApplication(app, 4, "Cancelled by applicant", 0)) {
        bot->Notice(theClient, "Could not cancel your application, please try again.");
        return false;
    }

    sqlChannel logChan(bot);
    logChan.setID(app.chanId);
    logChan.setName(app.chanName);
    bot->writeChannelLog(&logChan, theClient, sqlChannel::EV_WITHDRAW,
                         "by " + theUser->getUserName());
    bot->Notice(theClient, "Your application for %s has been cancelled.", app.chanName.c_str());
    return true;
}

} // namespace gnuworld
