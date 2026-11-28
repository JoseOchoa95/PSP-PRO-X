/* GPL-3.0-or-later */
#include "media_session.h"

int ms_session_mark(enum MsMediaSession *session, int monitoring, int presence)
{
    if (!monitoring || presence != 1)
        return 0;
    *session = MS_SESSION_CURRENT;
    return 1;
}

void ms_session_notify(enum MsMediaSession *session)
{
    if (*session == MS_SESSION_CURRENT)
        *session = MS_SESSION_INVALIDATED;
}

void ms_session_observe(enum MsMediaSession *session, int presence)
{
    if (presence != 1)
        ms_session_notify(session);
}
