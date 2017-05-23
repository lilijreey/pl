#ifndef _CONF_H_
#define _CONF_H_

#include "my_include.h"
#include <string>

#pragma pack(1)

/*************************************************************************************
    KEY_VALUE���ò���
**************************************************************************************/

class CConf
{
public:
	CConf();
	~CConf();

	//��ʼ�������ļ�
	TINT32 Init(const TCHAR *pszServFile, const TCHAR *pszModuleFile, const TCHAR *pszConfFile);

    TCHAR m_szProject[64];
    TCHAR m_szTblProject[64];
    TCHAR m_szModuleName[64];


    // ======================================================== //
	//����ip
	TCHAR m_szServerIp[20];
	//����˿�
	TUINT16 m_uwServerPort;

    TUINT16 m_uwRegPort;

    // ======================================================== //
	//���еĴ�С
    TUINT32 m_udwWorkQueSize;
	//�����̸߳���
    TUINT32 m_udwWorkThreadNum;

    // ======================================================== //
    // ��д���еĴ�С
    TUINT32 m_udwWritebackQueSize;
    // ��д����ʲôʱ�򴥷���д�Ĵ�С
    TUINT32 m_udwWritebackQueInitSize;
    // ��д���е���󳤶�
    TUINT32 m_udwWritebackQueMaxSize;
    // ��д�̸߳���
    TUINT32 m_udwWritebackThreadNum;
   
    // ��д��ʱ����
    TUINT32 m_udwWritebackInterval;
    // ÿ�λ�д��Item����
    TUINT32 m_udwWritebackSpeed;
    // ��д�����������ٶȣ����ڶ�̬������д�ٶ�
    TUINT32 m_udwWritebackSpeedFactor;
    // ��д�����Դ���
    TUINT32 m_udwMaxRetryTime;
    //��д�������ʱ
    TUINT32 m_udwWritebackDelay;


    // ======================================================== //
    // �ڴ������Ĳ���
    TUINT32 m_udwCacheThreshold;
    TUINT32 m_udwCacheLimit;
    TUINT32 m_udwCacheLRULoopNum;

    TUINT32 m_udwIdleItemMaxSize;
    TUINT32 m_udwIdleItemInitSize;
    TUINT32 m_udwIdleItemSizeFactor;



	// �澯����
	TCHAR m_project_name[DEFAULT_NAME_STR_LEN];
	TUINT32 m_stat_interval_time;
	TUINT32 m_need_send_message;
	TUINT32 m_error_num_threshold;
	TUINT32 m_error_rate_threshold;
};

#pragma pack()

#endif
