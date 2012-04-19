#include <stdlib.h>
#include "vcl.h"
#include "vrt.h"
#include "bin/varnishd/cache.h"
#include <time.h>

#include <arpa/inet.h>
#include <syslog.h>

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
	int ret = 0;
	if(ipv6){
		tmp->ss_family = AF_INET6;
	}else{
		tmp->ss_family = AF_INET;
	}

	if(str != NULL){
		if(ipv6){
			ret=inet_pton(AF_INET6 , str , &((struct sockaddr_in6 *)tmp)->sin6_addr);
		}else{
			ret=inet_pton(AF_INET , str , &((struct sockaddr_in *)tmp)->sin_addr);
		}
	}
	
	if(!ret){
		if(ipv6){
			if(defaultstr == NULL){
				ret=inet_pton(AF_INET6 , "ABC" , &((struct sockaddr_in6 *)tmp)->sin6_addr);
			}else{
				ret=inet_pton(AF_INET6 , defaultstr , &((struct sockaddr_in6 *)tmp)->sin6_addr);
			}
		}else{
			if(defaultstr == NULL){
				ret=inet_pton(AF_INET  , "ABC" , &((struct sockaddr_in *)tmp)->sin_addr);
			}else{
				ret=inet_pton(AF_INET  , defaultstr , &((struct sockaddr_in *)tmp)->sin_addr);
			}
		}
	}
	WS_Release(sp->wrk->ws, size);
	return tmp;
	
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
