#include <stdlib.h>
#include "vcl.h"
#include "vrt.h"
#include "bin/varnishd/cache.h"
#include <time.h>

#include <arpa/inet.h>
#include <syslog.h>
#include <poll.h>
	#include <fcntl.h>
	#include <sys/mman.h>
	#include <sys/types.h>
#include <stdio.h>

#include "vcc_if.h"




int
init_function(struct vmod_priv *priv, const struct VCL_conf *conf)
{
	return (0);
}

const char *
vmod_gethash(struct sess *sp){
	int size = DIGEST_LEN*2+1;
	if(WS_Reserve(sp->wrk->ws, 0) < size){
		WS_Release(sp->wrk->ws, 0);
		return NULL;
	}
	
	char *tmp = (char *)sp->wrk->ws->f;
	for(int i = 0; i < DIGEST_LEN; i++)
		sprintf(tmp + 2 * i, "%02x",sp->digest[i]);
	WS_Release(sp->wrk->ws, size);
	return tmp;
}
double vmod_timeoffset(struct sess *sp, double time ,double os ,unsigned rev){
	if(rev){ os*=-1; }
	return time+os;
}

double vmod_timecmp(struct sess *sp,double time1,double time2){
	return(time1-time2);
}

struct sockaddr_storage * vmod_inet_pton(struct sess *sp,unsigned ipv6,const char *str,const char *defaultstr){
	int size = sizeof(struct sockaddr_storage);
	if(WS_Reserve(sp->wrk->ws, 0) < size){
		WS_Release(sp->wrk->ws, 0);
		return NULL;
	}
	struct sockaddr_storage *tmp = (char *)sp->wrk->ws->f;
	void *paddr;
	int ret = 0;
	int af;
	if(ipv6){
		tmp->ss_family = AF_INET6;
		paddr = &((struct sockaddr_in6 *)tmp)->sin6_addr;
		af = AF_INET6;
	}else{
		tmp->ss_family = AF_INET;
		paddr = &((struct sockaddr_in *)tmp)->sin_addr;
		af = AF_INET;
	}

	if(str != NULL){
		ret = inet_pton(af , str , paddr);
	}
	
	if(ret < 1){
		if(defaultstr == NULL){
			ret=inet_pton(af , "(:3[__])" , paddr);
		}else{
			ret=inet_pton(af , defaultstr , paddr);
		}
	}
	WS_Release(sp->wrk->ws, size);
	return tmp;
	
}

const char*
vmod_postcapture(struct sess *sp){


	unsigned long content_length,orig_content_length;
	char *h_clen_ptr, *h_ctype_ptr;
	int buf_size, rsize;
	char buf[1024];
	

	h_ctype_ptr = VRT_GetHdr(sp, HDR_REQ, "\015Content-Type:");
	if (strcmp(h_ctype_ptr, "application/x-www-form-urlencoded")) {
		return"";
	}
	h_clen_ptr = VRT_GetHdr(sp, HDR_REQ, "\017Content-Length:");
	if (!h_clen_ptr) {
		return"";
	}
	orig_content_length = content_length = strtoul(h_clen_ptr, NULL, 10);
	
	int maxlen = params->http_req_hdr_len + 1;
	
	///////////////////////////////////////////////
	int u = WS_Reserve(sp->wrk->ws, 0);
	if(u < content_length + 1){
		return"";
	}
	char *newbuffer = (char*)sp->wrk->ws->f;
	newbuffer[0] = 0;
	WS_Release(sp->wrk->ws,content_length + 1);
	///////////////////////////////////////////////

	//
	if (content_length <= 0) {
		return"";
	}
		
		char *amp;
		char *eq;
		int  hsize = 0;int  eqsize = 0;
		char head[3];
		while (content_length) {
			if (content_length > sizeof(buf)) {
				buf_size = sizeof(buf) - 1;
			}
			else {
				buf_size = content_length;
			}

			// read body data into 'buf'
			rsize = HTC_Read(sp->htc, buf, buf_size);

			if (rsize <= 0) {
				return"";
			}

			hsize += rsize;
			content_length -= rsize;

			strncat(newbuffer, buf, buf_size);

	}

	
	sp->htc->pipeline.b = newbuffer;
	sp->htc->pipeline.e = newbuffer + orig_content_length;

	
	
	return newbuffer;
}

/*
double vmod_strptime(struct sess *sp,const char *str,const char *format){
	struct tm t;
	
	t.tm_year = 0;
	t.tm_mon  = 1;
	t.tm_mday = 1;
	t.tm_wday = 0;
	t.tm_hour = 0;
	t.tm_min  = 0;
	t.tm_sec  = 0;
	t.tm_isdst= -1;
	
	if(NULL==strptime(str, format, &t))
		return (double)mktime(&t);

	return 0;
}
*/
