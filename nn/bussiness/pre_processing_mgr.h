#ifndef _PRE_PROCESSING_MGR_H_
#define _PRE_PROCESSING_MGR_H_


#include "my_include.h"
#include "threadlock.h"
#include "session.h"
#include "file_mgr.h"
#include "work_session_mgr.h"
#include "writeback_session_mgr.h"
#include "time_utils.h"

using namespace std;

#define DATA_FILE__ACTIVE_PLAYER  ("../data/active_player")
#define DATA_FILE__ACTIVE_ALLIANCE  ("../data/active_alliance")



class CPreProcessingMgr
{
    // 私有内嵌类，程序结束时的自动释放
private:
    class CGarbo
    {
    public:
        ~CGarbo()
        {
            if (CPreProcessingMgr::m_poPreProcessingMgr)
            {
                delete CPreProcessingMgr::m_poPreProcessingMgr;
                CPreProcessingMgr::m_poPreProcessingMgr = NULL;
            }
        }
    };
    static CGarbo Garbo;                                //定义一个静态成员变量，程序结束时，系统会自动调用它的析构函数 


    // 单例服务的相关定义
private:
    CPreProcessingMgr()
    {}
    CPreProcessingMgr(const CPreProcessingMgr &);
    CPreProcessingMgr & operator =(const CPreProcessingMgr &);
    static CPreProcessingMgr *m_poPreProcessingMgr;

public:
    ~CPreProcessingMgr();

    // function  ===> 实例化 
    static CPreProcessingMgr *GetInstance();



    // 获取服务的公共接口函数
public:
    // function ==> 初始化file管理器
    TINT32 Init(CTseLogger *poLog, CWorkQueue *poTaskQueue, CWritebackQueue *poDelayTaskQueue);


    // function ==> pre create active player and alliance cache index from active player and alliance list file
    TINT32 PreCreateCacheIndexFromList(vector<SRawDataInfo> &vecRawDataInfo);
    // function ==> pre create active player and alliance cache index from raw_data and raw_data_offset file
    TINT32 PreCreateCacheIndexFromOffset(vector<SRawDataInfo> &vecRawDataInfo);

    // 原数据转工作任务节点
    TINT32 ConvertRawdataInfoToWorkTask(CWorkSession *poWorkSession, SRawDataInfo &stRawDataInfo);
    // 原数据转回写任务节点
    TINT32 ConvertRawdataInfoToWriteBack(CWritebackSession *poWritebackSession, SRawDataInfo &stRawDataInfo);

    // 预充处理
    TINT32 PreProcessing();

    // 内部成员函数
private:

    // function ===> 初始化active_player和active_alliance, 预充逻辑
    TINT32 ActiveDataInit(const string &strActicvePlayerFile, const string &strActicveAllianceFile);
    // function ==> 通过uid和aid生成query请求,用于预充时重建内存索引
    TINT32 GetQueryRequest(TINT64 ddwKey, SRawDataInfo &stRawDataInfo);



    // 内部成员变量
private:
    // 日志
    CTseLogger *m_poServLog;

    // 文件的线程锁
    CNewThreadLock m_oActivePlayer;
    CNewThreadLock m_oActiveAlliance;
    
    // 活跃玩家和联盟列表
    set<TINT64> m_setActivePlayer;
    set<TINT64> m_setActiveAlliance;
    
    CWorkQueue *m_poTaskQueue;
    CWritebackQueue *m_poDelayTaskQueue;
    
};

#endif