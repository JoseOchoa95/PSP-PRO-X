/* Paths for the opt-in resident query test. GPL-3.0-or-later. */
#ifndef PROX_RESIDENT_QUERY_H
#define PROX_RESIDENT_QUERY_H

#define PROX_RESIDENT_WORKER_ARG "PROX-RESIDENT-FLASH0-V1"
#define PROX_RESIDENT_QUERY_ARG "--prox-native-query-v3"
#define PROX_RESIDENT_MS_REPORTS "ms0:/PROXDIAG"
#define PROX_RESIDENT_EF_REPORTS "ef0:/PROXDIAG"

/* A report is optional. These paths never supply executable data or trust. */
static inline const char *prox_resident_report_directory(void)
{
    SceIoStat stat;

    if (sceIoMkdir(PROX_RESIDENT_MS_REPORTS, 0777) >= 0 ||
        (sceIoGetstat(PROX_RESIDENT_MS_REPORTS, &stat) >= 0 && FIO_S_ISDIR(stat.st_mode)))
        return PROX_RESIDENT_MS_REPORTS;
    if (sceIoMkdir(PROX_RESIDENT_EF_REPORTS, 0777) >= 0 ||
        (sceIoGetstat(PROX_RESIDENT_EF_REPORTS, &stat) >= 0 && FIO_S_ISDIR(stat.st_mode)))
        return PROX_RESIDENT_EF_REPORTS;
    return NULL;
}

#endif
