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


void decodeForm(struct sess *sp,char *tg){
	char head[256];
	char *eq,*amp;
	char *tgt = tg;
	int hsize;
	char tmp;
	while(1){
		if(!(eq = strchr(tgt,'='))){
			break;
		}
		if(!(amp = strchr(tgt,'&'))){
			amp = eq + strlen(tgt);
		}
		//head build;
		tmp = eq[0];
		eq[0] = 0;// = -> null
		
		hsize = strlen(tgt) + 1;
		if(hsize > 255) return;
		head[0] = hsize;
		head[1] = 0;
		snprintf(head +1,255,"%s:",tgt);
		eq[0] = tmp;
		tgt = eq + 1;
		tmp = amp[0];
		amp[0] = 0;// & -> null
		VRT_SetHdr(sp, HDR_REQ, head, tgt, vrt_magic_string_end);
		amp[0] = tmp;
		tgt = amp + 1;
	}
}
//----MMM893
int 
vmod_postcapture(struct sess *sp,const char* target){
	
	

/*
	
	1	=成功
	-1	=エラー		ワークスペースサイズが足りない
	-2	=エラー		target/ContentLengthがない/不正
	-3	=エラー		指定ContentLengthに満たないデータ
	-4	=エラー		未対応形式
	if (!(
		!VRT_strcmp(VRT_r_req_request(sp), "POST") ||
		!VRT_strcmp(VRT_r_req_request(sp), "PUT")
	)){return "";}
*/
	unsigned long content_length,orig_content_length;
	char *h_clen_ptr, *h_ctype_ptr;
	int buf_size, rsize;
	char buf[1024];
	char head[256];

	//headbuild
	int hsize = strlen(target) +1;
	if(hsize > 255){
		return -2;
	}
	head[0] = hsize;
	head[1] = 0;
	snprintf(head +1,255,"%s:",target);
	
	h_ctype_ptr = VRT_GetHdr(sp, HDR_REQ, "\015Content-Type:");
	if (VRT_strcmp(h_ctype_ptr, "application/x-www-form-urlencoded")) {
		return -4;
	}

	h_clen_ptr = VRT_GetHdr(sp, HDR_REQ, "\017Content-Length:");
	if (!h_clen_ptr) {
		return -2;
	}
	orig_content_length = content_length = strtoul(h_clen_ptr, NULL, 10);

	if (content_length <= 0) {
		return -2;
	}
	
//	syslog(6,"----rxbuf %ld %ld %ld",sp->htc->rxbuf.b,sp->htc->rxbuf.e,Tlen(sp->htc->rxbuf));
//	syslog(6,"----pipeline %ld %ld %ld",sp->htc->pipeline.b,sp->htc->pipeline.e,Tlen(sp->htc->pipeline));
	
	int maxlen = params->http_req_hdr_len + 1;

	int rxbuf = Tlen(sp->htc->rxbuf);
	
	///////////////////////////////////////////////
	int u = WS_Reserve(sp->wrk->ws, 0);
	if(u < content_length + rxbuf + 1){
		return -1;
	}
	char *newbuffer = (char*)sp->wrk->ws->f;
	memcpy(newbuffer, sp->htc->rxbuf.b, rxbuf);
	sp->htc->rxbuf.b = newbuffer;
	newbuffer += rxbuf;
	newbuffer[0]= 0;
	sp->htc->rxbuf.e = newbuffer;
	WS_Release(sp->wrk->ws,content_length + rxbuf + 1);
	///////////////////////////////////////////////

	//
		
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
				return -3;
			}

			hsize += rsize;
			content_length -= rsize;

			strncat(newbuffer, buf, buf_size);

	}

	sp->htc->pipeline.b = newbuffer;
	sp->htc->pipeline.e = newbuffer + orig_content_length;

	//target
	VRT_SetHdr(sp, HDR_REQ, head, newbuffer, vrt_magic_string_end);
	decodeForm(sp, newbuffer);
	
	return 1;
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
