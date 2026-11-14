#ifndef PROX_OTA_INSTALLER_H
#define PROX_OTA_INSTALLER_H
/* 0 manual install, 1 verified OTA request, -1 refuse before flash setup. */
int prox_ota_begin(int argc,char **argv,unsigned int firmware,unsigned int model);
int prox_ota_before_write(void);
int prox_ota_complete(void);
void prox_ota_result(int result);
void prox_ota_close(void);
int prox_ota_reboot_path(char path[256]);
int prox_ota_cleanup_modules(void);
#endif
