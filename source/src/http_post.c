/************************************************************************
    > File Name: https_post.c 
    > Author: willcao 
    > Created Time: 2018��08��29�� ������ 16ʱ42��21��
 ***********************************************************************/

#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include "common_lib.h"
#include <cJSON.h>
//#include "https_post.h"

extern pthread_mutex_t g_postlock;
#define HTTP_HEADERS_MAXLEN 	1024 	// Headers ����󳤶�

/*
 * Headers �������
 */
const char *HttpsPostHeaders = 	"User-Agent: Mozilla/4.0 (compatible; MSIE 5.01; Windows NT 5.0)\r\n"
								"Cache-Control: no-cache\r\n"
								"Accept: application/json\r\n"
								"Content-type: application/json\r\n";

/*
 * @Name 			- ����TCP����, ������������
 * @Parame *server 	- �ַ���, Ҫ���ӵķ�������ַ, ����Ϊ����, Ҳ����ΪIP��ַ
 * @Parame 	port 	- �˿�
 *
 * @return 			- ���ض�Ӧsock�������, ���ڿ��ƺ���ͨ��
 */
int client_connect_tcp(char *server,int port)
{
	int sockfd;
    struct hostent hostinfo,*result = NULL;
    char buf[1024] = {0};
    int rc = 0;
	struct sockaddr_in cliaddr;

	sockfd=socket(AF_INET,SOCK_STREAM,0);
	if(sockfd < 0){
		DEBUG_PRINT(DEBUG_LEVEL_ERROR,"create socket error");
		return -1;
	}
    if(gethostbyname_r(server,&hostinfo,buf,1024,&result,&rc)!= OK)
    {
        DEBUG_PRINT(DEBUG_LEVEL_ERROR,"gethostbyname(%s) error", server);
		return -2;
	}
    if(result == NULL)
    {
        DEBUG_PRINT(DEBUG_LEVEL_ERROR,"gethostbyname(%s) error", server);
		return -2;
	}
	bzero(&cliaddr,sizeof(struct sockaddr));
	cliaddr.sin_family=AF_INET;
	cliaddr.sin_port=htons(port);
	cliaddr.sin_addr=*((struct in_addr *)hostinfo.h_addr);

	if(connect(sockfd,(struct sockaddr *)&cliaddr,sizeof(struct sockaddr))<0){
		DEBUG_PRINT(DEBUG_LEVEL_ERROR,"[-] error");
		return -3;
	}

	return(sockfd);
}

/*
 * @Name 			- ��װpost���ݰ���headers
 * @parame *host 	- ������ַ, ����
 * @parame  port 	- �˿ں�
 * @parame 	page 	- url���·��
 * @parame 	len 	- �������ݵĳ���
 * @parame 	content - ��������
 * @parame 	data 	- �õ���װ�����ݽ��
 *
 * @return 	int 	- ���ط�װ�õ������ݳ���
 */
int post_pack(const char *host, int port, const char *page, int len, const char *content, char *data)
{
	int sdate_len = strlen(page) + strlen(host) + strlen(HttpsPostHeaders) + len + HTTP_HEADERS_MAXLEN;
    char temp_buf[URL_MAX_LEN+1] = {0};
	char *post = NULL;
    int re_len = 0;
	post = malloc(sdate_len+1);
	if(post == NULL){
        DEBUG_PRINT(DEBUG_LEVEL_ERROR,"post_pack:malloc(%d) failed",re_len);
		return -1;
	}
	memset(post, 0, re_len+1);
	sprintf(post, "POST %s HTTP/1.0\r\n", page);
    snprintf(temp_buf,URL_MAX_LEN,"Host: %s:%d\r\n", host, port);
    strcat(post,temp_buf);
    strcat(post,HttpsPostHeaders);
    memset(temp_buf,0,URL_MAX_LEN);
    snprintf(temp_buf,URL_MAX_LEN,"Content-Length: %d\r\n\r\n", len);
    strcat(post,temp_buf);
    strcat(post,content);
	re_len = strlen(post);
    if(re_len > sdate_len)
    {
        DEBUG_PRINT(DEBUG_LEVEL_ERROR,"post_pack:re_len(%d) failed",re_len);
        free(post);
		return -1;
    }
	strcpy(data, post);
    //DEBUG_PRINT(DEBUG_LEVEL_INFO,"post_pack post:(%d_%d:%s)",len,re_len,post);
	free(post);
	return re_len;
}

/*
 * @Name 		- 	��ʼ��SSL, ���Ұ�sockfd��SSL
 * 					��������ҪĿ����ͨ��SSL������sock
 * 					
 * @return 		- 	��������ɳ�ʼ�����󶨶�Ӧsockfd��SSLָ��
 */
SSL *ssl_init(int sockfd)
{
	int re = 0;
	SSL *ssl;
	SSL_CTX *ctx;

	SSL_library_init();
	SSL_load_error_strings();
	ctx = SSL_CTX_new(SSLv23_client_method());
	if (ctx == NULL){
		return NULL;
	}

	ssl = SSL_new(ctx);
	if (ssl == NULL){
		return NULL;
	}

	/* ��socket��SSL���� */
	re = SSL_set_fd(ssl, sockfd);
	if (re == 0){
		SSL_free(ssl);
		return NULL;
	}

    /*
     * ������, WIN32��ϵͳ��, ���ܺ���Ч�Ĳ��������, �˴��������������
     */
	RAND_poll();
	while (RAND_status() == 0)
	{
		unsigned short rand_ret = rand() % 65536;
		RAND_seed(&rand_ret, sizeof(rand_ret));
	}
	
	/*
     * ctxʹ�����, �����ͷ�
     */
	SSL_CTX_free(ctx);
	
	return ssl;
}

/*
 * @Name 			- ͨ��SSL�������Ӳ���������
 * @Parame 	*ssl 	- SSLָ��, �Ѿ���ɳ�ʼ�������˶�Ӧsock�����SSLָ��
 * @Parame 	*data 	- ׼���������ݵ�ָ���ַ
 * @Parame 	 size 	- ׼�����͵����ݳ���
 *
 * @return 			- ���ط�����ɵ����ݳ���, �������ʧ��, ���� -1
 */
int ssl_send(SSL *ssl, const char *data, int size)
{
	int re = 0;
	int count = 0;

	re = SSL_connect(ssl);

	if(re != 1){
		return -1;
	}

	while(count < size)
	{
		re = SSL_write(ssl, data+count, size-count);
		if(re == -1){
			return -2;
		}
		count += re;
	}

	return count;
}

/*
 * @Name 			- SSL��������, ��Ҫ�Ѿ���������
 * @Parame 	*ssl 	- SSLָ��, �Ѿ���ɳ�ʼ�������˶�Ӧsock�����SSLָ��
 * @Parame  *buff 	- �������ݵĻ�����, �ǿ�ָ��
 * @Parame 	 size 	- ׼�����յ����ݳ���
 *
 * @return 			- ���ؽ��յ������ݳ���, �������ʧ��, ����ֵ <0 
 */
int ssl_recv(SSL *ssl, char *buff, int size)
{
	int i = 0; 				// ��ȡ����ȡ��������, ���ж�headers�Ƿ���� 
	int len = 0;
	char headers[HTTP_HEADERS_MAXLEN] = {0};

	if(ssl == NULL){
		return -1;
	}

	// Headers�Ի��н���, �˴��ж�ͷ�Ƿ������
	while((len = SSL_read(ssl, headers, 1)) == 1)
	{
		if(i < 4){
			if(headers[0] == '\r' || headers[0] == '\n'){
				i++;
				if(i>=4){
					break;
				}
			}else{
				i = 0;
			}
		}
		//printf("%c", headers[0]);		// ��ӡHeaders
	}

	len = SSL_read(ssl, buff, size);
	return len;
}

int https_post(char *host, int port, char *url, const char *data, int dsize, char *buff, int bsize)
{
	SSL *ssl;
	int re = 0;
	int sockfd;
	int data_len = 0;
	int ssize = dsize + HTTP_HEADERS_MAXLEN; 	// �����͵����ݰ���С

	char *sdata = malloc(ssize+1);
	if(sdata == NULL){
        DEBUG_PRINT(DEBUG_LEVEL_ERROR,"https_post:malloc(%d) failed",ssize);
		return -1;
	}
    memset(sdata,0,ssize+1);
    //�����������������coerdump
    pthread_mutex_lock(&g_postlock);
	// 1������TCP����
	sockfd = client_connect_tcp(host, port);
	if(sockfd < 0){
		free(sdata);
        DEBUG_PRINT(DEBUG_LEVEL_ERROR,"https_post:client_connect_tcp failed");
        pthread_mutex_unlock(&g_postlock);
		return -2;
	}
    pthread_mutex_unlock(&g_postlock);
	// 2��SSL��ʼ��, ����Socket��SSL
	ssl = ssl_init(sockfd);
	if(ssl == NULL){
		free(sdata);
		close(sockfd);
        DEBUG_PRINT(DEBUG_LEVEL_ERROR,"https_post:ssl_init failed");
		return -3;
	}

	// 3�����POST����
	data_len = post_pack(host, port, url, dsize, data, sdata);
    if(data_len <= 0)
    {
		free(sdata);
		close(sockfd);
		SSL_shutdown(ssl);
        DEBUG_PRINT(DEBUG_LEVEL_ERROR,"https_post:post_pack failed");
		return -4;
    }
	// 4��ͨ��SSL��������
	re = ssl_send(ssl, sdata, data_len);
	if(re < 0){
		free(sdata);
		close(sockfd);
		SSL_shutdown(ssl);
        DEBUG_PRINT(DEBUG_LEVEL_ERROR,"https_post:ssl_send failed");
		return -5;
	}

	// 5��ȡ������
	int r_len = 0;
	r_len = ssl_recv(ssl, buff, bsize);
	if(r_len < 0){
		free(sdata);
		close(sockfd);
		SSL_shutdown(ssl);
        DEBUG_PRINT(DEBUG_LEVEL_ERROR,"https_post:ssl_recv failed");
		return -6;
	}

	// 6���رջỰ, �ͷ��ڴ�
	free(sdata);
	close(sockfd);
	SSL_shutdown(ssl);
	ERR_free_strings();

	return r_len;
}

