#pragma once
#include <vector>
#include "base/log/wtselogger.h"
#include "session.h"
#include "conf.h"
#include "delay_update_process.h"
#include "report_user_cache_proxy.h"
#include "file_mgr.h"

using namespace std;
using namespace wtse::log;

/*************************************************************************************
后台工作线程
**************************************************************************************/
class CWritebackProcess
{
public:
    CWritebackProcess();
    ~CWritebackProcess();
    /*
    *    初始化函数
    */
    TINT32 Init(CConf *poConf, CTseLogger *poLog, CWritebackQueue *poWritebackQ);
    /*
    *    线程启动函数
    */
    static TVOID * Start(TVOID *pParam);
    /*
    *    工作循环
    */
    TINT32    WorkRoutine();

private:
    // 处理前端请求
    TINT32 ProcessReq(CWritebackSession *poSession);
    TINT32 ProcessRsp(CWritebackSession *poSession);
    static TVOID * Promote(TVOID *pParam);

private:
    ILongConn	*m_poQueryLongConn;     //数据查询长连接
    CConf	*m_poConf;                  //配置

    CTseLogger	*m_poLog;               //Log

    CWritebackQueue	*m_poWritebackQ;         //更新队列

    CDelayUpdateProcess m_objDelayUpt;
    CDelayTaskQueue m_objTaskQueue;
    SDelayTask m_objTask;
    SReportUserCacheProxy m_stProxy;

    TBOOL m_bPromoting;

    TUINT32 m_udwWritebackInterval;           //回写的时间间隔
    TUINT32 m_udwWritebackStepSize;              //每次回写的Item数量
    TUINT32 m_udwWritebackStepSizeFactor;        //回写数量的增长速度，用于动态调整回写速度
    

    TUINT32 m_udwDynamodbReqTime;
    TUINT32 m_udwRetryTime;
    TUINT32 m_udwMaxRetryTime;

    TINT32 m_dwRetCode;
    TUINT32 m_udwCacheType;
    TINT64 m_ddwCurKey;

    TUINT32        m_udwWritebackQueSize;
    TUINT32        m_udwWritebackQueInitSize;
    TUINT32        m_udwWritebackQueMaxSize;
    TUINT32        m_udwWritebackDelay;


    SRawDataOffsetInfo m_stRawDataOffsetInfo;
};