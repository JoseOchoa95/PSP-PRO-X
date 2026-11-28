/* GPL-3.0-or-later */
#ifndef PROX_MEDIA_SESSION_H
#define PROX_MEDIA_SESSION_H

enum MsMediaSession {
    MS_SESSION_UNCHECKED,
    MS_SESSION_CURRENT,
    MS_SESSION_INVALIDATED,
};

int ms_session_mark(enum MsMediaSession *session, int monitoring, int presence);
void ms_session_observe(enum MsMediaSession *session, int presence);
void ms_session_notify(enum MsMediaSession *session);

#endif
