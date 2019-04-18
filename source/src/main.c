/*************************************************************************
 *
 *  main�ļ�
 *
 * 
 * 
 * 
 * 
 *
 * 
 * 
 * 
 * 
 *
 * 
 * 
 *************************************************************************/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
	 
#if defined(_WIN32) || defined(__WIN32__) || defined (WIN32)
#define _WIN32_WINNT 0x501
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif	 
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <semaphore.h> 
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/param.h>
#include <getopt.h>
#include <sys/socket.h>
#include <locale.h>
#include <errno.h> 
#include <sqlite3.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "cJSON.h"
#include "sql_db.h"
#include "common_lib.h"
#include "pn_imserver.h"
#include "version.h"
#include "tox_seg_msg.h"

static struct option long_opts[] = {
    {"help", no_argument, 0, 'h'},
    {"type", required_argument, 0, 't'},
    {"showqrcode", required_argument, 0, 's'},
    {"encrypt", required_argument, 0, 'e'},
    {"version", no_argument, 0, 'v'},
    {NULL, no_argument, NULL, 0},
};

const char *opts_str = "4bdeou:t:s:h:v:p:P:T:";
struct arg_opts_struct g_arg_opts;
char g_post_ret_buf[POST_RET_MAX_LEN] = {0};
extern sqlite3 *g_db_handle;
extern sqlite3 *g_friendsdb_handle;
extern sqlite3 *g_msglogdb_handle[PNR_IMUSER_MAXNUM+1];
extern struct im_user_array_struct g_imusr_array;
extern const char *data_file_name;
extern Tox *qlinkNode;
extern sqlite3 *g_msgcachedb_handle[PNR_IMUSER_MAXNUM+1];
extern char g_devadmin_loginkey[PNR_LOGINKEY_MAXLEN+1];
extern int g_format_reboot_time;
extern char g_dev_nickname[PNR_USERNAME_MAXLEN+1];
extern int g_pnrdevtype;
void *server_discovery_thread(void *args);
int im_debug_imcmd_deal(char* pcmd);

/*************************************************************************
 *
 * Function name: set_default_opts
 * 
 * Instruction: ����Ĭ����������
 * 
 * INPUT:none
 * 
 * 
 * OUPUT: none
 *
 *************************************************************************/
static void set_default_opts(void)
{
    memset(&g_arg_opts, 0, sizeof(g_arg_opts));
    /* set any non-zero defaults here*/
}
void print_usage(void)
{
    printf("command for example:\n");
    printf("\t pnr_server --showqrcode  XXX\n");
    printf("\t pnr_server --encrypt  XXX\n");
	printf("\t pnr_server --version\n");
    printf("\t pnr_server\n"); 
    printf("\t pnr_server -h\n");
    printf("\n");
}
void print_version(void)
{
    printf("version:%s.%s.%s\n"
		"build:%s ~ %s\n",
        PNR_SERVER_TOPVERSION,
        PNR_SERVER_MIDVERSION,
        PNR_SERVER_LOWVERSION,
        PNR_SERVER_BUILD_TIME,
	PNR_SERVER_BUILD_HASH);
}

/**********************************************************************************
  Function:      parse_args
  Description:   ������������
  Calls:
  Called By:
  Input:
  Output:        none
  Return:        0:���óɹ�
                 1:����ʧ��
  Others:

  History: 1. Date:2018-07-30
                  Author:Will.Cao
                  Modification:Initialize
***********************************************************************************/
static void parse_args(int argc, char *argv[])
{
    set_default_opts();

    int opt, indexptr;
    //long int port = 0;

    while ((opt = getopt_long(argc, argv, opts_str, long_opts, &indexptr)) != -1) 
    {
        switch (opt) 
        {
            case 'v':
    			print_version();
    			exit(EXIT_SUCCESS);
            case 's':
                account_qrcode_show(optarg);
    			exit(EXIT_SUCCESS);
            case 'e':
                pnr_encrypt_show(optarg,TRUE);
    			exit(EXIT_SUCCESS);
    		case 'h':
    		default:
    			print_usage();
    			exit(EXIT_SUCCESS);
                break;
        }
    }
}
/**********************************************************************************
  Function:      signal_init
  Description:  �ź�������
  Calls:
  Called By:
  Input:
  Output:        none
  Return:        0:���óɹ�
                     1:����ʧ��
  Others:

  History: 1. Date:2018-07-30
                  Author:Will.Cao
                  Modification:Initialize
***********************************************************************************/
int signal_init(void)
{ 
    //�����ն�I/O�źţ�STOP�ź�
    signal(SIGTTOU,SIG_IGN);
    signal(SIGTTIN,SIG_IGN);
    signal(SIGTSTP,SIG_IGN);
    signal(SIGHUP,SIG_IGN);
 
    //�ı乤��Ŀ¼��ʹ�ý��̲����κ��ļ�ϵͳ��ϵ
    chdir("/tmp");
 
    //���ļ���ʱ��������������Ϊ0
    umask(0);
 
    //����SIGCHLD�ź�
    signal(SIGCHLD,SIG_IGN); 
    return 0;
}
/**********************************************************************************
  Function:      init_daemon
  Description:  �л�Ϊ��̨����
  Calls:
  Called By:
  Input:
  Output:        none
  Return:        0:���óɹ�
                     1:����ʧ��
  Others:

  History: 1. Date:2018-07-30
                  Author:Will.Cao
                  Modification:Initialize
***********************************************************************************/
int init_daemon(void)
{ 
    int pid; 
    int i; 
 
    //�����ն�I/O�źţ�STOP�ź�
    signal(SIGTTOU,SIG_IGN);
    signal(SIGTTIN,SIG_IGN);
    signal(SIGTSTP,SIG_IGN);
    signal(SIGHUP,SIG_IGN);
    
    pid = fork();
    if(pid > 0) {
        exit(0); //���������̣�ʹ���ӽ��̳�Ϊ��̨����
    }
    else if(pid < 0) { 
        return -1;
    }
 
    //����һ���µĽ�����,������µĽ�������,�ӽ��̳�Ϊ�����������׽���,��ʹ�ý������������ն�
    setsid();
 
    //�ٴ��½�һ���ӽ��̣��˳������̣���֤�ý��̲��ǽ����鳤��ͬʱ�øý����޷��ٴ�һ���µ��ն�
    pid=fork();
    if( pid > 0) {
        exit(0);
    }
    else if( pid< 0) {
        return -1;
    }
 
    //�ر����дӸ����̼̳еĲ�����Ҫ���ļ�������
    for(i=0;i< NOFILE;close(i++));
 
    //�ı乤��Ŀ¼��ʹ�ý��̲����κ��ļ�ϵͳ��ϵ
    chdir("/tmp");
 
    //���ļ���ʱ��������������Ϊ0
    umask(0);
 
    //����SIGCHLD�ź�
    signal(SIGCHLD,SIG_IGN); 
    
    return 0;
}

/**********************************************************************************
  Function:      daemon_exists
  Description:  ��Ⲣ����pid�ļ�
  Calls:
  Called By:
  Input:
  Output:        none
  Return:        0:���óɹ�
                     1:����ʧ��
  Others:

  History: 1. Date:2018-07-30
                  Author:Will.Cao
                  Modification:Initialize
***********************************************************************************/
int daemon_exists(void)
{
	int fd;
	struct flock lock;
	char buffer[32];

	fd = open(DEAMON_PIDFILE, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
	if (fd < 0) {
		return 0;
	}

	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;

	if (fcntl(fd, F_SETLK, &lock) != 0) {
		close(fd);
		return 1;
	}

	ftruncate(fd, 0);
	snprintf(buffer, sizeof(buffer), "%d", getpid());
	write(fd, buffer, strlen(buffer));
	return 0;
}
/**********************************************************************************
  Function:      qlv_daemon_init
  Description:  qlv�ػ����̳�ʼ��
  Calls:
  Called By:
  Input:
  Output:        none
  Return:        0:���óɹ�
                 1:����ʧ��
  Others:

  History: 1. Date:2018-07-30
                  Author:Will.Cao
                  Modification:Initialize
***********************************************************************************/
int daemon_init(void)
{
	int8* errMsg = NULL;
    struct db_string_ret db_ret;
	char sql_cmd[SQL_CMD_LEN] = {0};
	struct rlimit resource = {65535, 65535};

    //����ͨ�Źܵ�
    unlink(DAEMON_FIFONAME);
    if (mkfifo(DAEMON_FIFONAME, 0777) == -1)
    {
        DEBUG_PRINT(DEBUG_LEVEL_ERROR,"mkfifo error %s", strerror(errno));
		return ERROR;
    }

	setrlimit(RLIMIT_NOFILE, &resource);
    //���������onespace����Ҫ��ִ����rngd��Ҫ��Ȼ sodium_init�����
    if(g_pnrdevtype == PNR_DEV_TYPE_ONESPACE)
    {
        system("/usr/bin/rngd -r/dev/urandom");
        system("echo 180 > /proc/sys/net/ipv4/netfilter/ip_conntrack_udp_timeout");
    }

    //�����ļ�Ŀ¼
    if(access(DAEMON_PNR_TOP_DIR,F_OK) != OK)
    {
        snprintf(sql_cmd,SQL_CMD_LEN,"mkdir -p %s",DAEMON_PNR_TOP_DIR);
        system(sql_cmd);
        DEBUG_PRINT(DEBUG_LEVEL_ERROR,"sql_cmd (%s)",sql_cmd);
    }  
    /* ����EPIPE�źţ���Ȼ������쳣�˳� */ 
    signal(SIGPIPE, SIG_IGN); 
    
    //������ݿ�
    if(sql_db_check() != OK)
    {
        DEBUG_PRINT(DEBUG_LEVEL_ERROR,"sql_db_check error %s", strerror(errno));
		return ERROR;
    }
    //��ȡ��ǰ����

    //im_server��ʼ��
    if (im_server_init() != OK) {
		DEBUG_PRINT(DEBUG_LEVEL_ERROR,"im_server_init failed");
		return ERROR;
	}
	snprintf(sql_cmd,SQL_CMD_LEN,"select value from generconf_tbl where name='%s';",DB_IMUSER_MAXNUM_KEYWORDK);
    if(sqlite3_exec(g_db_handle,sql_cmd,dbget_int_result,&g_imusr_array.max_user_num,&errMsg))
	{
        DEBUG_PRINT(DEBUG_LEVEL_ERROR,"get imuser_maxnum failed");
		sqlite3_free(errMsg);
		return ERROR;
	}
    db_ret.buf_len = PNR_LOGINKEY_MAXLEN;
    db_ret.pbuf = g_devadmin_loginkey;
	snprintf(sql_cmd,SQL_CMD_LEN,"select value from generconf_tbl where name='%s';",DB_DEVLOGINEKEY_KEYWORD);
    if(sqlite3_exec(g_db_handle,sql_cmd,dbget_singstr_result,&db_ret,&errMsg))
	{
        DEBUG_PRINT(DEBUG_LEVEL_ERROR,"get dev_loginkey failed");
		sqlite3_free(errMsg);
		return ERROR;
	}
    db_ret.buf_len = PNR_USERNAME_MAXLEN;
    db_ret.pbuf = g_dev_nickname;
	snprintf(sql_cmd,SQL_CMD_LEN,"select value from generconf_tbl where name='%s';",DB_DEVNAME_KEYWORD);
    if(sqlite3_exec(g_db_handle,sql_cmd,dbget_singstr_result,&db_ret,&errMsg))
	{
        DEBUG_PRINT(DEBUG_LEVEL_ERROR,"get g_dev_nickname failed");
		sqlite3_free(errMsg);
		return ERROR;
	}
    dev_hwaddr_init();
    DEBUG_PRINT(DEBUG_LEVEL_INFO,"get g_imusr_maxnum %d,cur_num %d,g_dev_nickname(%d:%s)",
        g_imusr_array.max_user_num,g_imusr_array.cur_user_num,db_ret.buf_len,g_dev_nickname);
    return OK;
}

/**********************************************************************************
  Function:      test_daemon
  Description:  �ػ����̣�
  Calls:
  Called By:
  Input:
  Output:        none
  Return:        0:���óɹ�
                     1:����ʧ��
  Others:

  History: 1. Date:2018-07-30
                  Author:Will.Cao
                  Modification:Initialize
***********************************************************************************/
static void *test_daemon(void *para)
{

	return NULL;
}
/**********************************************************************************
  Function:      tox_daemon
  Description:  tox�ػ����̣����������p2p�����齨������winq����Ϣ
  Calls:
  Called By:
  Input:
  Output:        none
  Return:        0:���óɹ�
                     1:����ʧ��
  Others:

  History: 1. Date:2018-07-30
                  Author:Will.Cao
                  Modification:Initialize
***********************************************************************************/
static void *tox_daemon(void *para)
{
    CreatedP2PNetwork();
	return NULL;
}
/**********************************************************************************
  Function:      imserver_daemon
  Description:  im_server�ػ����̣�������app��ͨ��
  Calls:
  Called By:
  Input:
  Output:        none
  Return:        0:���óɹ�
                     1:����ʧ��
  Others:

  History: 1. Date:2018-07-30
                  Author:Will.Cao
                  Modification:Initialize
***********************************************************************************/
static void *imserver_daemon(void *para)
{
    im_server_main();
	return NULL;
}

/**********************************************************************************
  Function:      monstat_daemon
  Description:  ״̬����ػ����̣�����ϵͳ����״̬���
  Calls:
  Called By:
  Input:
  Output:        none
  Return:        0:���óɹ�
                     1:����ʧ��
  Others:

  History: 1. Date:2018-07-30
                  Author:Will.Cao
                  Modification:Initialize
***********************************************************************************/
static void *monstat_daemon(void *para)
{
    //DEBUG_PRINT(DEBUG_LEVEL_INFO,"monstat_daemon in ");
    while(1)
    {
        get_meminfo();
        sleep(SYSINFO_CHECK_CYCLE);
    }
	return NULL;
}
/**********************************************************************************
  Function:      heartbeat_daemon
  Description:  �����ػ����̣�����άϵ����
  Calls:
  Called By:
  Input:
  Output:        none
  Return:        0:���óɹ�
                     1:����ʧ��
  Others:

  History: 1. Date:2018-07-30
                  Author:Will.Cao
                  Modification:Initialize
***********************************************************************************/
static void *heartbeat_daemon(void *para)
{
    //DEBUG_PRINT(DEBUG_LEVEL_INFO,"heartbeat_daemon in ");
    while(1)
    {
        //Heartbeat();
        sleep(HEARTBEAT_CYCLE);
    }
	return NULL;
}
/**********************************************************************************
  Function:      imuser_heartbeat_daemon
  Description:  im�û������ػ�����
  Calls:
  Called By:
  Input:
  Output:        none
  Return:        0:���óɹ�
                     1:����ʧ��
  Others:

  History: 1. Date:2018-07-30
                  Author:Will.Cao
                  Modification:Initialize
***********************************************************************************/
static void *imuser_heartbeat_daemon(void *para)
{
    //DEBUG_PRINT(DEBUG_LEVEL_INFO,"heartbeat_daemon in ");
    while(1)
    {
        imuser_heartbeat_deal();
        sleep(IMUSER_HEARTBEAT_CYCLE);
    }
	return NULL;
}

/*****************************************************************************
 �� �� ��  : im_send_msg_daemon
 ��������  : �����������Ϣ
 �������  : void *para  
 �������  : ��
 �� �� ֵ  : static
 ���ú���  : 
 ��������  : 
 
 �޸���ʷ      :
  1.��    ��   : 2018��10��16��
    ��    ��   : lichao
    �޸�����   : �����ɺ���

*****************************************************************************/
static void *im_send_msg_daemon(void *para)
{
	int *direction = (int *)para;
	
	while (1) {
        im_send_msg_deal(*direction);
        usleep(50000);
    }
	
	return NULL;
}

/**********************************************************************************
  Function:      msg_deal
  Description:  �ϱ���Ϣ�������
  Calls:
  Called By:
  Input:
  Output:        none
  Return:        0:���óɹ�
                     1:����ʧ��
  Others:

  History: 1. Date:2012-03-07
                  Author:Will.Cao
                  Modification:Initialize
***********************************************************************************/
void msg_deal(char * pbuf,int msg_len)
{
	int msg_type = 0;
	msg_type = atoi(pbuf);
	pbuf = strchr(pbuf,0x20);
	if(pbuf == NULL)
	{
		DEBUG_PRINT(DEBUG_LEVEL_ERROR,"bad msg_type param %d",pbuf);  
		return;
	}
	pbuf ++;
	msg_len -= 2;
	DEBUG_PRINT(DEBUG_LEVEL_NORMAL,"msg_deal len(%d) msg_type(%d) msg(%s)",msg_len,msg_type,pbuf); 
	switch(msg_type)
	{
	    case PNR_DEBUGCMD_SHOW_GLOBALINFO:
            im_global_info_show(pbuf);
            break;
        case PNR_DEBUGCMD_DATAFILE_BASE64_CHANGED:
            im_datafile_base64_change_cmddeal(pbuf);
            break;
        case PNR_DEBUGCMD_ACCOUNT_QRCODE_GET:
            im_account_qrcode_get_cmddeal(pbuf);
            break;
        case PNR_DEBUGCMD_DEBUG_IMCMD:
            im_debug_imcmd_deal(pbuf);
            break;
        case PNR_DEBUGCMD_PUSHNEWMSG_NOTICE:
            im_debug_pushnewnotice_deal(pbuf);
            break;
        //��̬����ĳЩϵͳ���ܵĿ���
        case PNR_DEBUGCMD_SET_FUNCENABLE:
            im_debug_setfunc_deal(pbuf);
            break;
        //ʹ��ģ���û�����
        case PNR_DEBUGCMD_SET_SIMULATION:
            im_simulation_setfunc_deal(pbuf);
            break;
        case PNR_DEBUGCMD_DEVINFO_REG:
            post_devinfo_upload_once(pbuf);
            break;
		default:
			DEBUG_PRINT(DEBUG_LEVEL_ERROR,"bad msg_type param %d",msg_type);  
			break;
	}
	return;
}

/**********************************************************************************
  Function:      fifo_msg_handle
  Description:  ��Ϣ����
  Calls:
  Called By:
  Input:
  Output:        none
  Return:        0:���óɹ�
                     1:����ʧ��
  Others:

  History: 1. Date:2012-03-07
                  Author:Will.Cao
                  Modification:Initialize
***********************************************************************************/
static int fifo_msg_handle(void)
{
    int fpipe;
    char line[BUF_LINE_MAX_LEN] = {0};
    char* pbuf;
	int line_len = 0;
    char* p_end = NULL;
	
	fpipe = open(DAEMON_FIFONAME, O_RDONLY);
	//�����ܵ�
	while(1)
    {	
    	line_len = read(fpipe, line, BUF_LINE_MAX_LEN);
        DEBUG_PRINT(DEBUG_LEVEL_INFO,"fifo_msg_handle read ret(%d)",line_len);  
        //��Ϣ�ṹ�������� "3 XXXX"
		if(line_len >= 3)
		{
			if(line[line_len-1] == '\n')
			{
				line[line_len-1] = 0;
				line_len = line_len-1;
			}
			//DEBUG_PRINT(DEBUG_LEVEL_INFO,"fifo_msg_handle %d (%s)",line_len,line);  
			pbuf = &line[0];
			p_end = NULL;
			p_end = strchr(pbuf,'\n');
			if(p_end != NULL)
			{
				//DEBUG_PRINT(DEBUG_LEVEL_INFO,"###########################################"); 
				//DEBUG_PRINT(DEBUG_LEVEL_INFO,"get len(%d) msg(%s)",line_len,pbuf); 
				while(1)
				{
					p_end[0] = 0;
					msg_deal(pbuf,p_end-pbuf);
					pbuf = p_end +1;
					p_end = NULL;
					p_end = strchr(pbuf,'\n');
					if(p_end == NULL)
					{
						msg_deal(pbuf,&line[line_len]-pbuf);
						break;
					}
				}
			}
			else
			{
				msg_deal(pbuf,line_len);
			}
            close(fpipe);
            fpipe = open(DAEMON_FIFONAME, O_RDONLY);
		}
		else
		{
    		DEBUG_PRINT(DEBUG_LEVEL_ERROR,"bad return (%d)",line_len); 
		    usleep(500);
            close(fpipe);
            fpipe = open(DAEMON_FIFONAME, O_RDONLY);
			continue;
		}
		memset(line,0,BUF_LINE_MAX_LEN);
	}
	
	close(fpipe);
    DEBUG_PRINT(DEBUG_LEVEL_ERROR,"fifo exit");  
	return OK;
}

/*****************************************************************************
 �� �� ��  : cron_thread
 ��������  : ��ʱ�����߳�
 �������  : void arg  
 �������  : ��
 �� �� ֵ  : 
 ���ú���  : 
 ��������  : 
 
 �޸���ʷ      :
  1.��    ��   : 2018��11��30��
    ��    ��   : lichao
    �޸�����   : �����ɺ���

*****************************************************************************/
void *cron_thread(void *para)
{
	char sql[1024];
	int8 *err = NULL;
	int deadtime, nowtime;
	int i = 0;
	
	while (1) {
		nowtime = time(NULL);

		for (i = 1; i <= PNR_IMUSER_MAXNUM; i++) {
			if (g_msgcachedb_handle[i] && (nowtime % 60 == 0)) {
				memset(sql, 0, sizeof(sql));
				deadtime = nowtime - 60;
				
				//use len as insert time
				snprintf(sql, sizeof(sql), "delete from msg_tbl where fromid='' and len < %d;", deadtime);

				if (sqlite3_exec(g_msgcachedb_handle[i], sql, 0, 0, &err)) {
			        DEBUG_PRINT(DEBUG_LEVEL_ERROR, "sqlite cmd(%s) err(%s)", sql, err);
			        sqlite3_free(err);
			    }
			}
		}

		if (g_format_reboot_time > 0 && nowtime >= g_format_reboot_time)
			system("sync;/opt/bin/umounthd.sh;reboot");
		
		sleep(1);
	}
}

/**********************************************************************************
  Function:      main
  Description:  ��������ں�������������������������������
  Calls:
  Called By:
  Input:
  Output:        none
  Return:        0:���óɹ�
                 1:����ʧ��
  Others:

  History: 1. Date:2018-07-30
                  Author:Will.Cao
                  Modification:Initialize
***********************************************************************************/
int32 main(int argc,char *argv[])
{
    pthread_t monstat_tid;
    pthread_t imserver_tid;
    pthread_t maintox_tid;
    pthread_t imuser_heartbeat_tid;
	pthread_t tid;
	int i = 0;

	/*���Կ���*/
    DEBUG_INIT(LOG_PATH);
    DEBUG_LEVEL(DEBUG_LEVEL_INFO);
	parse_args(argc, argv);
    //·��������proc�ű��л�����̨
#ifdef OPENWRT_ARCH
    signal_init();
#else
    init_daemon();
#endif
    if (daemon_exists()) 
    {
        DEBUG_PRINT(DEBUG_LEVEL_ERROR, "main exist");
        exit(1);
    }

	if (daemon_init()) {
		DEBUG_PRINT(DEBUG_LEVEL_ERROR, "init failed");
        exit(1);
	}

    //��ȡ������Ϣ
    /*������Ϣ����*/
    
    /*������tox���̣�����P2P����*/
	if (pthread_create(&maintox_tid, NULL, tox_daemon,NULL) != OK)
	{
        DEBUG_PRINT(DEBUG_LEVEL_ERROR, "pthread_create tox_daemon failed");
        goto out;
	}
	
     /*����monitor_stat���̣����ϵͳ��Դʹ�����*/
    if (pthread_create(&monstat_tid, NULL, monstat_daemon, NULL) != 0) 
    {
        DEBUG_PRINT(DEBUG_LEVEL_ERROR, "pthread_create monstat_daemon failed");
        goto out;
	}   

    //����im server����
    if (pthread_create(&imserver_tid, NULL, imserver_daemon, NULL) != 0) 
    {
        DEBUG_PRINT(DEBUG_LEVEL_ERROR, "pthread_create imserver_daemon failed");
        goto out;
	}
	
    //����im heartbeat����
    if (pthread_create(&imuser_heartbeat_tid, NULL, imuser_heartbeat_daemon, NULL) != 0) 
    {
        DEBUG_PRINT(DEBUG_LEVEL_ERROR, "pthread_create imuser_heartbeat_daemon failed");
        goto out;
	}

	//������Ϣ���ͽ���
	for (i = 0; i < 2; i++) {
		if (pthread_create(&tid, NULL, im_send_msg_daemon, &i) != 0) 
	    {
	        DEBUG_PRINT(DEBUG_LEVEL_ERROR, "pthread_create imuser_heartbeat_daemon failed");
	        goto out;
		}
	}

    //������������Ϣ
    if (pthread_create(&tid, NULL, server_discovery_thread, NULL) != 0) 
    {
        DEBUG_PRINT(DEBUG_LEVEL_ERROR, "pthread_create server discovery failed");
        goto out;
	}

    //pnr_qrcode_create_png_bycmd("111223334455","/tmp/test1.png");

	//tox��Ƭ��Ϣ����
    if (pthread_create(&tid, NULL, tox_seg_msg_flush, NULL) != 0) 
    {
        DEBUG_PRINT(DEBUG_LEVEL_ERROR, "pthread_create server discovery failed");
        goto out;
	}

	//��ʱ����
	if (pthread_create(&tid, NULL, cron_thread, NULL) != 0) 
    {
        DEBUG_PRINT(DEBUG_LEVEL_ERROR, "pthread_create cron failed");
        goto out;
	}
    //�豸ע������
    if(g_pnrdevtype == PNR_DEV_TYPE_ONESPACE)
    {
	    if (pthread_create(&tid, NULL,post_devinfo_upload_task, NULL) != 0) 
        {
            DEBUG_PRINT(DEBUG_LEVEL_ERROR, "pthread_create post_devinfo_upload_task failed");
            goto out;
        }
    }    
    //��Ϣ������ѯ����
    if (pthread_create(&tid, NULL,post_newmsgs_loop_task, NULL) != 0) 
    {
        DEBUG_PRINT(DEBUG_LEVEL_ERROR, "pthread_create post_newmsgs_loop_task failed");
        goto out;
    }
    fifo_msg_handle();
    while(1)
    {
        sleep(1);
    }

out:
	sqlite3_close(g_db_handle);
	sqlite3_close(g_friendsdb_handle);
    return OK;
}

