/**
 * sqlIncompleteChannel.h
 *
 * A channel application that is still being filled in on IRC
 * (REGISTER <#channel> REALNAME/DESCRIPTION/SUPPORTERS).
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

#ifndef __SQLINCOMPLETECHANNEL_H
#define __SQLINCOMPLETECHANNEL_H

#include <map>
#include <string>

#include "dbHandle.h"
#include "sqlUser.h"

namespace gnuworld {

class sqlIncompleteChannel {
  public:
    sqlIncompleteChannel(dbHandle*);

    /* Store the applicant's real name (pending.managername). */
    bool commitRealName();
    /* Store the channel description (pending.description). */
    bool commitDesc();
    /* Replace the supporters of this application with supps. */
    bool commitSupporters();
    /* Create the pending row (status 0, incoming) for this application. */
    bool commitNewPending(unsigned int mngrId, unsigned int chanId);

    unsigned int chanId;
    std::string chanName;
    std::string RealName;
    std::string Description;
    typedef std::map<unsigned int, sqlUser*> supporterListType;
    supporterListType supps;

    dbHandle* SQLDb;
};

} // namespace gnuworld

#endif // __SQLINCOMPLETECHANNEL_H
