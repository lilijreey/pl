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
��̨�����߳�
**************************************************************************************/
class CWritebackProcess
{
public:
    CWritebackProcess();
    ~CWritebackProcess();
    /*
    *    ��ʼ������
    */
    TINT32 Init(CConf *poConf, CTseLogger *poLog, CWritebackQueue *poWritebackQ);
    /*
    *    �߳���������
    */
    static TVOID * Start(TVOID *pParam);
    /*
    *    ����ѭ��
    */
    TINT32    WorkRoutine();

private:
    // ����ǰ������
    TINT32 ProcessReq(CWritebackSession *poSession);
    TINT32 ProcessRsp(CWritebackSession *poSession);
    static TVOID * Promote(TVOID *pParam);

private:
    ILongConn	*m_poQueryLongConn;     //���ݲ�ѯ������
    CConf	*m_poConf;                  //����

    CTseLogger	*m_poLog;               //Log

    CWritebackQueue	*m_poWritebackQ;         //���¶���

    CDelayUpdateProcess m_objDelayUpt;
    CDelayTaskQueue m_objTaskQueue;
    SDelayTask m_objTask;
    SReportUserCacheProxy m_stProxy;

    TBOOL m_bPromoting;

    TUINT32 m_udwWritebackInterval;           //��д��ʱ����
    TUINT32 m_udwWritebackStepSize;              //ÿ�λ�д��Item����
    TUINT32 m_udwWritebackStepSizeFactor;        //��д�����������ٶȣ����ڶ�̬������д�ٶ�
    

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