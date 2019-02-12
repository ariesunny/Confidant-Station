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


#define HTTP_HEADERS_MAXLEN 	512 	// Headers ����󳤶�

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
	struct hostent *host;
	struct sockaddr_in cliaddr;

	sockfd=socket(AF_INET,SOCK_STREAM,0);
	if(sockfd < 0){
		perror("create socket error");
		return -1;
	}

	if(!(host=gethostbyname(server))){
		printf("gethostbyname(%s) error!\n", server);
		return -2;
	}

	bzero(&cliaddr,sizeof(struct sockaddr));
	cliaddr.sin_family=AF_INET;
	cliaddr.sin_port=htons(port);
	cliaddr.sin_addr=*((struct in_addr *)host->h_addr);

	if(connect(sockfd,(struct sockaddr *)&cliaddr,sizeof(struct sockaddr))<0){
		perror("[-] error");
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
	int re_len = strlen(page) + strlen(host) + strlen(HttpsPostHeaders) + len + HTTP_HEADERS_MAXLEN;
    char temp_buf[BUF_LINE_MAX_LEN+1] = {0};
	char *post = NULL;
	post = malloc(re_len);
	if(post == NULL){
        DEBUG_PRINT(DEBUG_LEVEL_ERROR,"post_pack:malloc(%d) failed",re_len);
		return -1;
	}

	sprintf(post, "POST %s HTTP/1.0\r\n", page);
    snprintf(temp_buf,BUF_LINE_MAX_LEN,"Host: %s:%d\r\n", host, port);
    strcat(post,temp_buf);
    strcat(post,HttpsPostHeaders);
    snprintf(temp_buf,BUF_LINE_MAX_LEN,"Content-Length: %d\r\n\r\n", len);
    strcat(post,temp_buf);
    strcat(post,content);

	re_len = strlen(post);
	memset(data, 0, re_len+1);
	memcpy(data, post, re_len);

    cJSON * params = NULL;
    char* post_data = NULL;
    params = cJSON_CreateObject();
    if(params == NULL)
    {
        DEBUG_PRINT(DEBUG_LEVEL_ERROR,"json create err");
        return ERROR;
    }
    cJSON_AddItemToObject(params, "post_param", cJSON_CreateString(post));
    post_data = cJSON_PrintUnformatted_noescape(params);
    cJSON_Delete(params);
    DEBUG_PRINT(DEBUG_LEVEL_INFO,"post_pack post:(%d_%d:%s)",len,re_len,post_data);
    free(post_data);
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
	char headers[HTTP_HEADERS_MAXLEN];

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

	char *sdata = malloc(ssize);
	if(sdata == NULL){
        DEBUG_PRINT(DEBUG_LEVEL_ERROR,"https_post:malloc(%d) failed",ssize);
		return -1;
	}

	// 1������TCP����
	sockfd = client_connect_tcp(host, port);
	if(sockfd < 0){
		free(sdata);
        DEBUG_PRINT(DEBUG_LEVEL_ERROR,"https_post:client_connect_tcp failed");
		return -2;
	}

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

	// 4��ͨ��SSL��������
	re = ssl_send(ssl, sdata, data_len);
	if(re < 0){
		free(sdata);
		close(sockfd);
		SSL_shutdown(ssl);
        DEBUG_PRINT(DEBUG_LEVEL_ERROR,"https_post:ssl_send failed");
		return -4;
	}

	// 5��ȡ������
	int r_len = 0;
	r_len = ssl_recv(ssl, buff, bsize);
	if(r_len < 0){
		free(sdata);
		close(sockfd);
		SSL_shutdown(ssl);
        DEBUG_PRINT(DEBUG_LEVEL_ERROR,"https_post:ssl_recv failed");
		return -5;
	}

	// 6���رջỰ, �ͷ��ڴ�
	free(sdata);
	close(sockfd);
	SSL_shutdown(ssl);
	ERR_free_strings();

	return r_len;
}

