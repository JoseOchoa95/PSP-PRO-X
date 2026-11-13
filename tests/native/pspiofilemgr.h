/* Host-only PSP I/O mocks. */
#ifndef TEST_IO_H
#define TEST_IO_H
typedef struct { int unused; } SceIoStat;
#define PSP_O_RDONLY 1
#define PSP_O_WRONLY 2
#define PSP_O_CREAT 0x200
#define PSP_O_TRUNC 0x400
int sceIoOpen(const char *, int, int);
int sceIoRead(int, void *, unsigned int);
int sceIoWrite(int, const void *, unsigned int);
int sceIoClose(int);
int sceIoRemove(const char *);
int sceIoRename(const char *, const char *);
int sceIoSync(const char *, int);
int sceIoGetstat(const char *, SceIoStat *);
#endif
