#include <stdint.h>
#include "re.h"
#include "re_bytecode.h"
static uint32_t u(const unsigned char**pp){const unsigned char*p=*pp;unsigned c=*p++,n;if(c<128)n=0;else{n=c<224?1:c<240?2:3;c&=127>>(n+1);while(n--&&*p)c=c<<6|(*p++&63);}*pp=p;return c;}
static int16_t j(const unsigned char*p){return p[0]|p[1]<<RE_BYTE_BITS;}
static int w(unsigned c){return c=='_'||c-'0'<10||(c|32)-'a'<26;}
int re_match(const unsigned char*code,const char*text,const char**a,const void**v,unsigned steps,unsigned workspace_depth){uint32_t c,x,y;const unsigned char*b=(const unsigned char*)text,*p=code+1,*s=b,*q;const void**t=v;unsigned m=*code,i,k,n;for(i=m;i--;)a[i]=0;for(;;){if(!steps)return RE_BUDGET;--steps;switch(*p++){
case RE_OP_MATCH:return 1;
case RE_OP_BOL:if(s!=b)goto f;break;
case RE_OP_EOL:if(*s)goto f;break;
case RE_OP_WORD_BOUNDARY:case RE_OP_NOT_WORD_BOUNDARY:k=s>b&&s[-1]<128&&w(s[-1]);n=*s&&*s<128&&w(*s);if((k==n)==(p[-1]==RE_OP_WORD_BOUNDARY))goto f;break;
case RE_OP_LINE_BOL:if(s!=b&&s[-1]!='\n')goto f;break;
case RE_OP_LINE_EOL:if(*s&&*s!='\n')goto f;break;
case RE_OP_CLASS:if(!*s)goto f;q=s;c=u(&q);k=*p++;n=k&RE_CLASS_RANGE_MASK;k>>=RE_CLASS_NEGATED_SHIFT;while(n--){x=u(&p);y=u(&p);if(c>=x&&c<=y)k^=1;}if(!k)goto f;s=q;break;
case RE_OP_SPLIT:if(!workspace_depth)return RE_SPACE;--workspace_depth;*t++=p+RE_OFFSET_BYTES+j(p);*t++=s;for(i=0;i<m;i++)*t++=a[i];p+=RE_OFFSET_BYTES;break;
case RE_OP_JUMP:p=p+RE_OFFSET_BYTES+j(p);break;
case RE_OP_SAVE:a[*p++]=(const char*)s;break;
}continue;f:if(t==v)return 0;t-=m+2;p=t[0];s=t[1];for(i=0;i<m;i++)a[i]=t[i+2];++workspace_depth;}}
