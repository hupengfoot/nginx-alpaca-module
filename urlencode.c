#include <ngx_config.h>
#include <ngx_core.h>

#include "urlencode.h"

char *url_encode(char const *s, int len, int *new_length)//TODO,could use?
{
#define safe_emalloc(nmemb, size, offset)malloc((nmemb) * (size) + (offset))
	static unsigned char hexchars[] = "0123456789ABCDEF";
	register unsigned char c;
	unsigned char *to, *start;
	unsigned char const *from, *end;

	from = (unsigned char *)s;
	end = (unsigned char *)s + len;
	start = to = (unsigned char *) safe_emalloc(3, len, 1);
	if(!start || !to){
		return NULL;
	}

	while (from < end) {
		c = *from++;

		if (c == ' ') {
			*to++ = '+';
#ifndef CHARSET_EBCDIC
		} else if ((c < '0' && c != '-' && c != '.') ||
				(c < 'A' && c > '9') ||
				(c > 'Z' && c < 'a' && c != '_') ||
				(c > 'z')) {
			to[0] = '%';
			to[1] = hexchars[c >> 4];
			to[2] = hexchars[c & 15];
			to += 3;
#else /*CHARSET_EBCDIC*/
		} else if (!isalnum(c) && strchr("_-.", c) == NULL) {
			/* Allow only alphanumeric chars and '_', '-', '.'; escape the rest */
			to[0] = '%';
			to[1] = hexchars[os_toascii[c] >> 4];
			to[2] = hexchars[os_toascii[c] & 15];
			to += 3;
#endif /*CHARSET_EBCDIC*/
		} else {
			*to++ = c;
		}
	}
	*to = 0;
	if (new_length) {
		*new_length = to - start;
	}
	return (char *) start;
}

int pairUrlEncode(Pair* httpParams, char* out, int len){
	int isFirst = 1;
	int p = 0;
	int new_length = 0;
	char* buf;
	int i;
	for(i = 0; i < len; i++){
		if(isFirst == 1){
			isFirst = 0;
		}
		else{
			out[p] = '&';
			p++;
		}
		buf = url_encode(httpParams[i].key, strlen(httpParams[i].key), &new_length);
		if(!buf){
			return -1;
		}
		strcat(out, buf);
		free(buf);
		p++;
		p = p + new_length;
		strcat(out,"=");
		buf = url_encode(httpParams[i].value, strlen(httpParams[i].value), &new_length);	
		if(!buf){
			return -1;
		}
		strcat(out,buf);
		free(buf);
		p = p + new_length;
	}	
	return 1;
}
