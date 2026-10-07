/**
 * SCANCommand.cc
 *
 * Find the account a nickname is reserved for (nick protection).
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
 *
 */

#include <string>

#include "StringTokenizer.h"
#include "cservice.h"
#include "responses.h"

namespace gnuworld {
using std::string;

bool SCANCommand::Exec(iClient* theClient, const string& Message) {
    StringTokenizer st(Message);
    if (st.size() < 3) {
        Usage(theClient);
        return true;
    }

    sqlUser* theUser = bot->isAuthed(theClient, true);
    if (!theUser) {
        return false;
    }

    string option = string_upper(st[1]);
    if (option != "NICK" && option != "NICKNAME") {
        Usage(theClient);
        return true;
    }

    string nickOwner = bot->NickIsRegisteredTo(st[2]);
    if (!nickOwner.empty())
        bot->Notice(theClient, "Nickname %s is registered to %s", st[2].c_str(),
                    nickOwner.c_str());
    else
        bot->Notice(theClient, bot->getResponse(theUser, language::no_match).c_str());

    return true;
}

} // namespace gnuworld
