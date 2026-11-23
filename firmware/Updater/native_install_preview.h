#ifndef PROX_NATIVE_INSTALL_PREVIEW_SERVICE_H
#define PROX_NATIVE_INSTALL_PREVIEW_SERVICE_H
int prox_install_preview_download(const char *directory,ProxNativeProgress progress);
int prox_install_preview_launch(const char *directory);
#endif
