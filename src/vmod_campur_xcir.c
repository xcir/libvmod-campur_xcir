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
	for(int i = 0; i < DIGEST_LEN; ++i)
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
//��ɂ������ŃT�C�Y���v�Z����urlencode�`���ɕύX����int*

unsigned __url_encode(char* url,int *calc,char *copy){
	/*
	  base:http://d.hatena.ne.jp/hibinotatsuya/20091128/1259404695
	*/
	int i;
	char *pt = url;
	unsigned char c;
	char *url_en = copy;

	for(i = 0; i < strlen(pt); ++i){
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
	return(1);
}
unsigned url_encode_setHdr(struct sess *sp, char* url,char *head,int *encoded_size){
	char *copy;
	char buf[3075];
	int size = 3 * strlen(url) + 3;
	if(size > 3075){
		///////////////////////////////////////////////
		//use ws
		int u = WS_Reserve(sp->wrk->ws, 0);
		if(u < size){
			return 0;
		}
		copy = (char*)sp->wrk->ws->f;
		///////////////////////////////////////////////
	}else{
		copy = buf;
	}
	__url_encode(url,encoded_size,copy);

	*encoded_size += strlen(copy) + head[0] +1;
	if(size > 3075){
		WS_Release(sp->wrk->ws,strlen(copy)+1);
	}
	//sethdr
    VRT_SetHdr(sp, HDR_REQ, head, copy, vrt_magic_string_end);
	return(1);
}

void decodeForm_multipart(struct sess *sp,char *body,char *tgHead,unsigned parseFile){
	
	char tmp;
	char head[256];
	char sk_boundary[258];
	char *raw_boundary, *h_ctype_ptr;
	char *p_start, *p_end, *p_body_end, *name_line_end, *name_line_end_tmp, *start_body, *sc_name, *sc_filename;
	int  hsize, boundary_size ,idx;
	
	char *tmpbody    = body;
	int  encoded_size= 0;
	char *basehead   = 0;
	char *curhead;
	int  ll_u, ll_head_len, ll_size;
	int  ll_entry_count = 0;
	
	//////////////////////////////
	//get boundary
	h_ctype_ptr = VRT_GetHdr(sp, HDR_REQ, "\015Content-Type:");
	raw_boundary = strstr(h_ctype_ptr,"; boundary=");
	if(!raw_boundary || strlen(raw_boundary) > 255) return;
	raw_boundary   +=11;
	sk_boundary[0] = '-';
	sk_boundary[1] = '-';
	sk_boundary[2] = 0;
	strncat(sk_boundary,raw_boundary,strlen(raw_boundary));
	boundary_size = strlen(sk_boundary);


	p_start = strstr(tmpbody,sk_boundary);
	while(1){
		///////////////////////////////////////////////
		//search data

		//boundary
		p_end = strstr(p_start + 1,sk_boundary);
		if(!p_end) break;
		//name
		sc_name = strstr(p_start +boundary_size,"name=\"");
		if(!sc_name) break;
		sc_name+=6;
		//line end
		name_line_end  = strstr(sc_name,"\"\r\n");
		name_line_end_tmp = strstr(sc_name,"\"; ");
		if(!name_line_end) break;
		
		if(name_line_end_tmp && name_line_end_tmp < name_line_end)
			name_line_end = name_line_end_tmp;
		
		//body
		start_body = strstr(sc_name,"\r\n\r\n");
		if(!start_body) break;
		//filename
		sc_filename  = strstr(sc_name,"; filename");

		///////////////////////////////////////////////
		//build head
		if((name_line_end -1)[0]=='\\'){
			idx = 1;
		}else{
			idx=0;
		}
		tmp                   = name_line_end[idx];
		name_line_end[idx]    = 0;
		hsize                 = strlen(sc_name)+1;
		if(hsize > 255) break;
		head[0]               = hsize;
		head[1]               = 0;
		snprintf(head +1,255,"%s:",sc_name);
		name_line_end[idx]    = tmp;
		

		///////////////////////////////////////////////
		//filecheck
		if(!parseFile){
			if(sc_filename && sc_filename < start_body){
				//content is file(skip)
				p_start = p_end;
				continue;
			}
		}
		
		///////////////////////////////////////////////
		//create linked list
		//use ws
		ll_u = WS_Reserve(sp->wrk->ws, 0);
		ll_head_len = strlen(head +1) +1;
		ll_size = ll_head_len + sizeof(char*)+2;
		if(ll_u < ll_size){
			return ;
		}

		if(!basehead){
			curhead = (char*)sp->wrk->ws->f;
			basehead = curhead;
		}else{
			((char**)curhead)[0] = (char*)sp->wrk->ws->f;
			curhead = (char*)sp->wrk->ws->f;
		}
		++ll_entry_count;
		
		memcpy(curhead,head,ll_head_len);
		curhead +=ll_head_len;
		memset(curhead,0,sizeof(char*)+1);
		++curhead;
		WS_Release(sp->wrk->ws,ll_size);
		///////////////////////////////////////////////
		
		///////////////////////////////////////////////
		//url encode & set header
		p_body_end    = p_end -2;
		start_body    +=4;
		tmp           = p_body_end[0];
		p_body_end[0] = 0;
		//body��URL�G���R�[�h����
		if(!url_encode_setHdr(sp,start_body,head,&encoded_size)){
			//�������Ȃ�
			p_body_end[0] = tmp;
			break;
		}
		p_body_end[0] = tmp;

		///////////////////////////////////////////////
		//check last boundary
		if(!p_end) break;
		
		p_start = p_end;
	}
	if(tgHead[0] == NULL) return;

	//////////////
	//genarate x-www-form-urlencoded format data
	
	//////////////
	//use ws
	int u = WS_Reserve(sp->wrk->ws, 0);
	if(u < encoded_size + 1){
		return;
	}
	
	char *orig_cv_body, *cv_body, *h_val;
	orig_cv_body = cv_body = (char*)sp->wrk->ws->f;
	
	WS_Release(sp->wrk->ws,encoded_size+1);
	//////////////
	for(int i=0;i<ll_entry_count;++i){
		hsize = basehead[0];
		memcpy(cv_body,basehead+1,hsize);
		cv_body    += hsize - 1;
		cv_body[0] ='=';
		++cv_body;
		h_val      = VRT_GetHdr(sp,HDR_REQ,basehead);
		memcpy(cv_body,h_val,strlen(h_val));
		cv_body    +=strlen(h_val);
		cv_body[0] ='&';
		++cv_body;
		basehead   = *(char**)(basehead+strlen(basehead+1)+2);
	}
	orig_cv_body[encoded_size - 1] =0;
	
	VRT_SetHdr(sp, HDR_REQ, tgHead, orig_cv_body, vrt_magic_string_end);
	
}
void decodeForm_urlencoded(struct sess *sp,char *body){
	char head[256];
	char *sc_eq,*sc_amp;
	char *tmpbody = body;
	int hsize;
	char tmp;
	
	while(1){
		//////////////////////////////
		//search word
		if(!(sc_eq = strchr(tmpbody,'='))){
			break;
		}
		if(!(sc_amp = strchr(tmpbody,'&'))){
			sc_amp = sc_eq + strlen(tmpbody);
		}
		
		//////////////////////////////
		//build head
		tmp = sc_eq[0];
		sc_eq[0] = 0;// = -> null
		
		hsize = strlen(tmpbody) + 1;
		if(hsize > 255) return;
		head[0]   = hsize;
		head[1]   = 0;
		snprintf(head +1,255,"%s:",tmpbody);
		sc_eq[0]  = tmp;

		//////////////////////////////
		//build body
		tmpbody   = sc_eq + 1;
		tmp       = sc_amp[0];
		sc_amp[0] = 0;// & -> null

		//////////////////////////////
		//set header
		VRT_SetHdr(sp, HDR_REQ, head, tmpbody, vrt_magic_string_end);
		sc_amp[0] = tmp;
		tmpbody   = sc_amp + 1;
	}
}
int 
vmod_postcapture(struct sess *sp,const char* tgHeadName,unsigned parseFile){
//�f�o�b�O��ReInit�Ƃ�restart�̎��ɕs��łȂ����`�F�b�N�i���[���o�b�N���j
//Content = pipeline.e-b�̎���Rxbuf�m�ۂ����Ȃ��i�K�v�Ȃ��̂Łj
//mix�`����urlencode�ɐ؂�ւ���i�g�݊����ň��S�Ɂj<-����
/*
	  string(41) "submitter\"=a&submitter2=b&submitter3=vcc"
�@�@  string(42) "submitter%5C=a&submitter2=b&submitter3=vcc"
�@�@  
  string(39) "submitter=a&submitter2=b&submitter3=vcc"
  string(39) "submitter=a&submitter2=b&submitter3=vcc"
  string(83) "submitter=%e3%81%82%e3%81%84%e3%81%86%e3%81%88%e3%81%8a&submitter2=b&submitter3=vcc"
  string(83) "submitter=%E3%81%82%E3%81%84%E3%81%86%E3%81%88%E3%81%8A&submitter2=b&submitter3=vcc"

	1	=����
	-1	=�G���[		���[�N�X�y�[�X�T�C�Y������Ȃ�
	-2	=�G���[		target/ContentLength���Ȃ�/�s��
	-3	=�G���[		�w��ContentLength�ɖ����Ȃ��f�[�^
	-4	=�G���[		���Ή��`��
	if (!(
		!VRT_strcmp(VRT_r_req_request(sp), "POST") ||
		!VRT_strcmp(VRT_r_req_request(sp), "PUT")
	)){return "";}
*/
	unsigned long	content_length,orig_content_length;
	char 			*h_clen_ptr, *h_ctype_ptr, *body;
	int				buf_size, rsize;
	char			buf[1024],tgHead[256];
	unsigned		multipart = 0;

	
	
	//////////////////////////////
	//build tgHead
	int hsize = strlen(tgHeadName) +1;
	if(hsize > 1){
		if(hsize > 255) return -2;
		tgHead[0] = hsize;
		tgHead[1] = 0;
		snprintf(tgHead +1,255,"%s:",tgHeadName);
	}else{
		tgHead[0] = NULL;
	}

	//////////////////////////////
	//check Content-Type
	h_ctype_ptr = VRT_GetHdr(sp, HDR_REQ, "\015Content-Type:");
	
	if (!VRT_strcmp(h_ctype_ptr, "application/x-www-form-urlencoded")) {
		//application/x-www-form-urlencoded
	}else if(h_ctype_ptr != NULL && h_ctype_ptr == strstr(h_ctype_ptr, "multipart/form-data")){
		//multipart/form-data
		multipart = 1;
	}else{
		//none support type
		return -4;
	}

	//////////////////////////////
	//check Content-Length
	h_clen_ptr = VRT_GetHdr(sp, HDR_REQ, "\017Content-Length:");
	if (!h_clen_ptr) {
		//can't get
		return -2;
	}
	orig_content_length = content_length = strtoul(h_clen_ptr, NULL, 10);

	if (content_length <= 0) {
		//illegal length
		return -2;
	}

	//////////////////////////////
	//Check POST data is loaded
	if(sp->htc->pipeline.b != NULL && Tlen(sp->htc->pipeline) == content_length){
		//complete read
		body = sp->htc->pipeline.b;
	}else{
		//incomplete read
		
		int rxbuf_size = Tlen(sp->htc->rxbuf);
		///////////////////////////////////////////////
		//use ws
		int u = WS_Reserve(sp->wrk->ws, 0);
		if(u < content_length + rxbuf_size + 1){
			return -1;
		}
		body = (char*)sp->wrk->ws->f;
		memcpy(body, sp->htc->rxbuf.b, rxbuf_size);
		sp->htc->rxbuf.b = body;
		body += rxbuf_size;
		body[0]= 0;
		sp->htc->rxbuf.e = body;
		WS_Release(sp->wrk->ws,content_length + rxbuf_size + 1);
		///////////////////////////////////////////////
		
		//////////////////////////////
		//read post data
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

			strncat(body, buf, buf_size);
		}
		sp->htc->pipeline.b = body;
		sp->htc->pipeline.e = body + orig_content_length;
	}


	//////////////////////////////
	//decode form
	if(multipart){
		decodeForm_multipart(sp, body,tgHead,parseFile);
	}else{
		if(tgHead[0] != NULL)
			VRT_SetHdr(sp, HDR_REQ, tgHead, body, vrt_magic_string_end);
		decodeForm_urlencoded(sp, body);
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
