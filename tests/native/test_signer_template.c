/* Host-only inherited signer round trip, no PSP or network. */
#define main prox_signer_cli
#include "../../firmware/contrib/PrxEncrypter/main.c"
#undef main
#include <assert.h>
static unsigned char original[2*1024*1024],decoded[10*1024*1024],expanded[10*1024*1024];
int main(int argc,char **argv)
{
    unsigned int size=431506,i;FILE *file;z_stream stream={0};char *args[]={"signer","/tmp/prox-signer-in.prx","/tmp/prox-signer-out.prx"};
    if(argc==2) {
        file=fopen(argv[1],"rb");assert(file);size=fread(original,1,sizeof(original),file);assert(size>0 && size<sizeof(original) && !ferror(file));assert(!fclose(file));
    } else {
        for(i=0;i<size;++i)original[i]=(unsigned char)(i*13u+i/97u);
        memcpy(original,"\177ELF",4);
    }
    file=fopen(args[1],"wb");assert(file);assert(fwrite(original,1,size,file)==size);assert(!fclose(file));
    assert(!prox_signer_cli(3,args));
    assert(!memcmp(out_buffer,"~PSP",4) && is_compressed(out_buffer));
    assert(*(u32 *)(out_buffer+0x2c)<1024u*1024u);
    assert(!kirk_CMD1(decoded,kirk_enc,sizeof(kirk_enc),1)); /* CMAC checks enabled. */
    assert(inflateInit2(&stream,15+16)==Z_OK);
    stream.next_in=decoded;stream.avail_in=sizeof(decoded);stream.next_out=expanded;stream.avail_out=sizeof(expanded);
    assert(inflate(&stream,Z_FINISH)==Z_STREAM_END);
    assert(stream.total_out==(unsigned int)get_elf_size(out_buffer));
    assert(stream.total_out>=size && !memcmp(expanded,original,size));
    for(i=size;i<stream.total_out;++i)assert(!expanded[i]);
    assert(!inflateEnd(&stream));
    memset(elf,0,sizeof(elf));{ int too_large=6*1024*1024;assert(!select_header(&too_large)); }
    puts("PASS: inherited signer chooses bounded compressed template across old size threshold; checked KIRK/CMAC decrypt and exact original payload/zero-padding inflate; oversized input rejected");
    return 0;
}
