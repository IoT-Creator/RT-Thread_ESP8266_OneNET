#include <string.h>
#include <stdarg.h>	
#include <stdio.h>
#include <sys/socket.h>
#include <at_socket.h>
#include <time.h>
#include "bh1750.h"

#define DBG_ENABLE
#define DBG_SECTION_NAME "onenet.sample"
#define DBG_LEVEL DBG_LOG
#define DBG_COLOR
#include <rtdbg.h>

#define DEVICE_ID	"505619290"	
#define API_KEY		"SlxhH3MCLvuuvXJ0N=a14Yo6EAQ="
#define DATA_STREAM	"light"

static rt_thread_t recv_data_pro_thread = RT_NULL;

extern bh1750_device_t bh1750_dev;
rt_int32_t g_socket_id = -1;
char g_post_data_src[512];
char g_get_data_src[512];

/**************************************************************
��������:	socket_create
��������:	socket����
�������:	��
�� �� ֵ:	socket_id
��    ע:	��
**************************************************************/
rt_int32_t socket_create(void)
{
	rt_int32_t socket_id = -1;
	unsigned char domain = AF_INET;
    unsigned char type = SOCK_STREAM;
    unsigned char protocol = IPPROTO_IP;
	
	socket_id = at_socket(domain, type, protocol);

	if(socket_id < 0)
	{
		LOG_E("Socket create failure, socket_id:%d\n", socket_id);
	}
	else
	{
		LOG_I("Scoket created successfully, socket_id:%d\n", socket_id);
	}
	
	return socket_id;
}

/**************************************************************
��������:	socket_connect
��������:	socket ����Զ�̷�����
�������:	socket_id������socketʱ���ص�id
			remote_addr:Զ�̷�������ַ
			remote_port:�˿�
�� �� ֵ:	RT_EOK�����ӳɹ���RT_ERROR������ʧ��
��    ע:	��
**************************************************************/
rt_err_t socket_connect(rt_int32_t socket_id, char *remote_addr, rt_int32_t remote_port)
{
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	
	addr.sin_family = AF_INET;
	addr.sin_port = htons(remote_port);
	addr.sin_addr.s_addr = inet_addr(remote_addr);
	
	if(0 == at_connect(socket_id, (struct sockaddr *)&addr, sizeof(addr)))
	{	
		LOG_I("Socket connected to the remote server successfully\n");
		return RT_EOK;
	}
	else
	{
		LOG_E("Socket connected to the remote server failure\n");
		return RT_ERROR;
	}
}

/**************************************************************
��������:	socket_close
��������:	�ر�socket
�������:	socket_id������socketʱ���ص�id
�� �� ֵ:	RT_EOK���رճɹ���RT_ERROR���ر�ʧ��
��    ע:	��
**************************************************************/
rt_err_t socket_close(rt_int32_t socket_id)
{
	if(0 == at_closesocket(socket_id))
	{
		LOG_I("Socket closed successfully\n");
		return RT_EOK;
	}
	else
	{
		LOG_E("Socket closed failure\n");
		return RT_ERROR;
	}
}

/**************************************************************
��������:	socket_send
��������:	socket ��������
�������:	socket_id������socketʱ���ص�id
			data_buf:���ݰ�
			data_len:���ݰ���С
�� �� ֵ:	���ͳɹ��������ݰ���С
��    ע:	��
**************************************************************/
rt_int32_t socket_send(int socket_id, char *data_buf, int data_len)
{
	rt_int32_t send_rec  = 0;

	send_rec = at_send(socket_id, data_buf, data_len, 0);

	return send_rec;
}

/**************************************************************
��������:	socket_receive
��������:	socket ���շ������·�������
�������:	socket_id������socketʱ���ص�id
			data_buf���洢���յ���������
			data_len�������մ�С
�� �� ֵ:	�ɹ�ʱ���ش���0
��    ע:	��
**************************************************************/
rt_int32_t socket_receive(rt_int32_t socket_id, char *data_buf, rt_int32_t data_len)
{
	rt_int32_t recv_result = 0;

	recv_result = at_recv(socket_id, data_buf, data_len, MSG_DONTWAIT);

	return recv_result;
}

/**************************************************************
��������:	receive_onenet_http_process_thread
��������:	����ƽ̨�·��������
�������:	parameter:�߳���ڲ���
�� �� ֵ:	��
��    ע��	��
**************************************************************/
void receive_onenet_http_process_thread(void *parameter)
{
	char data_ptr[384];
	memset(data_ptr, 0, sizeof(data_ptr));
	
	while(1)
	{
		if(socket_receive(g_socket_id, data_ptr, 384) > 0)
		{
			rt_kprintf("\r\nSocket receive data is:\r\n%s\r\n", data_ptr);
			memset(data_ptr, 0, sizeof(data_ptr));
		}
		rt_thread_mdelay(1);
		rt_thread_yield();/* ����ʣ��ʱ��Ƭ������һ���߳��л� */
	}
}

/**************************************************************
��������:	connect_onenet_http_device
��������:	����OneNETƽ̨�豸
�������:	��
�� �� ֵ:	��
��    ע:	��
**************************************************************/
void connect_onenet_http_device(void)
{
	if(g_socket_id >= 0)
	{
		socket_close(g_socket_id);
	}
	
	g_socket_id = socket_create();
	if(g_socket_id >= 0)
	{
		if(RT_EOK == socket_connect(g_socket_id, "183.230.40.33", 80))
		{
			rt_thread_delay(500);
			rt_thread_resume(recv_data_pro_thread);
		}
	}
}

/**************************************************************
��������:	disconnect_onenet_http_device
��������:	�Ͽ�����OneNETƽ̨�豸
�������:	��
�� �� ֵ:	��
��    ע:	��
**************************************************************/
void disconnect_onenet_http_device(void)
{
	if(RT_EOK == socket_close(g_socket_id))
	{
		rt_thread_delay(500);
		rt_thread_suspend(recv_data_pro_thread);
	}
}

/**************************************************************
��������:	post_data_process
��������:	post���ݴ���
�������:	dev_id:�豸ID��api_key��APIKEY
�� �� ֵ:	��
��    ע��	��
**************************************************************/
void post_data_process(char *dev_id, char *api_key)
{
	float post_value_point = 0.0;
	char post_content[256];
	char post_content_len[4];
	
	memset(post_content, 0, sizeof(post_content));
	memset(g_post_data_src, 0, sizeof(g_post_data_src));
	
	post_value_point = bh1750_read_light(bh1750_dev);
	#if 1
	sprintf(post_content,"{\"datastreams\":[{\"id\":\"%s\",\"datapoints\":[{\"value\":%0.1f}]}]}", (char*)DATA_STREAM, post_value_point);
	#else
	strcat(post_content, "{\"datastreams\":[{\"id\":\"");
	strcat(post_content, g_post_data_stream_name);
	strcat(post_content, "\",\"datapoints\":[{\"value\":");
	strcat(post_content, g_post_value_point);
	strcat(post_content, "}]}]}");
	#endif
	sprintf(post_content_len, "%d", strlen(post_content));
	
	strcat(g_post_data_src, "POST /devices/");
	strcat(g_post_data_src, dev_id);
	strcat(g_post_data_src, "/datapoints HTTP/1.1\r\n");
	strcat(g_post_data_src, "api-key:");
	strcat(g_post_data_src, api_key);
	strcat(g_post_data_src, "\r\n");
	strcat(g_post_data_src, "Host:api.heclouds.com\r\n");
	strcat(g_post_data_src, "Content-Length:");
	strcat(g_post_data_src, post_content_len);
	strcat(g_post_data_src, "\r\n\r\n");
	strcat(g_post_data_src, post_content);
	strcat(g_post_data_src, "\r\n\r\n");
	
	LOG_I("\r\n%s\r\n", g_post_data_src);
}

/**************************************************************
��������:	get_data_process
��������:	get���ݴ���
�������:	dev_id:�豸ID��api_key��APIKEY
�� �� ֵ:	��
��    ע��	��
**************************************************************/
void get_data_process(char *dev_id, char *api_key)
{
	memset(g_get_data_src, 0, sizeof(g_get_data_src));
	
	sprintf(g_get_data_src,"GET http://api.heclouds.com/devices/%s/datapoints?datastream_id=%s HTTP/1.1\r\n", dev_id, DATA_STREAM);
	strcat(g_get_data_src, "api-key:");
	strcat(g_get_data_src, api_key);
	strcat(g_get_data_src, "\r\n");
	strcat(g_get_data_src, "Host:api.heclouds.com\r\n");
	strcat(g_get_data_src, "\r\n\r\n");
	
	LOG_I("\r\n%s\r\n", g_get_data_src);
}


/**************************************************************
��������:	post_data_stream_to_onenet
��������:	post��������OneNET
�������:	��
�� �� ֵ:	��
��    ע:	��
**************************************************************/
void post_data_stream_to_onenet(void)
{
	post_data_process(DEVICE_ID, API_KEY);
	socket_send(g_socket_id, g_post_data_src, strlen(g_post_data_src));
}

/**************************************************************
��������:	get_data_stream_from_onenet
��������:	get��������OneNET
�������:	��
�� �� ֵ:	��
��    ע:	��
**************************************************************/
void get_data_stream_from_onenet(void)
{
	get_data_process(DEVICE_ID, API_KEY);
	socket_send(g_socket_id, g_get_data_src, strlen(g_get_data_src));
}

rt_err_t onenet_sample(void)
{
	/* ƽ̨�·�������������߳� */
	recv_data_pro_thread = rt_thread_create("recv_dat", receive_onenet_http_process_thread, RT_NULL, 4096, 20, 100);
    if(recv_data_pro_thread == RT_NULL)
    {
    	LOG_I("recv_data_pro_thread create failed\n");
       	return RT_ERROR;
    }
	rt_thread_startup(recv_data_pro_thread);
	
	rt_thread_delay(500);
	
	rt_thread_suspend(recv_data_pro_thread);
	
	return RT_EOK;	
}

void esp8266(int argc, char *argv[])
{
    if (argc > 1)
    {
        if (!strcmp(argv[1], "connect"))
        {
			connect_onenet_http_device();
			//rt_thread_resume(recv_data_pro_thread);
        }
        else if (!strcmp(argv[1], "post"))
        {
            post_data_stream_to_onenet();
        }
		else if (!strcmp(argv[1], "get"))
        {
            get_data_stream_from_onenet();
        }
		else if (!strcmp(argv[1], "close"))
		{
			disconnect_onenet_http_device();
			//socket_close(g_socket_id);
			//rt_thread_suspend(recv_data_pro_thread);
		}
        else
        {
            rt_kprintf("Unknown command. Please enter 'esp8266' for help\n");
        }
    }
    else
    {
        rt_kprintf("Usage:\n");
        rt_kprintf("esp8266 connect   	- esp8266 connect to onenet http server\n");
		rt_kprintf("esp8266 post   		- esp8266 post data stream to onenet http server\n");
		rt_kprintf("esp8266 get   		- esp8266 get data stream from onenet http server\n");
		rt_kprintf("esp8266 close   	- esp8266 close connection\n");
    }
}
MSH_CMD_EXPORT(esp8266, esp8266 module function);










