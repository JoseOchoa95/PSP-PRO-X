/* Native executable for production JSON/contract/staging rejection tests. */
#include "contract.h"
#include "policy.h"
#include "stage.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
static int stop_after, calls;
int prox_stopping(void) { return stop_after && ++calls >= stop_after; }
void prox_log(const char *format, ...) { (void)format; }
static int read_json(Pj *j, const char *name, char *buffer, PjToken *tokens)
{
    FILE *f=fopen(name,"rb"); size_t size;
    if (!f) return -1;
    size=fread(buffer,1,256*1024+1,f);
    if (ferror(f) || fclose(f) || size > 256*1024) return -1;
    return pj_parse(j,buffer,size,tokens,8192);
}
int main(int argc, char **argv)
{
    char *buffer=malloc(256*1024+1); PjToken *tokens=calloc(8192,sizeof(*tokens)); Pj j;
    unsigned int running[3]={0,3,0},client[3]={0,3,0};
    ProxRelease release; ProxManifest manifest; int rc=-1;
    if (!buffer || !tokens || argc < 3) goto done;
    {
        const char *source=getenv("PROX_FIXTURE_RUNNING_VERSION");
        if (source && prox_version(source,running)) goto done;
    }
    if (read_json(&j,argv[2],buffer,tokens)) goto done;
    if (!strcmp(argv[1],"json")) { rc=0; goto done; }
    rc=prox_release(&j,0,running,&release);
    if (rc != 1) { rc=rc == 0 ? 2 : 1; goto done; }
    rc=-1;
    if (argc < 4 || read_json(&j,argv[3],buffer,tokens) || prox_manifest(&j,&release,running,client,1,&manifest)) goto done;
    if (!strcmp(argv[1],"contract")) { rc=0; goto done; }
    if (argc < 6) goto done;
    if (argc == 7) stop_after=atoi(argv[6]);
    if (prox_stage_dirs(argv[5])) goto done;
    rc=prox_zip_stage(argv[4],argv[5],&manifest);
    if (!rc) rc=prox_stage_check(argv[5],&manifest);
done:
    printf("RESULT %d\n",rc);
    free(tokens); free(buffer); return rc == 0 ? 0 : rc == 2 ? 2 : 1;
}
