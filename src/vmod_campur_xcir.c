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
char * url_encode(struct sess *sp, char* url,char *head){
	/*
	base:
	 http://d.hatena.ne.jp/hibinotatsuya/20091128/1259404695
	*/
	//3075
	char *copy;
	char buf[3075];
	int size = 3 * strlen(url) + 3;
	if(size > 3075){
		///////////////////////////////////////////////
		int u = WS_Reserve(sp->wrk->ws, 0);
		if(u < size){
			return NULL;
		}
		copy = (char*)sp->wrk->ws->f;
		///////////////////////////////////////////////
	}else{
		copy = buf;
	}
	
    int i;
    char *pt = url;
    unsigned char c;
    char *url_en = copy;

    for(i = 0; i < strlen(pt); i++){
        c = *url;

        if((c >= '0' && c <= '9')
        || (c >= 'A' && c <= 'Z')
        || (c >= 'a' && c <= 'z')
        || (c == '\'')
        || (c == '*')
        || (c == ')')
        || (c == '(')
        || (c == '-')
        || (c == '.')
        || (c == '_')){
            *url_en = c;
            ++url_en;
        }else if(c == ' '){
            *url_en = '+';
            ++url_en;
        }else{
            *url_en = '%';
            ++url_en;
            sprintf(url_en, "%02x", c);
            url_en = url_en + 2;
        }

        ++url;
    }
    
    *url_en = 0;
	if(size > 3075){
		WS_Release(sp->wrk->ws,strlen(copy)+1);
	}
    
    return copy;
}

void decodeForm_multipart(struct sess *sp,char *tg){
	
	char head[256];
	char bd[258];
	char *tgt = tg;
	char *boundary, *h_ctype_ptr;
	h_ctype_ptr = VRT_GetHdr(sp, HDR_REQ, "\015Content-Type:");
//Jun  2 17:44:38 localhost varnishd[10055]: multipart/form-data; boundary=----WebKitFormBoundary9zMcPL0jwBSXMH2x	
	boundary = strstr(h_ctype_ptr,"; boundary=");
	if(!boundary || strlen(boundary) > 255) return;
	boundary +=11;
	bd[0] = '-';
	bd[1] = '-';
	bd[2] = 0;
	strncat(bd,boundary,strlen(boundary));
	
	int bdlen = strlen(bd);
	
//	strlen(boundary);
	char* st = strstr(tgt,bd);
	char* ed;
	char* ned;
	char* ns;
	char* ne;
	char* ne2;
	char* bod;
	char* fn;
	char tmp;
	char *uebod;
	int hsize;
	int idx;
	while(1){
		ed = strstr(st + 1,bd);
		//getdata
		ns = strstr(st+bdlen,"name=\"");
		if(!ns) break;
		ns+=6;
		ne  = strstr(ns,"\"\r\n");
		ne2 = strstr(ns,"\"; ");
		bod = strstr(ns,"\r\n\r\n");
		fn = strstr(ns,"; filename");
		if(!ne) break;
		if(ne2 && ne2 < ne) ne = ne2;
		if(!bod) break;

		if((ne -1)[0]=='\\'){
			idx = 1;
		}else{
			idx=0;
		}
		tmp=ne[idx];
		ne[idx]=0;
		hsize=strlen(ns) +1;
		if(hsize > 255) break;
		head[0] = hsize;
		head[1] = 0;
		snprintf(head +1,255,"%s:",ns);
		ne[idx]=tmp;

		//filecheck
		if(fn && fn < bod){
			//content is file(skip)
			st = ed;
			continue;
		}
		ned = ed -2;
		bod +=4;
		tmp = ned[0];
		ned[0]=0;
		//bodyをURLエンコードする
		uebod = url_encode(sp,bod,head);
		if(uebod){
			VRT_SetHdr(sp, HDR_REQ, head, uebod, vrt_magic_string_end);
		}
		ned[0]=tmp;
		//search \r\n\r\n
		
		//search name="
		//"\r
		//" ;
		if(!ed)break;
		st = ed;
	}
}
void decodeForm_urlencoded(struct sess *sp,char *tg){
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
vmod_postcapture(struct sess *sp,const char* target,unsigned force){
	
	
//デバッグでReInitとかrestartの時に不具合でないかチェック（ロールバックも）
//Content = pipeline.e-bの時はRxbuf確保をしない（必要ないので）
//mix形式をurlencodeに切り替える（組み換えで安全に）
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
	char *h_clen_ptr, *h_ctype_ptr, *newbuffer;
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
	
	unsigned multipart = 0;
	
	h_ctype_ptr = VRT_GetHdr(sp, HDR_REQ, "\015Content-Type:");
//Jun  2 17:44:38 localhost varnishd[10055]: multipart/form-data; boundary=----WebKitFormBoundary9zMcPL0jwBSXMH2x
	
	if (!VRT_strcmp(h_ctype_ptr, "application/x-www-form-urlencoded")) {
		//multipart/form-data
	}else if(h_ctype_ptr != NULL && h_ctype_ptr == strstr(h_ctype_ptr, "multipart/form-data")){
		multipart = 1;
	}else{
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

	
	int maxlen = params->http_req_hdr_len + 1;
	if(Tlen(sp->htc->pipeline) == content_length){
		//no read
		newbuffer = sp->htc->pipeline.b;
	}else{
		int rxbuf = Tlen(sp->htc->rxbuf);
		///////////////////////////////////////////////
		int u = WS_Reserve(sp->wrk->ws, 0);
		if(u < content_length + rxbuf + 1){
			return -1;
		}
		newbuffer = (char*)sp->wrk->ws->f;
		memcpy(newbuffer, sp->htc->rxbuf.b, rxbuf);
		sp->htc->rxbuf.b = newbuffer;
		newbuffer += rxbuf;
		newbuffer[0]= 0;
		sp->htc->rxbuf.e = newbuffer;
		WS_Release(sp->wrk->ws,content_length + rxbuf + 1);
		///////////////////////////////////////////////
		
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
	}


	//target
	if(multipart){
		if(force) VRT_SetHdr(sp, HDR_REQ, head, newbuffer, vrt_magic_string_end);
		decodeForm_multipart(sp, newbuffer);
	}else{
		VRT_SetHdr(sp, HDR_REQ, head, newbuffer, vrt_magic_string_end);
		decodeForm_urlencoded(sp, newbuffer);
	}
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
