/* Host tests of untrusted network reply handling; GPL-3.0-or-later. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "protocol.h"

int main(void)
{
    unsigned char q[272], p[512], ip[4];
    int n, found, length, te, rc;
    size_t i, body;
    const char *host="example.com";
    const unsigned char http[]="HTTP/1.0 200 OK\r\nContent-Length: 1\r\n\r\np";
    n=nd_dns_query(q,sizeof(q),0x1234,host);
    assert(n==29 && q[0]==0x12 && q[1]==0x34 && q[5]==1);
    assert(nd_dns_query(q,8,0,host)==-1);
    assert(nd_dns_query(q,sizeof(q),0,"bad..name")==-1);
    n=nd_dns_query(q,sizeof(q),0x1234,host);
    memcpy(p,q,(size_t)n); p[2]=0x81; p[3]=0x80; p[7]=1;
    {
        const unsigned char rr[]={0xc0,0x0c,0,1,0,1,0,0,0,60,0,4,192,0,2,1};
        memcpy(p+n,rr,sizeof(rr)); n+=(int)sizeof(rr);
    }
    for (i=0;i<(size_t)n;++i)
        assert(nd_dns_answer(p,i,0x1234,host,ip,&found)<0);
    assert(nd_dns_answer(p,(size_t)n,0x1234,host,ip,&found)==0 && found);
    assert(ip[0]==192 && ip[3]==1);
    assert(nd_dns_answer(p,(size_t)n,0x5678,host,ip,&found)==-1);
    assert(nd_dns_answer(p,(size_t)n,0x1234,"another.com",ip,&found)==-1);
    p[2]|=2; assert(nd_dns_answer(p,(size_t)n,0x1234,host,ip,&found)==-2); p[2]&=~2;
    p[3]=0x83; assert(nd_dns_answer(p,(size_t)n,0x1234,host,ip,&found)==3 && !found);
    p[3]=0x80; p[n-6]=0xff;
    assert(nd_dns_answer(p,(size_t)n,0x1234,host,ip,&found)==-1);
    for (i=0;i<sizeof(http)-2;++i) {
        rc=nd_http_status(http,i,&body,&length,&te);
        assert(rc==0 || rc==200); /* headers may be complete before the body */
    }
    assert(nd_http_status(http,sizeof(http)-1,&body,&length,&te)==200);
    assert(length==1 && !te && body==sizeof(http)-2 && http[body]=='p');
    {
        const char *cases[]={
            "HTTP/1.0 200 OK\r\nContent-Length: 999999999999999\r\n\r\n",
            "HTTP/1.0 200 OK\r\nContent-Length: -1\r\n\r\n",
            "HTTP/1.0 200 OK\r\nContent-Length: 1\r\ncontent-length: 2\r\n\r\n",
            "HTTP/1.0 200oops\r\n\r\n",
            "<html>not an HTTP response</html>"
        };
        for (i=0;i<sizeof(cases)/sizeof(cases[0]);++i)
            assert(nd_http_status((const unsigned char *)cases[i],strlen(cases[i]),&body,&length,&te)==-1);
    }
    {
        const char *chunked="HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n1\r\np\r\n0\r\n\r\n";
        assert(nd_http_status((const unsigned char *)chunked,strlen(chunked),&body,&length,&te)==200);
        assert(te && length==-1);
    }
    puts("netdiag DNS/HTTP malformed, truncated and fragmented input: PASS");
    return 0;
}
