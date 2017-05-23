#ifndef _GLOBAL_SERV_H_
#define _GLOBAL_SERV_H_

#include "my_include.h"
#include "conf.h"
#include "work_session_mgr.h"
#include "writeback_session_mgr.h"
#include "query_netio.h"
#include "search_netio.h"
#include "work_process.h"
#include "write_back_process.h"
#include "session.h"
#include "file_mgr.h"
#include "pre_processing_mgr.h"

#include <unistd.h>

class CGlobalServ
{
public:
    static int InitAwsTable(const TCHAR *pszProjectPrefix);

    static TINT32 Init();

    static TINT32 Start();

    static TINT32 StopProcess();

    static TUINT32 GenerateHsReqSeq();

    static TINT32 PreProcessing();

public:
    static CTseLogger *m_poServLog; /// log
    static CTseLogger *m_poRegLog; /// log
    static CTseLogger *m_poReqLog; /// log
    static CTseLogger *m_poStatLog; /// log

    static CConf *m_poConf;

    static CZkRegConf *m_poZkConf; //ZK ≈‰÷√
    static CZkRegClient *m_poZkRegClient;

    static CQueryNetIO *m_poQueryNetIO;
    static CSearchNetIO *m_poSearchNetIO;

    static CWorkQueue *m_poTaskQueue;
    static CWritebackQueue *m_poDelayTaskQueue;

    static CWorkProcess *m_poTaskProcess;
    static CWritebackProcess *m_poDelayUpdateProcess;

private:
    static TUINT32 m_udwReqSeq;
    static pthread_mutex_t m_mtxReqSeq;
};


#endif 


