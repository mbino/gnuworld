/**
 * sqlIncompleteChannel.cc
 *
 * A channel application that is still being filled in on IRC.
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

#include "misc.h"
#include "sqlIncompleteChannel.h"

namespace gnuworld {
using std::ends;
using std::stringstream;

sqlIncompleteChannel::sqlIncompleteChannel(dbHandle* _SQLDb)
    : chanId(0), chanName(), RealName(), Description(), SQLDb(_SQLDb) {}

bool sqlIncompleteChannel::commitRealName() {
    stringstream queryString;
    queryString << "UPDATE pending SET managername = '" << escapeSQLChars(RealName)
                << "' WHERE channel_id = " << chanId << ends;
    return SQLDb->Exec(queryString);
}

bool sqlIncompleteChannel::commitDesc() {
    stringstream queryString;
    queryString << "UPDATE pending SET description = '" << escapeSQLChars(Description)
                << "' WHERE channel_id = " << chanId << ends;
    return SQLDb->Exec(queryString);
}

bool sqlIncompleteChannel::commitSupporters() {
    stringstream queryString;
    queryString << "DELETE FROM supporters WHERE channel_id = " << chanId << ends;
    if (!SQLDb->Exec(queryString))
        return false;

    for (supporterListType::iterator itr = supps.begin(); itr != supps.end(); ++itr) {
        queryString.str("");
        queryString << "INSERT INTO supporters (channel_id,user_id,reason,last_updated) VALUES ("
                    << chanId << ", " << itr->first
                    << ", '', date_part('epoch', CURRENT_TIMESTAMP)::int)" << ends;
        if (!SQLDb->Exec(queryString))
            return false;
    }
    return true;
}

bool sqlIncompleteChannel::commitNewPending(unsigned int mngrId, unsigned int newChanId) {
    stringstream theQuery;
    theQuery << "INSERT INTO pending (channel_id,manager_id,created_ts,decision_ts,decision,"
             << "comments,description,managername,last_updated,reg_acknowledged,check_start_ts) "
             << "VALUES (" << newChanId << "," << mngrId
             << ",date_part('epoch', CURRENT_TIMESTAMP)::int,0,'','','','',"
             << "date_part('epoch', CURRENT_TIMESTAMP)::int,'N',0)" << ends;
    return SQLDb->Exec(theQuery);
}

} // namespace gnuworld
