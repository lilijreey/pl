#ifndef _FILE_MGR_H_
#define _FILE_MGR_H_


#include "my_include.h"
#include "threadlock.h"
#include "session.h"

using namespace std;

#define DATA_FILE__RAW_DATA_PRE  ("../data/raw_data")
#define DATA_FILE__RAW_DATA_OFFSET  ("../data/raw_data_offset")
#define DATA_FILE__RAW_DATA_CUR_INFO  ("../data/raw_data_cur_info")
#define DATA_FILE__ACTIVE_PLAYER  ("../data/active_player")
#define DATA_FILE__ACTIVE_ALLIANCE  ("../data/active_alliance")

// �ļ����Ϊ10
#define DATA_FILE__RAW_DATA_INDEX (50)
// �����ļ����Ϊ1024Byte
#define DATA_FILE__RAW_DATA_MAX_SIZE (100) // 1024*1024*2038 //Լ����1.99 << 30, ��Ϊftell��ֻ�ܶ� 2^31 -1 ��С���ļ����в���
// д���ٴβŶ�
#define DATA_FILE__RAW_DATA_SHIFT_COUNT (5) // ��ϵ����ļ��Ĵ�С������
// �����ļ�������к�
#define DATA_FILE__RAW_DATA_MAX_ROW (10000) // ����ļ��Ĵ�С������




// raw_data�ļ���Э��ṹ
struct SRawDataInfo
{
    string m_strRawData;
    TUINT32 m_udwFileIndex;
    TUINT64 m_uddwFileRow;

    SRawDataInfo()
    {
        Reset();
    }

    TVOID Reset()
    {
        m_strRawData = "";
        m_udwFileIndex = 0;
        m_uddwFileRow = 0;
    }
};


// raw_data_cur_info�ļ��Ľṹ����
struct SRawDataCurInfo
{
    TINT32 m_dwRawDataEmpty[DATA_FILE__RAW_DATA_INDEX];
    TINT32 m_dwRawDataShiftCount;
    TUINT32 m_udwRawDataCurFileIndex;
    TUINT64 m_uddwRawDataCurFileRow;

    SRawDataCurInfo()
    {
        Reset();
    }

    TVOID Reset()
    {
        for (TINT32 dwIdx = 0; dwIdx < DATA_FILE__RAW_DATA_INDEX; ++dwIdx)
        {
            m_dwRawDataEmpty[dwIdx] = TRUE;
        }
        m_dwRawDataShiftCount = 0;
        m_udwRawDataCurFileIndex = 0;
        m_uddwRawDataCurFileRow = 0;
    }
};


// raw_data_offset�ļ���index���кŵĽṹ����
struct SRawDataOffsetInfo
{
    TUINT32 m_udwFileIndex;
    TUINT64 m_uddwFileRow;

    SRawDataOffsetInfo()
    {
        Reset();
    }

    TVOID Reset()
    {
        m_udwFileIndex = 0;
        m_uddwFileRow = 0;
    }

    bool operator <(const SRawDataOffsetInfo& a) const
    {
        if (m_udwFileIndex < a.m_udwFileIndex)
        {
            return true;
        }
        else if (m_udwFileIndex > a.m_udwFileIndex)
        {
            return false;
        }
        else
        {
            return m_uddwFileRow < a.m_uddwFileRow;
        }

        return false;
    }
};



class CFileMgr
{
    // ˽����Ƕ�࣬�������ʱ���Զ��ͷ�
private:
    class CGarbo
    {
    public:
        ~CGarbo()
        {
            if (CFileMgr::m_poFileMgr)
            {
                delete CFileMgr::m_poFileMgr;
                CFileMgr::m_poFileMgr = NULL;
            }
        }
    };
    static CGarbo Garbo;                                //����һ����̬��Ա�������������ʱ��ϵͳ���Զ����������������� 


// �����������ض���
private:
    CFileMgr()
    {}
    CFileMgr(const CFileMgr &);
    CFileMgr & operator =(const CFileMgr &);
    static CFileMgr *m_poFileMgr;

public:
    ~CFileMgr();

    // function  ===> ʵ���� 
    static CFileMgr *GetInstance();



    // ��ȡ����Ĺ����ӿں���
public:

    // function ==> ��ʼ��file������
    TINT32 Init(CTseLogger *poLog);

    /*
    // function ===> ����raw_data���α��ļ�
    TINT32 UpdateRawDataOffset(const SRawDataOffsetInfo &stRawDataOffsetInfo);
    // function ===> ����raw_data�ļ�
    TINT32 UpdateRawData(const string &strUpdateInfo, SRawDataOffsetInfo &stCurRawDataOffsetInfo);
    */


    // function ==> write raw_data file
    TINT32 WriteRawDataFile(const string &strRawDataString, SRawDataOffsetInfo &stCurRawDataOffsetInfo);
    // function ==> read raw_data file(<0: ��ȡʧ��; 0: û�пɶ�; >0: ��ʣ������û�ж�)(һ��Ҫ)
    TINT32 ReadRawDataFile(const SRawDataOffsetInfo &stRawDataOffsetInfo, vector<SRawDataInfo> &vecRawDataInfo);


    // function ==> write raw_data_offset file
    TINT32 WriteRawDataOffsetFile(const SRawDataOffsetInfo &stRawDataOffsetInfo);
    // function ==> read raw_data_offset file
    TINT32 ReadRawDataOffsetFile(SRawDataOffsetInfo &stRawDataOffsetInfo);


    // �ڲ���Ա����
private:



    // function ===> ��ʼ��raw_data��raw_data_offset��raw_data_cur_info
    TINT32 RawDataInit(const string &strRawDataFile, const string &strRawDataOffsetFile, const string &strRawDataCurInfo);


    // function ==> ��ԭʼ��raw_data stringת��Ϊraw_data�ṹ
    TINT32 GetRawdataInfoFromString(const string &strRawdata, SRawDataInfo &stRawDataInfo);
    // function ==> ��ԭʼ��raw_data_offset stringת��Ϊraw_data_offset�ṹ
    TINT32 GetRawdataOffsetFromString(const string &strRawdataOffset, SRawDataOffsetInfo &stRawDataOffsetInfo);
    // function ==> ��ԭʼ��raw_data_cur_info stringת��Ϊraw_data_cur_info�ṹ
    TINT32 GetRawdataCurInfoFromString(const string &strRawdataCurInfo, SRawDataCurInfo &stRawDataCurInfo);



    // function ==> write raw_data_cur_info file
    TINT32 WriteRawDataCurInfoFile(SRawDataCurInfo &stRawDataCurInfo);
    // function ==> read raw_data_cur_info file
    TINT32 ReadRawDataCurInfoFile();


    // function ==> ����Ƿ���Ҫ�л��ļ�,�����ص�ǰ���ļ����
    TINT32 shiftFiles();

    // function ==> ��վɼ�¼�洢���ļ�����
    TINT32 ClearOldRawDataFile(SRawDataOffsetInfo &stCurRawDataOffsetInfo);


    // �ڲ���Ա����
private:
    // ��־
    CTseLogger *m_poServLog;

    // �ļ����߳���
    CNewThreadLock m_oRawData;
    CNewThreadLock m_oRawDataOffset;
    CNewThreadLock m_oRawDataCurInfo;

    

    // raw_data��raw_data_offset�ļ�����Ӧ��Ϣ,ʹ��ǰ������м�������
    FILE *m_fpRawData[DATA_FILE__RAW_DATA_INDEX];
    TINT32 m_dwRawDataEmpty[DATA_FILE__RAW_DATA_INDEX];
    TINT32 m_dwRawDataShitCount;
    TUINT32 m_udwRawDataCurFileIndex;
    TUINT64 m_uddwRawDataCurFileRow;


    FILE *m_fpRawDataOffset;
    FILE *m_fpRawDataCurInfo;
    SRawDataOffsetInfo m_stExpectFileOffset;
    set<SRawDataOffsetInfo> m_setFileOffset;
    
};

#endif
