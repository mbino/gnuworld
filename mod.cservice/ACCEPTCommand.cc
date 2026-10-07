/**
 * ACCEPTCommand.cc
 *
 * ACCEPT <#channel> <decision>
 * Accept an open channel application from IRC: register the channel to its applicant.
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
#include "levels.h"
#include "responses.h"

namespace gnuworld {
using std::string;

bool ACCEPTCommand::Exec(iClient* theClient, const string& Message) {
    StringTokenizer st(Message);
    if (st.size() < 3) {
        Usage(theClient);
        return true;
    }

    sqlUser* theUser = bot->isAuthed(theClient, true);
    if (!theUser)
        return false;

    if (bot->getAdminAccessLevel(theUser) < level::accept) {
        bot->Notice(theClient, bot->getResponse(theUser, language::insuf_access,
                                                string("You have insufficient access to perform "
                                                       "that command.")));
        return false;
    }

    string decision = st.assemble(2);
    if (decision.size() > 300) {
        bot->Notice(theClient, "The decision can't be longer than 300 characters.");
        return true;
    }

    cservice::openApplication app;
    if (!bot->findOpenApplication(st[1], app)) {
        bot->Notice(theClient, "%s is not in my list of open applications.", st[1].c_str());
        return true;
    }

    sqlUser* manager = bot->getUserRecord(app.managerId);
    if (!manager) {
        bot->Notice(theClient, "The applicant of %s no longer exists; reject it instead.",
                    app.chanName.c_str());
        return true;
    }

    /* Registers the channel, adds the 500 manager, joins it and logs, like The Judge does. */
    if (!bot->sqlRegisterChannel(theClient, manager, app.chanName))
        return false;

    bot->closeApplication(app, 3, "by " + theUser->getUserName() + ": " + decision, theUser);
    bot->NoteAllAuthedClients(manager, "Your channel application of %s has been accepted: %s",
                              app.chanName.c_str(), decision.c_str());
    bot->Notice(theClient, "Accepted the application of %s by %s.", app.chanName.c_str(),
                manager->getUserName().c_str());
    return true;
}

} // namespace gnuworld
