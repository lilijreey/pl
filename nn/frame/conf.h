#ifndef _CONF_H_
#define _CONF_H_

#include "my_include.h"
#include <string>

#pragma pack(1)

/*************************************************************************************
    KEY_VALUE配置参数
**************************************************************************************/

class CConf
{
public:
	CConf();
	~CConf();

	//初始化配置文件
	TINT32 Init(const TCHAR *pszServFile, const TCHAR *pszModuleFile, const TCHAR *pszConfFile);

    TCHAR m_szProject[64];
    TCHAR m_szTblProject[64];
    TCHAR m_szModuleName[64];


    // ======================================================== //
	//服务ip
	TCHAR m_szServerIp[20];
	//服务端口
	TUINT16 m_uwServerPort;

    TUINT16 m_uwRegPort;

    // ======================================================== //
	//队列的大小
    TUINT32 m_udwWorkQueSize;
	//工作线程个数
    TUINT32 m_udwWorkThreadNum;

    // ======================================================== //
    // 回写队列的大小
    TUINT32 m_udwWritebackQueSize;
    // 回写队列什么时候触发回写的大小
    TUINT32 m_udwWritebackQueInitSize;
    // 回写队列的最大长度
    TUINT32 m_udwWritebackQueMaxSize;
    // 回写线程个数
    TUINT32 m_udwWritebackThreadNum;
   
    // 回写的时间间隔
    TUINT32 m_udwWritebackInterval;
    // 每次回写的Item数量
    TUINT32 m_udwWritebackSpeed;
    // 回写数量的增长速度，用于动态调整回写速度
    TUINT32 m_udwWritebackSpeedFactor;
    // 回写的重试次数
    TUINT32 m_udwMaxRetryTime;
    //回写的最大延时
    TUINT32 m_udwWritebackDelay;


    // ======================================================== //
    // 内存索引的参数
    TUINT32 m_udwCacheThreshold;
    TUINT32 m_udwCacheLimit;
    TUINT32 m_udwCacheLRULoopNum;

    TUINT32 m_udwIdleItemMaxSize;
    TUINT32 m_udwIdleItemInitSize;
    TUINT32 m_udwIdleItemSizeFactor;



	// 告警设置
	TCHAR m_project_name[DEFAULT_NAME_STR_LEN];
	TUINT32 m_stat_interval_time;
	TUINT32 m_need_send_message;
	TUINT32 m_error_num_threshold;
	TUINT32 m_error_rate_threshold;
};

#pragma pack()

#endif
