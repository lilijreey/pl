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
    // ˽����Ƕ�࣬�������ʱ���Զ��ͷ�
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
    static CGarbo Garbo;                                //����һ����̬��Ա�������������ʱ��ϵͳ���Զ����������������� 


    // �����������ض���
private:
    CPreProcessingMgr()
    {}
    CPreProcessingMgr(const CPreProcessingMgr &);
    CPreProcessingMgr & operator =(const CPreProcessingMgr &);
    static CPreProcessingMgr *m_poPreProcessingMgr;

public:
    ~CPreProcessingMgr();

    // function  ===> ʵ���� 
    static CPreProcessingMgr *GetInstance();



    // ��ȡ����Ĺ����ӿں���
public:
    // function ==> ��ʼ��file������
    TINT32 Init(CTseLogger *poLog, CWorkQueue *poTaskQueue, CWritebackQueue *poDelayTaskQueue);


    // function ==> pre create active player and alliance cache index from active player and alliance list file
    TINT32 PreCreateCacheIndexFromList(vector<SRawDataInfo> &vecRawDataInfo);
    // function ==> pre create active player and alliance cache index from raw_data and raw_data_offset file
    TINT32 PreCreateCacheIndexFromOffset(vector<SRawDataInfo> &vecRawDataInfo);

    // ԭ����ת��������ڵ�
    TINT32 ConvertRawdataInfoToWorkTask(CWorkSession *poWorkSession, SRawDataInfo &stRawDataInfo);
    // ԭ����ת��д����ڵ�
    TINT32 ConvertRawdataInfoToWriteBack(CWritebackSession *poWritebackSession, SRawDataInfo &stRawDataInfo);

    // Ԥ�䴦��
    TINT32 PreProcessing();

    // �ڲ���Ա����
private:

    // function ===> ��ʼ��active_player��active_alliance, Ԥ���߼�
    TINT32 ActiveDataInit(const string &strActicvePlayerFile, const string &strActicveAllianceFile);
    // function ==> ͨ��uid��aid����query����,����Ԥ��ʱ�ؽ��ڴ�����
    TINT32 GetQueryRequest(TINT64 ddwKey, SRawDataInfo &stRawDataInfo);



    // �ڲ���Ա����
private:
    // ��־
    CTseLogger *m_poServLog;

    // �ļ����߳���
    CNewThreadLock m_oActivePlayer;
    CNewThreadLock m_oActiveAlliance;
    
    // ��Ծ��Һ������б�
    set<TINT64> m_setActivePlayer;
    set<TINT64> m_setActiveAlliance;
    
    CWorkQueue *m_poTaskQueue;
    CWritebackQueue *m_poDelayTaskQueue;
    
};

#endif