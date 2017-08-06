/*{
    "window.zoomLevel": 0,
    "files.autoSave": "off"
} > 
mipsel-openwrt-linux-gcc udpTrClient.c b64.c -L./ -I./json/out/include/json/  -L./json/out/lib -ljson -luci -lubox  -o udpreport

 ************************************************************************/
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#include <sys/wait.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <pthread.h>
#include "b64.h"
#include "uci.h"
#include "json.h"
#include "httppost.h"


#define ROUTER_PORT 8880
#define HOMEDEV_PORT 8880

#define BUFFER_SIZE 1200
#define FILE_NAME_MAX_SIZE 512

#define MAX_TEXT 512

#define MAXBUF 1500
#define DEV_SIZE 6
#define APP_VERSION 1
#define SERVER_IP "192.168.3.68" //tz.pifii.com
#define SERVER_PORT "8880"
#define HTTPPOST_IP "192.168.3.151"//tz.pifii.com
#define HTTPPOST_PORT "8082"
#define HTTPPOST_PATH "/wifi_home/acsjson"
#define HARD_VERSION "v2.1.8"
#define SOFT_VERSION "v2.1.8"


static char *fc_script = "/usr/sbin/freecwmp";
static char *fc_script_set_actions = "/tmp/freecwmp_set_action_values.sh";
#define HOMEPWD "/etc/config/"
#define JSPWD "/usr/lib/js/"
//#define JSPWD "./js/"
#define ErrorJson "{\"name\": \"errorResponse\",\"version\": \"1.0.0\",\"serialnumber\": \"112233445566\",\"error\": \"1\"}"
#define FileJson "{\"name\": \"getResponse\",\"version\": \"1.0.0\",\"serialnumber\": \"%s\",\
				\"keyname\": \"file\",\"packet\": {\"path\": \"/etc/config/\",\"filename\": \"%s\",\"data\": \"%s\"}}"
#define ConfigJson "{\"name\": \"getResponse\",\"version\": \"1.0.0\",\"serialnumber\": \"%s\",\
				\"keyname\": \"config\",\"packet\": {\"data\": \"%s\"}}"
#define CommandJson "{\"name\": \"getResponse\",\"version\": \"1.0.0\",\"serialnumber\": \"%s\",\
				\"keyname\": \"command\",\"packet\": {\"data\": \"%s\"}}"
#define SetResponse "{\"name\": \"setResponse\",\"version\": \"1.0.0\",\"serialnumber\": \"%s\",\"keyname\": \"%s\",\
					\"packet\": {\"data\": \"%s\"}}"
#define DownloadResponse "{\"name\": \"setResponse\",\"version\": \"1.0.0\",\"serialnumber\": \"%s\",\"keyname\": \"%s\",\
					\"packet\": {\"data\": \"%s\",\"ProductClass\":\"%s\",\"Manufacturer\":\"%s\",\"Status\":\"%s\"}}"
#define GetResponse "{\"name\": \"getResponse\",\"version\": \"1.0.0\",\"serialnumber\": \"%s\",\
				\"keyname\": \"config\",\"packet\": {%s}}"
#define GetReport "{\"name\": \"getResponse\",\"version\": \"1.0.0\",\"serialnumber\": \"%s\",\
				\"keyname\": \"reportconfig\",\"packet\": {%s}}"        
#define TestJson "{\"name\": \"get\",\"version\": \"1.0.0\",\"serialnumber\": \"112233445566\",\
				\"keyname\": \"getvalue\",\"packet\": {\"UpTime\": \"sss\",\"wan_type\": \"sss\"}}"
#define HomeResponse "{\"sid\": \"%s\",\"id\": \"%s\",\"ver\": \"%s\",\
				\"cmdtype\": \"2\",\"result\":\"ok\", \"errordes\":\"\"}"
#define HomeDeviceState "{\"name\": \"update_smartdevs\", \"serialnumber\": \"%s\", \"devs\":[%s]}"

char informRes[1500];
pthread_mutex_t mutex;  
char reportServerIp[32];
char reportServerPort[32];

char httpPostServerIp[32];
char httpPostServerPort[32];
char httpPostServerPath[32];

#define FREE(x) \
  do            \
  {             \
    free(x);    \
    x = NULL;   \
  } while (0);

typedef int SOCKET;

typedef struct
{
  char devid[DEV_SIZE];
  int version;
  int id;
  int bufsize;
  char data[BUFFER_SIZE];
} SendPack;

typedef struct
{
  char devid[12];
  int version;
  int id;
  int bufsize;
  char data[BUFFER_SIZE]; //包含 RealTimeDate WirelessDates  NetworkDate
} RecvPack;

typedef struct
{
  char tr069state;       //tr069状态
  char cputype;          //cpu类型  1：mt7620 2：mt7628 3:ar9341
  char connectnum;       //客户端连接数量
  char aprouter;         //ap router类型  1：ap 2：router
  char equipment[16];    //硬件型号：FQa10-Tb
  char hardwaretype[16]; //设备厂家：FQ
  char softwaretype[16]; //软件版本：HBUCC-v1.7.013
  char portstate[8];     //port状态:1:连接 0：未连接
  int cpuload;           //系统负载：10 表示10%
  int memload;           //内存利用率：10 表示10%
  int upflow;            //上行流量
  int downflow;          //下行流量
  int uptime;            //在线时长'
} RealTimeDate;

typedef struct
{
  char ssid[30];
  char password[30];
  int encryption;
  int channel;
  int portel;
  int disabled;
} WirelessDate;

typedef struct
{
  int wifinum;
  WirelessDate wifidata[2];
} WirelessDates;

typedef struct
{
  int mode;
  char username[50];
  char password[50];
  char ipaddr[20];
  char network[20];
  char gateway[20];
  char dns1[20];
  char dns2[20];
} NetworkDate;

typedef struct
{
  int enable;
  int ipaddr;
  int port;
} TcpdumpData;

char deviceMac[13];
char deviceMacFu[18];

pid_t getPidByName(char *name)
{
  FILE *fp;
  char n = 0;
  pid_t pid = -1;
  char buf[10] = "";
  fp = popen(name, "r");
  if (fp != NULL)
  {
    if ((fgets(buf, 6, fp)) == NULL)
    {
      pclose(fp);
      return (pid);
    }
    pclose(fp);
    pid = atoi(buf);
  }
  return (pid);
} /* end of getpidbyname */

int external_get_action(char *action, char *name, char **value)
{
  //lfc_log_message(NAME, L_NOTICE, "executing get %s '%s'\n",
  //		action, name);
  int pid;
  int pfds[2];
  char *c = NULL;

  printf("jiangyibo action %s %s\n", action, name);

  if (pipe(pfds) < 0)
    return -1;

  if ((pid = fork()) == -1)
    goto error;

  if (pid == 0)
  {
    /* child */
    const char *argv[8];
    int i = 0;
    argv[i++] = "/bin/sh";
    argv[i++] = fc_script;
    argv[i++] = "--newline";
    argv[i++] = "--value";
    argv[i++] = "get";
    argv[i++] = action;
    argv[i++] = name;
    argv[i++] = NULL;

    close(pfds[0]);
    dup2(pfds[1], 1);
    close(pfds[1]);
    execvp(argv[0], (char **)argv);
    exit(ESRCH);
  }
  else if (pid < 0)
    goto error;

  /* parent */
  close(pfds[1]);

  int status;
  while (wait(&status) != pid)
  {
    printf("waiting for child to exit");
  }

  char buffer[256];
  ssize_t rxed;
  int t;

  *value = NULL;
  while ((rxed = read(pfds[0], buffer, sizeof(buffer))) > 0)
  {

    if (*value)
    {
      t = asprintf(&c, "%s%.*s", *value, (int)rxed, buffer);
      printf("jiangyibo get kkkkk%s\n", c);
    }
    else
    {

      t = asprintf(&c, "%.*s", (int)rxed, buffer);
      /*      *value = NULL;
      goto done;
*/
      printf("jiangyibo get %s\n", c);
    }
    printf("jiangyibo get kkkkk sss %d %s\n",t, c);
    if (t == -1)
      goto error;

    free(*value);
    *value = strdup(c);
    free(c);
  }

  if (!(*value))
  {
    goto done;
  }

  if (!strlen(*value))
  {
    FREE(*value);
    goto done;
  }

  if (rxed < 0)
    goto error;

done:
  close(pfds[0]);
  return 0;

error:
  free(c);
  FREE(*value);
  close(pfds[0]);
  return -1;
}

int external_set_action_write(char *action, char *name, char *value)
{

  FILE *fp;

  if (access(fc_script_set_actions, R_OK | W_OK | X_OK) != -1)
  {
    fp = fopen(fc_script_set_actions, "a");
    if (!fp)
      return -1;
  }
  else
  {
    fp = fopen(fc_script_set_actions, "w");
    if (!fp)
      return -1;

    fprintf(fp, "#!/bin/sh\n");

    if (chmod(fc_script_set_actions,
              strtol("0700", 0, 8)) < 0)
    {
      return -1;
    }
  }

  fprintf(fp, "/bin/sh %s set %s %s '%s'\n", fc_script, action, name, value);

  fclose(fp);

  return 0;
}

int external_set_action_execute()
{
  int pid = 0;
  if ((pid = fork()) == -1)
  {
    return -1;
  }

  if (pid == 0)
  {
    /* child */

    const char *argv[3];
    int i = 0;
    argv[i++] = "/bin/sh";
    argv[i++] = fc_script_set_actions;
    argv[i++] = NULL;

    execvp(argv[0], (char **)argv);
    exit(ESRCH);
  }
  else if (pid < 0)
    return -1;

  /* parent */
  int status;
  while (wait(&status) != pid)
  {
    printf("waiting for child to exit");
  }

  // TODO: add some kind of checks
  /*
	if (remove(fc_script_set_actions) != 0)
		return -1;
*/
  return 0;
}

int external_download(char *url, char *size)
{
  int pid = 0;

  if ((pid = fork()) == -1)
    return -1;

  if (pid == 0)
  {
    /* child */

    const char *argv[8];
    int i = 0;
    argv[i++] = "/bin/sh";
    argv[i++] = fc_script;
    argv[i++] = "download";
    argv[i++] = "--url";
    argv[i++] = url;
    argv[i++] = "--size";
    argv[i++] = size;
    argv[i++] = NULL;

    execvp(argv[0], (char **)argv);
    exit(ESRCH);
  }
  else if (pid < 0)
    return -1;

  /* parent */
  int status;
  while (wait(&status) != pid)
  {
    printf("waiting for child to exit");
  }

  if (WIFEXITED(status) && !WEXITSTATUS(status))
    return 0;
  else
    return 1;

  return 0;
}

int commandDownload(char *url, char *size)
{
  external_download(url, size);
  return 1;
}

typedef struct homedevice
{
  unsigned int addr;
  int uptime;
  int enable;  
  char mac[18];
  char devtype[8];
  char statstr[64];
} HomeDevice;

HomeDevice homeDev[10];

int initHomeDevice()
{
  memset(homeDev, 0, 10 * sizeof(HomeDevice));
  return 1;
}

int AddHomeDevice(char *mac,unsigned int addr,char *devtype,char *statstr)
{
  int i=0;
  time_t uptime;
  uptime = time(NULL);

  pthread_mutex_lock(&mutex);//锁定互斥锁  
  for (i = 0; i < 10; i++)
  {

    if (homeDev[i].enable == 0)
    {
      memcpy(homeDev[i].mac, mac,17);
      homeDev[i].addr = addr;
      if(devtype!=NULL)
      {
         strcpy(homeDev[i].devtype,devtype);
      }
      if(statstr!=NULL)
      {
         strcpy(homeDev[i].statstr,statstr);
      }
      homeDev[i].uptime = uptime;
      homeDev[i].enable = 1;
      pthread_mutex_unlock(&mutex);//打开互斥锁 
      return 1;
    }
    else if (!strcmp(mac, homeDev[i].mac))
    {
      if (homeDev[i].addr == addr)
      {
        homeDev[i].uptime = uptime;
      }
      else
      {
        homeDev[i].addr = addr;
        homeDev[i].uptime = uptime;
      }

      if(devtype!=NULL)
      {
         strcpy(homeDev[i].devtype,devtype);
      }
      if(statstr!=NULL)
      {
         strcpy(homeDev[i].statstr,statstr);
      }

      pthread_mutex_unlock(&mutex);//打开互斥锁 
      return 1;
    }
  }
  pthread_mutex_unlock(&mutex);//打开互斥锁 
  return 0;
}

int GetHomeDevice(char *value)
{
  int i;
  int index = 0;
  time_t lt;
  lt = time(NULL);
  char tempstr[128];
    pthread_mutex_lock(&mutex);//锁定互斥锁 
  for (i = 0; i < 10; i++)
  {
    if (homeDev[i].enable == 1)
    {
      if (lt - homeDev[i].uptime > 120)
      {
        homeDev[i].enable = 0;
      }
      else
      {
        memset(tempstr, 0, 128);
        if (index++ == 0)
        {
          sprintf(tempstr, "{\"id\":\"%s\",\"devtype\":\"%s\",\"statstr\":\"%s\"}", homeDev[i].mac,homeDev[i].devtype,homeDev[i].statstr);
        }
        else
        {
          sprintf(tempstr, ",{\"id\":\"%s\",\"devtype\":\"%s\",\"statstr\":\"%s\"}", homeDev[i].mac,homeDev[i].devtype,homeDev[i].statstr);
        }
        strcat(value, tempstr);
      }
    }
  }
  pthread_mutex_unlock(&mutex);//打开互斥锁 
  return 0;
}

int FindHomeDevice(char *mac)
{
  int i;
  for (i = 0; i < 10; i++)
  {
    if (homeDev[i].enable == 1)
    {
      if (!strcmp(mac, homeDev[i].mac))
      {
        return homeDev[i].addr;
      }
    }
  }
  return 0;
}

int commandSendtoHomeDevice(int server_socket_fd, char *mac, char *sendData)
{
  int addr = FindHomeDevice(mac);
  struct sockaddr_in client_addr;
  socklen_t client_addr_length = sizeof(client_addr);
  if (addr == 0)
  {
    return 0;
  }
  else
  {
    bzero(&client_addr, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
      client_addr.sin_port = htons(HOMEDEV_PORT);
    client_addr.sin_addr.s_addr = addr;
    printf("command send %0x\n",addr);
    if (sendto(server_socket_fd, sendData, strlen(sendData), 0, (struct sockaddr *)&client_addr, client_addr_length) < 0)
    {
      printf("Send File Name Failed:");
    }
    else
    {
      printf("Send ok\n");
    }

    return 1;
  }
}

int commandFactoryset()
{
  system("rm -rf /overlay/*");
  system("reboot");
  return 1;
}

int setShellValue(char *value)
{

  char *c = NULL;
  if (NULL == value || '\0' == value[0])
  {
    if (external_get_action("value", "text", &c))
      goto error;
  }
  else
  {
    c = strdup(value);
  }
  if (c)
  {

    FREE(c);
  }
  return 0;
error:
  return -1;
}

char *GetValByEtype(json_object *jobj, const char *sname)
{
  json_object *pval = NULL;
  enum json_type type;
  pval = json_object_object_get(jobj, sname);
  if (NULL != pval)
  {
    type = json_object_get_type(pval);
    switch (type)
    {
    case json_type_string:
      return json_object_get_string(pval);
    case json_type_int:
      return json_object_get_int(pval);

    default:
      return NULL;
    }
  }
  return NULL;
}

int GetIntByEtype(json_object *jobj, const char *sname)
{
  json_object *pval = NULL;
  enum json_type type;
  pval = json_object_object_get(jobj, sname);
  if (NULL != pval)
  {
    type = json_object_get_type(pval);
    switch (type)
    {
    case json_type_int:
      return json_object_get_int(pval);

    default:
      return 0;
    }
  }
  return 0;
}

json_object *GetValByEdata(json_object *jobj, const char *sname)
{
  json_object *pval = NULL;
  enum json_type type;
  pval = json_object_object_get(jobj, sname);
  if (NULL != pval)
  {
    type = json_object_get_type(pval);
    switch (type)
    {
    case json_type_object:
      return pval;

    case json_type_array:
      return pval;
    default:
      return NULL;
    }
  }
  return NULL;
}

char *GetValByKey(json_object *jobj, const char *sname)
{
  json_object *pval = NULL;
  enum json_type type;
  pval = json_object_object_get(jobj, sname);
  if (NULL != pval)
  {
    type = json_object_get_type(pval);
    switch (type)
    {
    case json_type_string:
      return json_object_get_string(pval);

    case json_type_object:
      return json_object_to_json_string(pval);

    default:
      return NULL;
    }
  }
  return NULL;
}
int getConfigFile(char *msg, char *filename)
{
  char temp[64];
  sprintf(temp, "%s%s", HOMEPWD, filename);
  FILE *pFile = fopen(temp, "r"); //

  if (pFile == NULL)
  {
    return 0;
  }

  fseek(pFile, 0, SEEK_END); //把指针移动到文件的结尾 ，获取文件长度
  int len = ftell(pFile);    //获取文件长度

  rewind(pFile);             //把指针移动到文件开头 因为我们一开始把指针移动到结尾，如果不移动回来 会出错
  fread(msg, 1, len, pFile); //读文件
  msg[len] = 0;              //把读到的文件最后一位 写为0 要不然系统会一直寻找到0后才结束

  fclose(pFile); // 关闭文件
  return len;
}

void getFileData(char *msg, char *filename)
{
  char temp[64];
  sprintf(temp, "%s%s", JSPWD, filename);
  FILE *pFile = fopen(temp, "r"); //获取文件的指针

  fseek(pFile, 0, SEEK_END); //把指针移动到文件的结尾 ，获取文件长度
  int len = ftell(pFile);    //获取文件长度

  rewind(pFile);             //把指针移动到文件开头 因为我们一开始把指针移动到结尾，如果不移动回来 会出错
  fread(msg, 1, len, pFile); //读文件
  msg[len] = 0;              //把读到的文件最后一位 写为0 要不然系统会一直寻找到0后才结束

  fclose(pFile); // 关闭文件
}

int jsonGetConfig(SOCKET s, json_object *config)
{
  int rc = 0;
  char tempstr[2048];
  char sendbuf[2048];
  char kvbuf[2048];
  char *tempVal = NULL;
  enum json_type type;
  int index = 0;
  json_object *obj = config;
  char *key;
  struct json_object *val;
  char *value;
  memset(kvbuf, 0, 2048);

  if (config == NULL)
  {
    printf("jyb test error\n");
    return;
  }

  struct lh_entry *entry = json_object_get_object(obj)->head;
  for (; entry != NULL;)
  {
    printf("ri mabi\n");
    if (entry)
    {
      key = (char *)entry->k;
      val = (struct json_object *)entry->v;
      entry = entry->next;
    }
    else
    {
      printf("mabi\n");
      break;
    }

    printf("jiangyibo sfdsfsa mabi\n");
    type = json_object_get_type(val);
    switch (type)
    {
    case json_type_string:
      tempVal = json_object_get_string(val);
      break;
    default:
      break;
    }
    printf("jyb test %s %s\n", key, tempVal);
    memset(tempstr, 0, 1024);
    sprintf(tempstr, "%s", key);
    value = NULL;
    if (external_get_action("value", tempstr, &value) == 0)
    {
      if (index++ == 0)
      {
        if (value == NULL)
        {
          sprintf(kvbuf, "\"%s\":\"\"", key);
        }
        else
        {
          sprintf(kvbuf, "\"%s\":\"%s\"", key, value);
        }
      }
      else
      {
        if (value == NULL)
        {
          sprintf(kvbuf, "%s,\"%s\":\"\"", kvbuf, key);
        }
        else
        {
          sprintf(kvbuf, "%s,\"%s\":\"%s\"", kvbuf, key, value);
        }
      }
      printf("jyb test  value %s \n", value);
      free(value);
      value = NULL;
    }
    else
    {
      if (value == NULL)
      {
        if (index++ == 0)
        {
          sprintf(kvbuf, "\"%s\":\"\"", value);
        }
        else
        {
          sprintf(kvbuf, "%s,\"%s\":\"\"", kvbuf, key);
        }
      }
    }

    if (entry == NULL)
    {
      break;
    }
  }
  memset(sendbuf, 0, 2048);
  sprintf(sendbuf, GetResponse, deviceMac, kvbuf);

  printf("jiangyibo send mmmmmm %s\n", sendbuf);

  httppost(sendbuf, strlen(sendbuf));
  printf("jiangyibo send mmmmmm  333\n");
  // rc = send(s, sendbuf, strlen(sendbuf), 0);

  return rc;
}

int jsonSetConfig(SOCKET s, json_object *config)
{
  int rc = 0;
  char tempstr[2048];
  char *tempVal = NULL;
  enum json_type type;
  int index = 0;
  json_object *obj = config;
  memset(tempstr, 0, 2048);
  int strlenssid = 0;
  char *pssid = NULL;

  if (config == NULL)
  {
    printf("jyb test %s\n", config);
    return;
  }

  char *key;
  struct json_object *val;
  struct lh_entry *entry = json_object_get_object(obj)->head;

  if (remove(fc_script_set_actions) == 0)
    printf("Removed %s\n", fc_script_set_actions);

  for (; entry != NULL;)
  {
    pssid = NULL;
    printf("ri mabi\n");
    if (entry)
    {
      key = (char *)entry->k;
      val = (struct json_object *)entry->v;
      entry = entry->next;
    }
    else
    {
      printf("mabi\n");
      break;
    }

    printf("jiangyibo sfdsfsa mabi\n");
    type = json_object_get_type(val);
    switch (type)
    {
    case json_type_string:
      tempVal = json_object_get_string(val);
      break;
    default:
      break;
    }

    printf("jyb test 55555555 %s %s\n", key, tempVal);
    if (key != NULL && !strcmp(key, "InternetGatewayDevice.DeviceInfo.wireless_ssid"))
    {
      strlenssid = strlen(tempVal);
      pssid = zstream_b64decode(tempVal, &strlenssid);
      tempVal = pssid;
    }

    if (external_set_action_write("value", key, tempVal))
    {
    }
    if (!pssid)
    {
      free(pssid);
    }

    if (entry == NULL)
    {
      break;
    }
  }
  external_set_action_execute();
  memset(tempstr, 0, 1024);
  sprintf(tempstr, SetResponse, deviceMac, "setok");

  // rc = send(s, tempstr, sizeof(tempstr), 0);

  return rc;
}

int read_mac()
{
  FILE *fp = NULL;
  char ch;
  char bufexe[128];
  char buffstr[4096];
  memset(deviceMac, 0, 13);
  memset(deviceMacFu, 0, 18);

  if ((fp = fopen("/dev/mtdblock2", "r")) == NULL)
  {
    printf("file cannot be opened/n");
  }
  fgets(buffstr, 32, fp);
  sprintf(deviceMac, "%02X%02X%02X%02X%02X%02X", 0xff & buffstr[4], 0xff & buffstr[5], 0xff & buffstr[6], 0xff & buffstr[7], 0xff & buffstr[8], 0xff & buffstr[9]);
  sprintf(deviceMacFu, "%02X:%02X:%02X:%02X:%02X:%02X", 0xff & buffstr[4], 0xff & buffstr[5], 0xff & buffstr[6], 0xff & buffstr[7], 0xff & buffstr[8], 0xff & buffstr[9]);
  /* 
  deviceMac[0] = buffstr[4];
  deviceMac[1] = buffstr[5];
  deviceMac[2] = buffstr[6];
  deviceMac[3] = buffstr[7];
  deviceMac[4] = buffstr[8];
  deviceMac[5] = buffstr[9];*/

  fclose(fp);
  fp = NULL;

  return 0;
}

static void sigHandle(int sig, struct siginfo *siginfo, void *myact)
{

  //printf("sig=%d siginfo->si_int=%d SIGALRM=%d,SIGSEGV=%d\n",sig,siginfo->si_int,SIGALRM,SIGSEGV);
  if (sig == SIGALRM)
  {
  }
  else if (sig == SIGSEGV)
  {
    sleep(1);
  }
}

static void sigInit()
{
  int i;
  struct sigaction act;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_SIGINFO;
  act.sa_sigaction = sigHandle;

  sigaction(SIGALRM, &act, NULL);
  sigaction(SIGSEGV, &act, NULL);
}

char cliBuff[1024];

char *exeShell(char *comm)
{
  FILE *fstream = NULL;

  int errnoT = 0;

  memset(cliBuff, 0, sizeof(cliBuff));

  if (NULL == (fstream = popen(comm, "r")))
  {
    fprintf(stderr, "execute command failed: %s", strerror(errno));
    return "error";
  }
  /*    if(NULL!=fread(cliBuff,1, sizeof(cliBuff), fstream))    
    {    
        printf("exeShell zhi\n");   
    }    
    else   
    {   
        pclose(fstream);   
        return cliBuff;   
    }   
    */
  pclose(fstream);

  return cliBuff;
}

int getCpuUsage()
{
  float sys_usage;
  float user_usage;
#define CPU_FILE_PROC_STAT "/proc/stat"
  FILE *fp = NULL;
  char tmp[10];
  unsigned long user, sys, nice, idle, total;

  fp = fopen(CPU_FILE_PROC_STAT, "r");
  if (fp == NULL)
  {
    return 10;
  }
  fscanf(fp, "%s %lu %lu %lu %lu", tmp, &user, &nice, &sys, &idle);

  fclose(fp);
  total = user + sys + nice + idle;
  if (total > 0)
  {
    sys_usage = sys * 100.0 / total;
    user_usage = user * 100.0 / total;
    return (int)((sys_usage + user_usage));
  }
  else
  {
    sys_usage = 0;
    user_usage = 0;
    return 10;
  }
  //cpu_rate = (1-idle/total)*100;

  return 0;
}

struct mem_usage_t
{
  unsigned long total;
  unsigned long used;
  unsigned long free;
  unsigned long shared;
  unsigned long buffers;
  unsigned long cached;
};

int getMemUsage()
{
  FILE *fp = NULL;
  struct mem_usage_t memge;
  struct mem_usage_t *usage;
  usage = &memge;
  char tmp[1024];
  char str[128];
  char str1[128];
  int total = 0, memfree = 0;
  int index = 0;
  char *t;

  fp = fopen("/proc/meminfo", "r");
  if (fp == NULL)
  {
    return 10;
  }

  while ((fgets(tmp, 1024, fp)) != NULL)
  {
    if (strstr(tmp, "MemTotal:"))
    {
      index = 0;
      t = strtok(tmp, " ");
      while (t != NULL)
      {
        index++;
        if (index == 2)
        {
          total = atoi(t);
          //  printf("%s\n", t);
        }
        t = strtok(NULL, " ");
      }
    }
    else if (strstr(tmp, "MemFree:"))
    {
      index = 0;
      t = strtok(tmp, " ");
      while (t != NULL)
      {
        index++;
        if (index == 2)
        {
          //   printf("%s\n", t);
          memfree = atoi(t);
        }
        t = strtok(NULL, " ");
      }
    }
    else
    {

      break;
    }
  }
  // printf("jiangyibo mem %d \n",(int)((memfree*100.0)/total));

  return (int)((memfree * 100.0) / total);
}

int getRunTime()
{

  FILE *fp = NULL;
  char tmp[128];
  int timeBuf = 0;

  fp = popen("cat /proc/uptime | awk -F \".\" '{ print $1 }'", "r");
  if (fp == NULL)
  {
    return 0;
  }
  if (fread(tmp, 1, 128, fp) > 0)
  {
    timeBuf = atoi(tmp);
  }
  else
  {
  }
  pclose(fp);
  //  printf("jiangyibo getRunTime %d\n", timeBuf);
  return timeBuf;
}

int getPortState(char *portstate)
{

  FILE *fp = NULL;
  char tmp[1024];
  int timeBuf = 0;
  char *port = portstate;
  int i;

  int portResult = 0;

  fp = popen("swconfig dev switch0 show 2>/dev/null", "r");
  if (fp == NULL)
  {
    return 0;
  }

  while ((fgets(tmp, 1024, fp)) != NULL)
  {
    if (strstr(tmp, "link: port:0 link:up"))
    {
      port[0] = 1;
    }
    else if (strstr(tmp, "link: port:1 link:up"))
    {
      port[1] = 1;
    }
    else if (strstr(tmp, "link: port:2 link:up"))
    {
      port[2] = 1;
    }
    else if (strstr(tmp, "link: port:3 link:up"))
    {
      port[3] = 1;
    }
    else if (strstr(tmp, "link: port:4 link:up"))
    {
      port[4] = 1;
    }
  }

  pclose(fp);

  return portResult;
}

int getDeviceSpeed(int *total, int *used)
{

  FILE *fp = NULL;
  char tmp[128];
  int timeBuf = 0;

  fp = popen("ifconfig br-lan|grep bytes|awk -F 'bytes:' '{print $2,$3}'|awk '{print $1,$5}'", "r");
  if (fp == NULL)
  {
    return 0;
  }
  fscanf(fp, "%lu %lu", total, used);
  //  printf("jiangyibo getRunTime %d %d\n", *total, *used);
  pclose(fp);
  return 0;
}

int getConnectNum(char *conn)
{

  FILE *fp = NULL;
  char tmp[128];
  int timeBuf = 0;
  int total;

  fp = popen("iwinfo ra0 a|grep RX|wc -l", "r");
  if (fp == NULL)
  {
    return 0;
  }
  fscanf(fp, "%lu", &total);
  //  printf("jiangyibo getConnectNum %d\n", total);
  pclose(fp);
  *conn = (char)total;
  return 0;
}

int spilt_string(char *string)
{
  int i = 0;
  const char *split = " ";
  char *p;

  p = strtok(string, split);
  while (p)
  {
    if (i == 1)
    {
      strcpy(string, p);
      //printf(" is : %s \n",string);
      return 0;
    }
    i++;
    p = strtok(NULL, split);
  }
  return -1;
}

int find_position(char *find)
{
  FILE *fp;
  char *p, buffer[128] = {0}; //初始化
  int ret;

  fp = fopen("/etc/config/wireless", "r");
  if (fp < 0)
  {
    printf("open file failed.\n");
    return -1;
  }

  //memset(buffer, 0, sizeof(buffer));
  fseek(fp, 0, SEEK_SET);
  while (fgets(buffer, 128, fp) != NULL)
  {
    p = strstr(buffer, find);
    if (p)
    {
      // printf("string is :%s \n",p);
      ret = spilt_string(p);
      if (ret == 0)
      {
        memset(find, 0, sizeof(find));
        strncpy(find, p, sizeof(p));
        return 0;
      }
    }
    memset(buffer, 0, sizeof(buffer));
  }

  fclose(fp);
  return -1;
}

int get_ower()
{
  char find[] = "Power";
  int ret;
  ret = find_position(&find);
  printf("ower --> %s", find);

  return 0;
}

void set_pifii_report(char *s1_addr,char *s1_port,char *s2_addr,char *s2_port,char *s2_path)
{
  char option[16];
  struct uci_context *ctx = uci_alloc_context(); //申请上下文
  struct uci_ptr ptr = {
      .package = "pifii",
      .section = "server",
      //.option = "value",
      //.value = "256",
  };
  memset(reportServerIp,0,32);
  strcpy(reportServerIp,s1_addr);
  memset(reportServerPort,0,32);
  strcpy(reportServerPort,s1_port);
  memset(httpPostServerIp,0,32);
  strcpy(httpPostServerIp,s2_addr);
  memset(httpPostServerPort,0,32);
  strcpy(httpPostServerPort,s2_port);
  memset(httpPostServerPath,0,32);
  strcpy(httpPostServerPath,s2_path);

  memset(option,0,16);
  strcpy(option,"reportaddr");
  ptr.option = option;
  ptr.value = s1_addr;
  uci_set(ctx, &ptr);             //写入配置

  memset(option,0,16);
  strcpy(option,"reportport");
  ptr.option = option;
  ptr.value = s1_port;
  uci_set(ctx, &ptr);             //

  memset(option,0,16);
  strcpy(option,"postaddr");
  ptr.option = option;
  ptr.value = s2_addr;
  uci_set(ctx, &ptr);             //写入配置

  memset(option,0,16);
  strcpy(option,"postport");
  ptr.option = option;
  ptr.value = s2_port;
  uci_set(ctx, &ptr);             //写入配置  

  memset(option,0,16);
  strcpy(option,"postpath");
  ptr.option = option;
  ptr.value = s2_path;
  uci_set(ctx, &ptr);             //写入配置    

  uci_commit(ctx, &ptr.p, false); //提交保存更改
  uci_unload(ctx, ptr.p);         //卸载包

  uci_free_context(ctx); //释放上下文
}


void set_pifii_uci(char *enable,char *weekdays,char *blacklist,char *timespan1,char *timespan2,char *timespan3)
{
  char option[16];
  struct uci_context *ctx = uci_alloc_context(); //申请上下文
  struct uci_ptr ptr = {
      .package = "pifii",
      .section = "server",
      //.option = "value",
      //.value = "256",
  };
  memset(option,0,16);
  strcpy(option,"enable");
  ptr.option = option;
  ptr.value = enable;
  uci_set(ctx, &ptr);             //写入配置
  memset(option,0,16);
  strcpy(option,"weekdays");
  ptr.option = option;
  ptr.value = weekdays;
  uci_set(ctx, &ptr);             //写入配置
  memset(option,0,16);
  strcpy(option,"blacklist");
  ptr.option = option;
  ptr.value = blacklist;
  uci_set(ctx, &ptr);             //写入配置
  memset(option,0,16);
  strcpy(option,"timespan1");
  ptr.option = option;
  ptr.value = timespan1;
  uci_set(ctx, &ptr);             //写入配置
  memset(option,0,16);
  strcpy(option,"timespan2");
  ptr.option = option;
  ptr.value = timespan2;
  uci_set(ctx, &ptr);             //写入配置
  memset(option,0,16);
  strcpy(option,"timespan3");
  ptr.option = option;
  ptr.value = timespan3;
  uci_set(ctx, &ptr);             //写入配置  
  uci_commit(ctx, &ptr.p, false); //提交保存更改
  uci_unload(ctx, ptr.p);         //卸载包

  uci_free_context(ctx); //释放上下文
}


struct msg_st
{
	long int msg_type;
	char text[MAX_TEXT];
};

int sendMsgQ()
{
	int running = 1;
	struct msg_st data;
	int msgid = -1;

	//建立消息队列
	msgid = msgget((key_t)1234, 0666 | IPC_CREAT);
	if(msgid == -1)
	{
		printf("msgget failed with error:\n");
	}

	//向消息队列中写消息，直到写入end


		data.msg_type = 1;    //注意2
		strcpy(data.text, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbo");
		//向队列发送数据
		if(msgsnd(msgid, (void*)&data, MAX_TEXT, 0) == -1)
		{
			printf("msgsnd failed\n");
		}

}


float wirelessConfig(struct uci_context *c, WirelessDates *pWireless)
{
  char buf[128];
  struct uci_ptr p;
  memset(pWireless, 0, sizeof(WirelessDates));
  sprintf(buf, "wireless.@wifi-iface[0].ssid");
  if (UCI_OK != uci_lookup_ptr(c, &p, buf, true))
  {
    //  sprintf(pWireless->wifidata[0].ssid, "");
  }
  else
  {
    if (p.o != NULL)
    {
      sprintf(pWireless->wifidata[0].ssid, p.o->v.string);
    }
    else
    {
    }
  }
  // printf("jiangyibo wireless get \n");
  sprintf(buf, "wireless.@wifi-iface[0].key");
  if (UCI_OK != uci_lookup_ptr(c, &p, buf, true))
  {
  }
  else
  {
    if (p.o != NULL)
    {
      sprintf(pWireless->wifidata[0].password, p.o->v.string);
    }
  }
  sprintf(buf, "wireless.@wifi-iface[0].encryption");
  if (UCI_OK != uci_lookup_ptr(c, &p, buf, true))
  {
    pWireless->wifidata[0].encryption = 0;
  }
  else
  {
    if (p.o != NULL)
    {
      if (!strcmp("psk2+aes", p.o->v.string))
      {
        pWireless->wifidata[0].encryption = 2;
      }
      else if (!strcmp("psk2", p.o->v.string))
      {
        pWireless->wifidata[0].encryption = 1;
      }
      else
      {
        pWireless->wifidata[0].encryption = 3;
      }
    }
  }
  sprintf(buf, "wireless.ra0.channel");
  if (UCI_OK != uci_lookup_ptr(c, &p, buf, true))
  {
    pWireless->wifidata[0].channel = 13;
  }
  else
  {
    if (p.o != NULL)
    {
      if (!strcmp(p.o->v.string, "auto"))
      {
        pWireless->wifidata[0].channel = 100;
      }
      else
      {
        //    printf("jiangyibo wireless get 223388 %d \n", p.o);
        pWireless->wifidata[0].channel = atoi(p.o->v.string);
      }
    }
  }
  sprintf(buf, "wireless.@wifi-iface[0].portel");
  if (uci_lookup_ptr(c, &p, buf, true))
  {
    //   printf("jiangyibo wireless get 23\n");
    pWireless->wifidata[0].portel = 0;
  }
  else
  {
    if (p.o != NULL)
    {
      pWireless->wifidata[0].portel = atoi(p.o->v.string);
    }
    else
    {
      pWireless->wifidata[0].portel = 0;
    }
  }
  sprintf(buf, "wireless.@wifi-iface[0].disabled");
  if (UCI_OK != uci_lookup_ptr(c, &p, buf, true))
  {
    pWireless->wifidata[0].disabled = 0;
  }
  else
  {
    if (p.o != NULL)
    {
      pWireless->wifidata[0].disabled = atoi(p.o->v.string);
    }
    else
    {
      pWireless->wifidata[0].disabled = 0;
    }
  }
  // printf("jiangyibo wireless get 3\n");
}

float networkConfig(struct uci_context *c, NetworkDate *pNet)
{
  char buf[128];
  struct uci_ptr p;
  memset(pNet, 0, sizeof(NetworkDate));

  sprintf(buf, "network.wan.proto");
  if (UCI_OK != uci_lookup_ptr(c, &p, buf, true))
  {
    pNet->mode = 0;
  }
  else
  {
    if (p.o != NULL)
    {
      if (!strcmp("dhcp", p.o->v.string))
      {
        pNet->mode = 1;
      }
      else if (!strcmp("pppoe", p.o->v.string))
      {
        pNet->mode = 2;
      }
      else if (!strcmp("static", p.o->v.string))
      {
        pNet->mode = 3;
      }
      else if (!strcmp("relay", p.o->v.string))
      {
        pNet->mode = 4;
      }
      else
      {
        pNet->mode = 0;
      }
    }
    else
    {
      pNet->mode = 0;
    }
  }
  sprintf(buf, "network.wan.username");
  if (UCI_OK != uci_lookup_ptr(c, &p, buf, true))
  {
  }
  else
  {
    if (p.o != NULL)
    {
      sprintf(pNet->username, p.o->v.string);
    }
  }
  sprintf(buf, "network.wan.password");
  if (UCI_OK != uci_lookup_ptr(c, &p, buf, true))
  {
  }
  else
  {
    if (p.o != NULL)
    {
      sprintf(pNet->password, p.o->v.string);
    }
  }

  sprintf(buf, "network.wan.ipaddr");
  if (UCI_OK != uci_lookup_ptr(c, &p, buf, true))
  {
  }
  else
  {
    if (p.o != NULL)
    {
      sprintf(pNet->ipaddr, p.o->v.string);
    }
  }
  sprintf(buf, "network.wan.netmask");
  if (UCI_OK != uci_lookup_ptr(c, &p, buf, true))
  {
  }
  else
  {
    if (p.o != NULL)
    {
      sprintf(pNet->network, p.o->v.string);
    }
  }
  sprintf(buf, "network.wan.gateway");
  if (UCI_OK != uci_lookup_ptr(c, &p, buf, true))
  {
  }
  else
  {
    if (p.o != NULL)
    {
      sprintf(pNet->gateway, p.o->v.string);
    }
  }
  sprintf(buf, "network.wan.dns");
  if (UCI_OK != uci_lookup_ptr(c, &p, buf, true))
  {
  }
  else
  {
    if (p.o != NULL)
    {
      sprintf(pNet->dns1, p.o->v.string);
    }
  }
  sprintf(buf, "network.wan.dns1");
  if (UCI_OK != uci_lookup_ptr(c, &p, buf, true))
  {
  }
  else
  {
    if (p.o != NULL)
    {
      sprintf(pNet->dns2, p.o->v.string);
    }
  }
}

void returnReportInfo(char *reportMsg)
{
    sprintf(reportMsg,"\"reportaddr\":\"%s\",\"reportport\":\"%s\",\"postaddr\":\"%s\",\"postport\":\"%s\",\"postpath\":\"%s\"",\
    reportServerIp,reportServerPort,httpPostServerIp,httpPostServerPort,httpPostServerPath);
}


void initReportConfig(struct uci_context *c)
{
  char buf[64];
  struct uci_ptr p;


  sprintf(buf, "pifii.server.reportaddr");
  if (UCI_OK != uci_lookup_ptr(c, &p, buf, true))
  {
    sprintf(reportServerIp,SERVER_IP);
  }
  else
  {
    if (p.o != NULL)
    {
       sprintf(reportServerIp,p.o->v.string);
    }else{
      sprintf(reportServerIp,SERVER_IP);
    }
  }

  sprintf(buf, "pifii.server.reportport");
  if (UCI_OK != uci_lookup_ptr(c, &p, buf, true))
  {
    sprintf(reportServerPort,SERVER_PORT);
  }
  else
  {
    if (p.o != NULL)
    {
       sprintf(reportServerPort,p.o->v.string);
    }
    else {
      sprintf(reportServerPort,SERVER_PORT);
    }
  }
  sprintf(buf, "pifii.server.postaddr");
  if (UCI_OK != uci_lookup_ptr(c, &p, buf, true))
  {
    sprintf(httpPostServerIp,HTTPPOST_IP);
  }
  else
  {
    if (p.o != NULL)
    {
       sprintf(httpPostServerIp,p.o->v.string);
    }else{
     sprintf(httpPostServerIp,HTTPPOST_IP);
    }
  }

  sprintf(buf,"pifii.server.postport");
  if (UCI_OK != uci_lookup_ptr(c, &p, buf, true))
  {
    sprintf(httpPostServerPort,HTTPPOST_PORT);
  }
  else
  {
    if (p.o != NULL)
    {
       sprintf(httpPostServerPort,p.o->v.string);
    }else{
      sprintf(httpPostServerPort,HTTPPOST_PORT);
    }
  }
    sprintf(buf,"pifii.server.postpath");
  if (UCI_OK != uci_lookup_ptr(c, &p, buf, true))
  {
    sprintf(httpPostServerPath,HTTPPOST_PORT);
  }
  else
  {
    if (p.o != NULL)
    {
       sprintf(httpPostServerPath,p.o->v.string);
    }else{
      sprintf(httpPostServerPath,HTTPPOST_PATH);
    }
  }
  
}
/*
int threadHome()
{
  int client_socket_fd;
  char recvData[1024];
  char sendData[1024];
  char tempstr[1500];

  int id = 1;
  json_object *pobj, *p1_obj, *p2_obj, *p3_obj = NULL;

  char *param_p1, *param_p2, *param_p3, *param_p4, *param_p5 = NULL;

  char *hsid,*hid,*hver,*hcmdtype;

  int param_int;

  char *typeE, *name, *command;

  char *dataE;

  int typeInt;

  int datalength;

    int len = 0;

  json_object *new_obj;

  int length;
  int rc;
  int commandId;

  int server_socket_fd = socket(AF_INET, SOCK_DGRAM, 0); 
  if(server_socket_fd == -1) 
  { 
    perror("Create Socket Failed:"); 
    exit(1); 
  } 
  

  if(-1 == (bind(server_socket_fd,(struct sockaddr*)&server_addr,sizeof(server_addr)))) 
  { 
    perror("Server Bind Failed:"); 
    exit(1); 
  } 

      struct sockaddr_in client_addr; 
    socklen_t client_addr_length = sizeof(client_addr); 

  
  while(1) {
    memset(recvData, 0, 4096);
    if(recvfrom(server_socket_fd, recvData, sizeof(recvData),0,(struct sockaddr*)&client_addr, &client_addr_length) == -1) 
    { 
    printf("jiangyibo 888%s\n", recvData);
    //     new_obj = json_tokener_parse(TestJson);
    new_obj = json_tokener_parse(recvData);
    if (is_error(new_obj))
    {
      printf("jiangyibo error para%s\n");
      // rc = send(s, ErrorJson, sizeof(ErrorJson), 0);
    }
    else
    {

      hsid = GetValByEtype(new_obj, "sid");
      hid = GetValByEtype(new_obj, "id");
      hver = GetValByEtype(new_obj, "ver");
      hcmdtype = GetValByEtype(new_obj, "cmdtype");
      //typeE = GetValByEtype(new_obj, "params");
      printf("jiangyibo name %s\n", name);
      if (hsid == NULL||hid==NULL||hver==NULL||hcmdtype==NULL)
      {
        
        //发送  的json 错误
      }
      else if (!strcmp(hcmdtype, "4097"))
      {
            memset(sendData,0,1024);
            sprintf(sendData,HomeResponse,hsid,hid,hver);
            if (sendto(server_socket_fd, sendData, strlen(sendData), 0, (struct sockaddr*)&client_addr, client_addr_length)) < 0)
            {
              printf("Send File Name Failed:");
        
            }
            else{
              printf("Send ok\n");
            }
      }
      else if (!strcmp(hcmdtype, "get"))
      {
 
      }
      else if (!strcmp(hcmdtype, "set"))
      {
 
      }
      else
      {

      }
      json_object_put(new_obj);
    }
  }
  
}
*/

int threadUdp(int *socket_fd)
{
  int client_socket_fd = *socket_fd;
  char recvData[4096];
  char sendmsgData[1500];
  char tempstr[1500];
  char *wificlient=NULL;

  int id = 1;
  json_object *pobj, *p1_obj, *p2_obj, *p3_obj = NULL;

  char *param_p1, *param_p2, *param_p3, *param_p4, *param_p5 = NULL,*param_p6;

  char *hsid, *hid, *hver, *hcmdtype,*hdevtype,*hstatstr;

  int param_int;

  char *typeE, *name, *command;

  char *dataE;

  int typeInt;

  int datalength;

  int len = 0;

  json_object *new_obj;

  int length;
  int rc;
  int commandId;

  struct sockaddr_in server_addr;
  bzero(&server_addr, sizeof(server_addr));
  socklen_t server_addr_length = sizeof(server_addr);
  while (1)
  {
    bzero(&server_addr, sizeof(server_addr));
    server_addr_length = sizeof(server_addr);
    memset(recvData, 0, 4096);
    if ((len = recvfrom(client_socket_fd, recvData, sizeof(recvData), 0, (struct sockaddr *)&server_addr, &server_addr_length)) > 0)
    {
      printf("jiangyibo 888%s\n", recvData);
      //     new_obj = json_tokener_parse(TestJson);
      new_obj = json_tokener_parse(recvData);
      printf("jiangyibo 888 333%s\n", recvData);
      if (new_obj==NULL||is_error(new_obj))
      {
        printf("jiangyibo error para%s\n");
        // rc = send(s, ErrorJson, sizeof(ErrorJson), 0);
      }
      else
      {
        name = NULL;

        name = GetValByEtype(new_obj, "name");

        //typeE = GetValByEtype(new_obj, "params");
        printf("jiangyibo recv name %s\n", name);
        if (name == NULL)
        {
          //rc = send(client_socket_fd, ErrorJson, sizeof(ErrorJson), 0);
          //发送  的json 错误
          hsid = GetValByEtype(new_obj, "sid");
          hid = GetValByEtype(new_obj, "id");
          hver = GetValByEtype(new_obj, "ver");
          hcmdtype = GetValByEtype(new_obj, "cmdtype");
          if (hsid == NULL || hid == NULL || hver == NULL || hcmdtype == NULL)
          {

            //发送  的json 错误
          }
          else if (!strcmp(hcmdtype, "4097"))
          {
            //
            hdevtype = GetValByEtype(new_obj, "devtype");
            hstatstr = GetValByEtype(new_obj, "statstr");
            AddHomeDevice(hid, server_addr.sin_addr.s_addr,hdevtype,hstatstr);
            memset(sendmsgData, 0, 1500);
            sprintf(sendmsgData, HomeResponse, hsid, hid, hver);

             printf("jiangyibo homedevice %s\n",sendmsgData);
            if (sendto(client_socket_fd, sendmsgData, strlen(sendmsgData), 0, (struct sockaddr *)&server_addr, server_addr_length) < 0)
            {
              printf("Send File Name Failed:");
            }
            else
            {
              printf("Send ok\n");
            }
          }
          else if (!strcmp(hcmdtype, "5001")||!strcmp(hcmdtype, "5002")||!strcmp(hcmdtype, "5003"))  //重启
          { 
            commandSendtoHomeDevice(client_socket_fd,hid,recvData);
          }
          else
          {
          }
        }
        else if (!strcmp(name, "informResponse"))
        {
          printf("jyb test 11 %d\n", commandId);
          commandId = GetIntByEtype(new_obj, "commandEvent");

          printf("jyb test 11 %d\n", commandId);
          if (commandId == 0)
          {
            printf("jyb test 11\n");
            //   if (pReal->tr069state == 3)
            {
              //exeShell("/etc/init.d/freecwmpd stop&");
            }
          }
          else if (commandId == 1)
          {
            // if (pReal->tr069state == 4)
            {
              // exeShell("/etc/init.d/freecwmpd start&");
            }
          }
          else if (commandId == 5)
          {
            //if (pReal->tr069state == 3)
            {
              // exeShell("/etc/init.d/freecwmpd stop&");
            }
          }
          else if (commandId == 6)
          {

            system("reboot -f");
          }
          else if (commandId == 7)
          {

            system("uci set pifii.register.udpport=1&&uci commit pifii");
          }
          else if (commandId == 8)
          {
            system("/usr/sbin/updateUdpReport.sh &");
          }
          else
          {
          }
          //发送  的定时上报报文
        }
        else if (!strcmp(name, "get"))
        {
          command = GetValByEtype(new_obj, "keyname");
          if (command == NULL)
          {
          }
          else if (strcmp(command, "config") == 0)
          {
            p1_obj = json_object_object_get(new_obj, "packet");
            jsonGetConfig(client_socket_fd, p1_obj);
          }
          else if (strcmp(command, "getvalue") == 0)
          {
            memset(sendmsgData, 0, 1500);
            getFileData(tempstr, "config.json");
            sprintf(sendmsgData, ConfigJson, deviceMac, tempstr);
            rc = send(client_socket_fd, (char *)sendmsgData, sizeof(sendmsgData), 0);
          }
          else if (strcmp(command, "inform") == 0)
          {
            memset(tempstr, 0, 1500);
            if (external_get_action("value", "InternetGatewayDevice.LANDevice.1.Wireless.WiFiClient", &wificlient) == 0)
            {
                printf("jiangyibo wificlient %s\n",wificlient);
             
            }
            if(wificlient==NULL)
            {
              sprintf(tempstr, informRes, deviceMac,"[]");
              free(wificlient);
              httppost(tempstr, strlen(tempstr));
            }else {
              sprintf(tempstr, informRes, deviceMac,wificlient);
              free(wificlient);
              httppost(tempstr, strlen(tempstr));
            }
          }
          else if (strcmp(command, "reportconfig") == 0)
          {
            memset(sendmsgData, 0, 1500);
            returnReportInfo(sendmsgData);
            sprintf(tempstr, GetReport, deviceMac, sendmsgData);
            httppost(tempstr, strlen(tempstr));
          }          
          else if (strcmp(command, "command") == 0)
          {
            memset(sendmsgData, 0, 1024);
            p1_obj = GetValByEdata(new_obj, "packet");
            param_p1 = GetValByKey(p1_obj, "shellcmd");
            param_p2 = exeShell(param_p1);
            length = strlen(param_p2);
            param_p3 = zstream_b64encode(param_p2, &length);
            printf("jiangyibo %s\n", param_p3);
            sprintf(sendmsgData, CommandJson, deviceMac, param_p3);
            free(param_p3);
            rc = send(client_socket_fd, (char *)sendmsgData, sizeof(sendmsgData), 0);
          }
          else if (strcmp(command, "file") == 0)
          {
            memset(sendmsgData, 0, 1500);
            p1_obj = GetValByEdata(new_obj, "packet");
            param_p1 = GetValByKey(p1_obj, "shellcmd");
            printf("jyb test %s\n", param_p1);
            if (param_p1 != NULL)
            {
              if (getConfigFile(tempstr, param_p1) != 0)
              {
                length = strlen(tempstr);
                param_p3 = zstream_b64encode(tempstr, &length);

                memset(sendmsgData, 0, 1500);
                sprintf(sendmsgData, FileJson, deviceMac, param_p1, param_p3);
                free(param_p3);
                // rc = send(client_socket_fd, (char *)sendmsgData, sizeof(sendmsgData), 0);
              }
              else
              {
                memset(sendmsgData, 0, 1500);
                // rc = send(client_socket_fd, ErrorJson, sizeof(ErrorJson), 0);
              }
            }
          }
          else
          {
            memset(sendmsgData, 0, 1500);
            rc = send(client_socket_fd, ErrorJson, sizeof(ErrorJson), 0);
          }
        }
        else if (!strcmp(name, "set"))
        {
          char *c = NULL;

          command = GetValByEtype(new_obj, "keyname");
          printf("jiangyibo eeee 333 %s\n", command);
          if (!strcmp(command, "config"))
          {
            p1_obj = json_object_object_get(new_obj, "packet");
            jsonSetConfig(client_socket_fd, p1_obj);
            printf("jyb test ok\n");
            memset(tempstr, 0, 1024);
            sprintf(tempstr, SetResponse, deviceMac, command, "setok");
            httppost(tempstr, strlen(tempstr));
          }
          else if (!strcmp(command, "reboot"))
          {
            memset(tempstr, 0, 1024);
            sprintf(tempstr, SetResponse, deviceMac, command, "setok");
            httppost(tempstr, strlen(tempstr));
            system("reboot");
          }
          else if (!strcmp(command, "download"))
          {
            memset(tempstr, 0, 1024);
            sprintf(tempstr, DownloadResponse, deviceMac, command, "setok", "PF-308", "pifii", "1");
            httppost(tempstr, strlen(tempstr));

            p1_obj = GetValByEdata(new_obj, "packet");
            param_p1 = GetValByKey(p1_obj, "url");
            param_p2 = GetValByKey(p1_obj, "FileSize");
            commandDownload(param_p1, param_p2);
          }
          else if (!strcmp(command, "factory"))
          {
            memset(tempstr, 0, 1024);
            sprintf(tempstr, SetResponse, deviceMac, command, "setok");
            httppost(tempstr, strlen(tempstr));

            commandFactoryset();
          }
          else if (!strcmp(command, "sysupgrade"))
          {
            memset(tempstr, 0, 1024);
            sprintf(tempstr, SetResponse, deviceMac, command, "setok");
            httppost(tempstr, strlen(tempstr));

            p1_obj = GetValByEdata(new_obj, "packet");
            param_p1 = GetValByKey(p1_obj, "url");
            param_p2 = GetValByKey(p1_obj, "size");
            system("/usr/sbin/updateUdpReport.sh &");
          }
          else if (!strcmp(command, "reportconfig"))
          {
            memset(tempstr, 0, 1024);
            sprintf(tempstr, SetResponse, deviceMac, command, "setok");
            httppost(tempstr, strlen(tempstr));

            p1_obj = GetValByEdata(new_obj, "packet");
            param_p1 = GetValByKey(p1_obj, "reportaddr");
            param_p2 = GetValByKey(p1_obj, "reportport");
            param_p3 = GetValByKey(p1_obj, "postaddr");
            param_p4 = GetValByKey(p1_obj, "postport");       
            param_p5 = GetValByKey(p1_obj, "postpath");                     
            set_pifii_report(param_p1,param_p2,param_p3,param_p4,param_p5);
          }          
          else if (!strcmp(command, "homedevice"))
          {
            memset(tempstr, 0, 1024);
            sprintf(tempstr, DownloadResponse, deviceMac, command, "setok", "PF-308", "pifii", "1");
            httppost(tempstr, strlen(tempstr));

          }
          else if (!strcmp(command, "blackmac"))
          {
                        printf("jiangyibo blackmac oooo 222\n");
            memset(tempstr, 0, 1024);
            sprintf(tempstr, SetResponse, deviceMac, command, "setok");
            httppost(tempstr, strlen(tempstr));
            printf("jiangyibo blackmac oooo\n");
            p1_obj = GetValByEdata(new_obj, "packet");
            param_p1 = GetValByKey(p1_obj, "enable");
            param_p2 = GetValByKey(p1_obj, "weekdays");            
            param_p3 = GetValByKey(p1_obj, "blacklist");
            param_p4 = GetValByKey(p1_obj, "timespan1");
            param_p5 = GetValByKey(p1_obj, "timespan2");
            param_p6 = GetValByKey(p1_obj, "timespan3");   
            set_pifii_uci(param_p1,param_p2,param_p3,param_p4,param_p5,param_p6);
            sendMsgQ();

          }
          else
          {

            //rc = send(client_socket_fd, ErrorJson, sizeof(ErrorJson), 0);
          }
        }
        else
        {

          // rc = send(client_socket_fd, ErrorJson, sizeof(ErrorJson), 0);
          //发送   的json 错误
        }
        json_object_put(new_obj);
      }
      
      
    }
  }
}

int main(int argc, char *argv[])
{
  char infomsg[1500];
  char commandkey[] = "inform";
  char sendData[1500];
  char strhomeState[1500];
      int uptime = 0;
      int cpuload = 0;
  int length;
  int rc;
  int commandId;
  int i;
  int ret;
  pthread_t id1, id2;

  sigInit();
  read_mac();
  initHomeDevice();
  pthread_mutex_init(&mutex,NULL); 

  memset(informRes, 0, 1500);
  memset(infomsg, 0, 1500);
  getFileData(infomsg, "inform.json");
  getFileData(informRes, "informResponse.json");

  printf("send ok tit\n");

  int id = 0;

  SendPack sendmsg;
  int index = 0;
  char *p;
  RealTimeDate *pReal;
  WirelessDates *pWireless;
  NetworkDate *pNet;

  struct uci_context *c;

  pReal = sendmsg.data;
  pWireless = sendmsg.data + sizeof(RealTimeDate);
  pNet = sendmsg.data + sizeof(RealTimeDate) + sizeof(WirelessDates);

  sendmsg.version = APP_VERSION;
  sendmsg.id = 222;
  memcpy(sendmsg.devid, deviceMac, 6);
  sendmsg.bufsize = 1500;
  sprintf(pReal->equipment, "TZ");
  sprintf(pReal->hardwaretype, "PF308-TZ-H");
  sprintf(pReal->softwaretype, "1.6.12");

  pReal->aprouter = 1;
  pWireless->wifinum = 1;

  sendmsg.version = APP_VERSION;

  /* 服务端地址 */
  struct sockaddr_in server_addr;
  struct sockaddr_in client_addr;
  bzero(&server_addr, sizeof(server_addr));
  bzero(&client_addr, sizeof(client_addr));
  server_addr.sin_family = AF_INET;


  socklen_t server_addr_length = sizeof(server_addr);

  /* 创建socket */
  int client_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (client_socket_fd < 0)
  {
    perror("Create Socket Failed:");
    exit(1);
  }

  client_addr.sin_family = AF_INET;
  client_addr.sin_port = htons(ROUTER_PORT);
  client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  /* 绑定 */
  if (-1 == (bind(client_socket_fd, (struct sockaddr *)&client_addr, sizeof(client_addr))))
  {
    printf("Server Bind \n");
  }

  struct timeval timeout = {5, 0};
  setsockopt(client_socket_fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(struct timeval));
  setsockopt(client_socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(struct timeval));

  RecvPack pack_info;

  ret = pthread_create(&id1, NULL, (void *)threadUdp, &client_socket_fd); //创建线程1
  if (ret != 0)
  {
    printf("create pthread error !\n");
  }
  else
  {
  }

  int len = 0;
  int temp = 1;
  int inSpeed = 0, outSpeed = 0;
  int looptimes = 10;
  int homeOrRoute = 0;
  char *wificlient=NULL;

    c = uci_alloc_context();
    //   printf("jiangyibo wireless 22\n");
    wirelessConfig(c, pWireless);
    //   printf("jiangyibo wireless\n");
    networkConfig(c, pNet);
    //   printf("jiangyibo net\n");
    initReportConfig(c);

    uci_free_context(c);



  while (1)
  {
    //  printf("jiangyibo while\n");
    if (getPidByName("pidof freecwmpd") < 1)
      pReal->tr069state = 4;
    else
      pReal->tr069state = 3;
    if (looptimes++ >= 10)
    {
      looptimes = 0;
      getPortState(pReal->portstate);
      cpuload = getCpuUsage();
      pReal->memload = getMemUsage();
      getDeviceSpeed(&pReal->upflow, &pReal->downflow);
      uptime = getRunTime();
      pReal->cputype = 1;
      getConnectNum(&pReal->connectnum);
    }



    memset(sendData, 0, 1500);


    server_addr.sin_addr.s_addr = inet_addr(reportServerIp);

    server_addr.sin_port = htons(atoi(reportServerPort));

    printf("jiangyibo report port %s %s\n",reportServerIp,reportServerPort);


    
    if (homeOrRoute % 2 == 0)
    {
      sprintf(sendData, infomsg, deviceMac, commandkey, deviceMac,HARD_VERSION,SOFT_VERSION, cpuload,uptime);
      printf("send ok\n%s\n", sendData);
      if (sendto(client_socket_fd, sendData, strlen(sendData), 0, (struct sockaddr *)&server_addr, server_addr_length) < 0)
      {
        printf("Send File Name Failed:");
        //exit(1);
      }
    }
    else
    {
      memset(strhomeState, 0, 1500);
      GetHomeDevice(strhomeState);
      sprintf(sendData, HomeDeviceState, deviceMacFu, strhomeState);
      printf("send ok\n%s\n", sendData);
      if (sendto(client_socket_fd, sendData, strlen(sendData), 0, (struct sockaddr *)&server_addr, server_addr_length) < 0)
      {
        printf("Send File Name Failed:");
        //exit(1);
      }
    }
    /* 从服务器接收数据，并写入文件 */

    sleep(20);
    homeOrRoute++;
    //break;
  }

  printf("cucuo \n");
  /*
  alarm(30);

  while (1)
  {	
    pause();
  }
*/
  close(client_socket_fd);
  return 0;
}
