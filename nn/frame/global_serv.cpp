#include "global_serv.h"
#include "aws_table_include.h"
#include "statistic.h"
#include "curl/curl.h"
#include "write_back_process.h"
#include "work_process.h"
#include "cache_mgr.h"
#include "aws_consume_info.h"


CTseLogger *CGlobalServ::m_poServLog = NULL;
CTseLogger *CGlobalServ::m_poRegLog = NULL;
CTseLogger *CGlobalServ::m_poReqLog = NULL;
CTseLogger *CGlobalServ::m_poStatLog = NULL;

CConf *CGlobalServ::m_poConf = NULL;

CZkRegConf* CGlobalServ::m_poZkConf = NULL;
CZkRegClient *CGlobalServ::m_poZkRegClient = NULL;

CWorkQueue *CGlobalServ::m_poTaskQueue = NULL;
CQueryNetIO *CGlobalServ::m_poQueryNetIO = NULL;
CSearchNetIO *CGlobalServ::m_poSearchNetIO = NULL;
CWritebackQueue *CGlobalServ::m_poDelayTaskQueue = NULL;

CWorkProcess *CGlobalServ::m_poTaskProcess = NULL;
CWritebackProcess *CGlobalServ::m_poDelayUpdateProcess = NULL;

TUINT32 CGlobalServ::m_udwReqSeq = 100000;
pthread_mutex_t CGlobalServ::m_mtxReqSeq = PTHREAD_MUTEX_INITIALIZER;

//===============================================================================

int CGlobalServ::InitAwsTable(const TCHAR *pszProjectPrefix)
{
    TbReport_user_cache::Init("../tblxml/report_user_cache.xml", pszProjectPrefix);
    return 0;
}

TINT32 CGlobalServ::Init()
{
    //log
    INIT_LOG_MODULE("../conf/serv_log.conf");
    assert(config_ret == 0);
    DEFINE_LOG(serv_Log, "serv_log");
    m_poServLog = serv_Log;
    DEFINE_LOG(reg_Log, "reg_log");
    m_poRegLog = reg_Log;
    DEFINE_LOG(req_Log, "req_log");
    m_poReqLog = req_Log;
    DEFINE_LOG(stat_Log, "stat_log");
    m_poStatLog = stat_Log;

    //加载conf
    m_poConf = new CConf;
    if (0 != m_poConf->Init("../conf/serv_info.conf", "../conf/module.conf", "../conf/report_center.conf"))
    {
        TSE_LOG_ERROR(m_poServLog, ("GlobalServ Init conf failed"));
        return -1;
    }
    TSE_LOG_INFO(m_poServLog, ("GlobalServ Init conf succ"));
    
    //初始化表
    InitAwsTable(m_poConf->m_szTblProject);

    //初始化zk相关配置
    m_poZkConf = new CZkRegConf;
    if (0 != m_poZkConf->Init("../conf/serv_info.conf", "../conf/module.conf"))
    {
        TSE_LOG_ERROR(m_poServLog, ("GlobalServ init zk_conf failed!"));
        return -2;
    }
    TSE_LOG_INFO(m_poServLog, ("GlobalServ init zk_conf success!"));

    // ******************************** 工作线程相关的 ******************************** //
    // 初始化工作线程的工作队列管理器
    m_poTaskQueue = new CWorkQueue;
    if (0 != m_poTaskQueue->Init(m_poConf->m_udwWorkQueSize))
    {
        TSE_LOG_ERROR(m_poServLog, ("GlobalServ Init work queue failed with queue_size=%u", m_poConf->m_udwWorkQueSize));
        return -1;
    }
    TSE_LOG_INFO(m_poServLog, ("GlobalServ Init work queue succ"));

    // 初始化工作线程的Session管理器
    if (0 != CWorkSessionMgr::Instance()->Init(m_poConf, m_poServLog))
    {
        TSE_LOG_ERROR(m_poServLog, ("GlobalServ init work_session_mgr failed!"));
        return -1;
    }
    TSE_LOG_INFO(m_poServLog, ("GlobalServ init work_session_mgr success!"));


    // ******************************** 回写线程相关的 ******************************** //
    m_poDelayTaskQueue = new CWritebackQueue;
    if (0 != m_poDelayTaskQueue->Init(m_poConf->m_udwWritebackQueSize))
    {
        TSE_LOG_ERROR(m_poServLog, ("GlobalServ Init write back queue failed with queue_size=%u", m_poConf->m_udwWritebackQueSize));
        return -1;
    }
    TSE_LOG_INFO(m_poServLog, ("GlobalServ Init write back queue succ"));

    // 初始化工作线程的Session管理器
    if (0 != CWritebackSessionMgr::Instance()->Init(m_poConf, m_poServLog))
    {
        TSE_LOG_ERROR(m_poServLog, ("GlobalServ init write_back_session_mgr failed!"));
        return -1;
    }
    TSE_LOG_INFO(m_poServLog, ("GlobalServ init write_back_session_mgr success!"));

    // ******************************************************************************** //


    // 初始化文件管理
    if (0 != CFileMgr::GetInstance()->Init(m_poServLog))
    {
        TSE_LOG_ERROR(m_poServLog, ("init file mgr failed!"));
        return -3;
    }

    // 初始化预充管理
    if (0 != CPreProcessingMgr::GetInstance()->Init(m_poServLog, m_poTaskQueue, m_poDelayTaskQueue))
    {
        TSE_LOG_ERROR(m_poServLog, ("init pre-processsing mgr failed!"));
        return -3;
    }


    // ********************************** 内存索引 ************************************* //
    if (0 != CCacheMgr::Instance()->Init(m_poConf, m_poServLog, m_poDelayTaskQueue))
    {
        TSE_LOG_ERROR(m_poServLog, ("GlobalServ init cache_index_mgr failed!"));
        return -1;
    }
    TSE_LOG_INFO(m_poServLog, ("GlobalServ init cache_index_mgr success!"));
    // ******************************************************************************** //

    // 接收请求网络IO
    m_poQueryNetIO = new CQueryNetIO;
    if (0 != m_poQueryNetIO->Init(m_poConf, m_poServLog, m_poTaskQueue, m_poDelayTaskQueue))
    {
        TSE_LOG_ERROR(m_poServLog, ("GlobalServ Init query netio failed with [ip=%s] [port=%u]",
            m_poConf->m_szServerIp, m_poConf->m_uwServerPort));
        return -1;
    }
    TSE_LOG_INFO(m_poServLog, ("GlobalServ Init query net_io succ"));

  
    // 初始化下游网络IO
    m_poSearchNetIO = new CSearchNetIO;
    if (0 != m_poSearchNetIO->Init(m_poConf, m_poServLog, m_poTaskQueue))
    {
        TSE_LOG_ERROR(m_poServLog, ("GlobalServ init net_io failed!"));
        return -6;
    }
    TSE_LOG_INFO(m_poServLog, ("GlobalServ init net_io success!"));


    // 延迟更新工作线程
    m_poDelayUpdateProcess = new CWritebackProcess[m_poConf->m_udwWritebackThreadNum];
    for (TUINT32 udwIdx = 0; udwIdx < m_poConf->m_udwWritebackThreadNum; udwIdx++)
    {
        if (0 != m_poDelayUpdateProcess[udwIdx].Init(m_poConf, m_poServLog, m_poDelayTaskQueue))
        {
            TSE_LOG_ERROR(m_poServLog, ("GlobalServ init %uth write back process failed!", udwIdx));
            return -1;
        }
    }
    TSE_LOG_INFO(m_poServLog, ("GlobalServ init write back process success!"));

    // 工作线程
    m_poTaskProcess = new CWorkProcess[m_poConf->m_udwWorkThreadNum];
    for (TUINT32 udwIdx = 0; udwIdx < m_poConf->m_udwWorkThreadNum; udwIdx++)
    {
        if (0 != m_poTaskProcess[udwIdx].Init(m_poConf, m_poServLog, m_poReqLog, m_poTaskQueue, m_poDelayTaskQueue, \
                                              m_poQueryNetIO->m_pLongConn, m_poSearchNetIO->m_pLongConn))
        {
            TSE_LOG_ERROR(m_poServLog, ("GlobalServ init %uth work process failed!", udwIdx));
            return -1;
        }
    }
    TSE_LOG_INFO(m_poServLog, ("GlobalServ init work process success!"));

    //初始化zk下游管理器
    if (0 != CDownMgr::Instance()->zk_Init(m_poRegLog, m_poSearchNetIO->m_pLongConn, m_poZkConf))
    {
        TSE_LOG_ERROR(m_poServLog, ("GlobalServ init session_mgr failed!"));
        return -7;
    }
    TSE_LOG_INFO(m_poServLog, ("GlobalServ init zk_session_mgr success!"));

    m_poZkRegClient = new CZkRegClient;
    if (0 != m_poZkRegClient->Init(m_poRegLog, m_poZkConf))
    {
        TSE_LOG_ERROR(m_poServLog, ("GlobalServ Init reg_client_zk failed"));
        return -4;
    }
    TSE_LOG_INFO(m_poServLog, ("GlobalServ Init reg_client_zk succ"));

    // statistic
    CStatistic::Instance()->Init(m_poStatLog, m_poConf->m_stat_interval_time,
        m_poConf->m_project_name, m_poConf->m_need_send_message,
        m_poConf->m_error_num_threshold, m_poConf->m_error_rate_threshold,
        m_poConf->m_szServerIp, m_poConf->m_uwServerPort);
    TSE_LOG_INFO(m_poServLog, ("GlobalServ Init statistic succ"));

    curl_global_init(CURL_GLOBAL_ALL);

    TSE_LOG_INFO(m_poServLog, ("report center Init succ!!"));

    // 做随机种子
    srand((unsigned)time(0));

    TSE_LOG_INFO(m_poServLog, ("GlobalServ Init succ!!"));

    return 0;
}

TINT32 CGlobalServ::Start()
{
    pthread_t thread_id = 0;

    // 1. 创建工作线程
    for (TUINT32 udwIdx = 0; udwIdx < m_poConf->m_udwWorkThreadNum; udwIdx++)
    {
        if (pthread_create(&thread_id, NULL, CWorkProcess::Start, &m_poTaskProcess[udwIdx]) != 0)
        {
            TSE_LOG_ERROR(m_poServLog, ("create task process thread failed! [idx=%u] Error[%s]",
                udwIdx, strerror(errno)));
            return -1;
        }
    }
    TSE_LOG_INFO(m_poServLog, ("create task_process thread success!"));

    // 1.1 创建延迟更新工作线程
    for (TUINT32 udwIdx = 0; udwIdx < m_poConf->m_udwWritebackThreadNum; ++udwIdx)
    {
        if (pthread_create(&thread_id, NULL, CWritebackProcess::Start, &m_poDelayUpdateProcess[udwIdx]) != 0)
        {
            TSE_LOG_ERROR(m_poServLog, ("create delay task process thread failed! [idx=%u] Error[%s]",
                udwIdx, strerror(errno)));
            return -1;
        }
    }
    TSE_LOG_INFO(m_poServLog, ("create delay_update_process thread success!"));

    //2.1创建zk下游管理线程
    if (pthread_create(&thread_id, NULL, CDownMgr::zk_StartPull, CDownMgr::Instance()) != 0)
    {
        TSE_LOG_ERROR(m_poServLog, ("create down manager thread failed! Error[%s]", strerror(errno)));
        return -2;
    }
    if (pthread_create(&thread_id, NULL, CDownMgr::zk_StartCheck, CDownMgr::Instance()) != 0)
    {
        TSE_LOG_ERROR(m_poServLog, ("create down manager thread failed! Error[%s]", strerror(errno)));
        return -2;
    }

    // 4. 创建网络线程-search
    if (pthread_create(&thread_id, NULL, CSearchNetIO::RoutineNetIO, m_poSearchNetIO) != 0)
    {
        TSE_LOG_ERROR(m_poServLog, ("create search net io thread failed! Error[%s]",
            strerror(errno)));
        return -5;
    }
    TSE_LOG_INFO(m_poServLog, ("create search net io thread success!"));

    // ================================================================================== //
    // 预充流程
    if (0 != CPreProcessingMgr::GetInstance()->PreProcessing())
    {
        TSE_LOG_ERROR(m_poServLog, ("pre processing failed!"));
        return -4;
    }

    // ================================================================================== //
    // 4. 创建网络线程-query
    if (pthread_create(&thread_id, NULL, CQueryNetIO::RoutineNetIO, m_poQueryNetIO) != 0)
    {
        TSE_LOG_ERROR(m_poServLog, ("create query net io thread failed! Error[%s]",
            strerror(errno)));
        return -5;
    }
    TSE_LOG_INFO(m_poServLog, ("create query net io thread success!"));

    // 5. 开启统计线程
    if (pthread_create(&thread_id, NULL, CStatistic::Start, CStatistic::Instance()) != 0)
    {
        TSE_LOG_ERROR(m_poServLog, ("create statistic thread failed! Error[%s]",
            strerror(errno)));
        return -6;
    }
    TSE_LOG_INFO(m_poServLog, ("create statistic thread success!"));

    // 6. 启动注册线程
    m_poZkRegClient->Start();
    TSE_LOG_INFO(m_poServLog, ("create reg client zk thread success!"));

    TSE_LOG_INFO(m_poServLog, ("GlobalServ start all thread success!"));

    return 0;
}

TINT32 CGlobalServ::StopProcess()
{
    m_poZkRegClient->Stop();
    sleep(3);
    m_poQueryNetIO->StopNetServ();

    /* todo::stop掉回写更新
    while (!m_poTaskQueue->BeEmpty())
    {
        sleep(1);
    }

    while (!m_poDelayTaskQueue->BeEmpty())
    {
        sleep(1);
    }
    */


    curl_global_cleanup();

    return 0;
}

TUINT32 CGlobalServ::GenerateHsReqSeq()
{
    TUINT32 udwSeq = 0;
    pthread_mutex_lock(&m_mtxReqSeq);
    m_udwReqSeq++;
    if (m_udwReqSeq < 100000)
    {
        m_udwReqSeq = 100000;
    }
    udwSeq = m_udwReqSeq;
    pthread_mutex_unlock(&m_mtxReqSeq);
    return udwSeq;
}
