/* GPL-3.0-or-later */
#include <assert.h>

#include "media_session.h"

int main(void)
{
    enum MsMediaSession session = MS_SESSION_UNCHECKED;
    ms_session_notify(&session);
    assert(session == MS_SESSION_UNCHECKED);
    assert(!ms_session_mark(&session, 0, 1));
    assert(!ms_session_mark(&session, 1, 0));
    assert(!ms_session_mark(&session, 1, -1));
    assert(ms_session_mark(&session, 1, 1));
    ms_session_observe(&session, 1);
    assert(session == MS_SESSION_CURRENT);

    /* A notification also invalidates a fast swap that polling missed. */
    ms_session_notify(&session);
    ms_session_observe(&session, 1);
    assert(session == MS_SESSION_INVALIDATED);
    assert(!ms_session_mark(&session, 0, 1));
    assert(session == MS_SESSION_INVALIDATED);

    assert(ms_session_mark(&session, 1, 1));
    ms_session_observe(&session, 0);
    ms_session_observe(&session, 1);
    assert(session == MS_SESSION_INVALIDATED);
    assert(ms_session_mark(&session, 1, 1));
    ms_session_observe(&session, -1);
    assert(session == MS_SESSION_INVALIDATED);
    return 0;
}
