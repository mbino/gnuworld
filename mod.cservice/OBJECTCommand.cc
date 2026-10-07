/**
 * OBJECTCommand.cc
 *
 * OBJECT <#channel> <reason>
 * Post an objection to an open channel application.
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

#include <sstream>
#include <string>

#include "StringTokenizer.h"
#include "cservice.h"
#include "dbHandle.h"
#include "responses.h"

namespace gnuworld {
using std::ends;
using std::string;
using std::stringstream;

bool OBJECTCommand::Exec(iClient* theClient, const string& Message) {
    StringTokenizer st(Message);
    if (st.size() < 3) {
        Usage(theClient);
        return true;
    }

    sqlUser* theUser = bot->isAuthed(theClient, true);
    if (!theUser)
        return false;

    string comment = st.assemble(2);
    if (comment.size() < 2 || comment.size() > 450) {
        bot->Notice(theClient, bot->getResponse(theUser, language::reason_must).c_str(), 2, 450);
        return true;
    }

    cservice::openApplication app;
    if (!bot->findOpenApplication(st[1], app)) {
        bot->Notice(theClient, "%s is not in my list of open applications.", st[1].c_str());
        return true;
    }
    if (app.managerId == theUser->getID()) {
        bot->Notice(theClient, "You can't object to your own application.");
        return true;
    }

    stringstream theQuery;
    theQuery << "SELECT 1 FROM objections WHERE admin_only = 'N' AND channel_id = " << app.chanId
             << " AND user_id = " << theUser->getID() << ends;
    if (!bot->SQLDb->Exec(theQuery, true)) {
        LOGSQL_ERROR(bot->SQLDb);
        return false;
    }
    if (bot->SQLDb->Tuples() > 0) {
        bot->Notice(theClient, "You have already posted an objection to %s.", app.chanName.c_str());
        return true;
    }

    theQuery.str("");
    theQuery << "INSERT INTO objections (channel_id,user_id,comment,created_ts,admin_only) VALUES ("
             << app.chanId << "," << theUser->getID() << ",'" << escapeSQLChars(comment)
             << "',date_part('epoch', CURRENT_TIMESTAMP)::int,'N')" << ends;
    if (!bot->SQLDb->Exec(theQuery)) {
        LOGSQL_ERROR(bot->SQLDb);
        bot->Notice(theClient, "Your objection could not be stored, please try again.");
        return false;
    }
    bot->Notice(theClient, "Your objection to %s has been posted.", app.chanName.c_str());
    return true;
}

} // namespace gnuworld
