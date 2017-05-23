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

// 文件序号为10
#define DATA_FILE__RAW_DATA_INDEX (50)
// 单个文件最大为1024Byte
#define DATA_FILE__RAW_DATA_MAX_SIZE (100) // 1024*1024*2038 //约等于1.99 << 30, 因为ftell的只能对 2^31 -1 大小的文件进行操作
// 写多少次才对
#define DATA_FILE__RAW_DATA_SHIFT_COUNT (5) // 配合单个文件的大小做调整
// 单个文件的最大行号
#define DATA_FILE__RAW_DATA_MAX_ROW (10000) // 结合文件的大小来决定




// raw_data文件的协议结构
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


// raw_data_cur_info文件的结构定义
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


// raw_data_offset文件的index和行号的结构定义
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
    // 私有内嵌类，程序结束时的自动释放
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
    static CGarbo Garbo;                                //定义一个静态成员变量，程序结束时，系统会自动调用它的析构函数 


// 单例服务的相关定义
private:
    CFileMgr()
    {}
    CFileMgr(const CFileMgr &);
    CFileMgr & operator =(const CFileMgr &);
    static CFileMgr *m_poFileMgr;

public:
    ~CFileMgr();

    // function  ===> 实例化 
    static CFileMgr *GetInstance();



    // 获取服务的公共接口函数
public:

    // function ==> 初始化file管理器
    TINT32 Init(CTseLogger *poLog);

    /*
    // function ===> 更新raw_data的游标文件
    TINT32 UpdateRawDataOffset(const SRawDataOffsetInfo &stRawDataOffsetInfo);
    // function ===> 更新raw_data文件
    TINT32 UpdateRawData(const string &strUpdateInfo, SRawDataOffsetInfo &stCurRawDataOffsetInfo);
    */


    // function ==> write raw_data file
    TINT32 WriteRawDataFile(const string &strRawDataString, SRawDataOffsetInfo &stCurRawDataOffsetInfo);
    // function ==> read raw_data file(<0: 读取失败; 0: 没有可读; >0: 还剩多少行没有读)(一定要)
    TINT32 ReadRawDataFile(const SRawDataOffsetInfo &stRawDataOffsetInfo, vector<SRawDataInfo> &vecRawDataInfo);


    // function ==> write raw_data_offset file
    TINT32 WriteRawDataOffsetFile(const SRawDataOffsetInfo &stRawDataOffsetInfo);
    // function ==> read raw_data_offset file
    TINT32 ReadRawDataOffsetFile(SRawDataOffsetInfo &stRawDataOffsetInfo);


    // 内部成员函数
private:



    // function ===> 初始化raw_data和raw_data_offset和raw_data_cur_info
    TINT32 RawDataInit(const string &strRawDataFile, const string &strRawDataOffsetFile, const string &strRawDataCurInfo);


    // function ==> 从原始的raw_data string转换为raw_data结构
    TINT32 GetRawdataInfoFromString(const string &strRawdata, SRawDataInfo &stRawDataInfo);
    // function ==> 从原始的raw_data_offset string转换为raw_data_offset结构
    TINT32 GetRawdataOffsetFromString(const string &strRawdataOffset, SRawDataOffsetInfo &stRawDataOffsetInfo);
    // function ==> 从原始的raw_data_cur_info string转换为raw_data_cur_info结构
    TINT32 GetRawdataCurInfoFromString(const string &strRawdataCurInfo, SRawDataCurInfo &stRawDataCurInfo);



    // function ==> write raw_data_cur_info file
    TINT32 WriteRawDataCurInfoFile(SRawDataCurInfo &stRawDataCurInfo);
    // function ==> read raw_data_cur_info file
    TINT32 ReadRawDataCurInfoFile();


    // function ==> 检测是否需要切换文件,并返回当前的文件序号
    TINT32 shiftFiles();

    // function ==> 清空旧记录存储的文件内容
    TINT32 ClearOldRawDataFile(SRawDataOffsetInfo &stCurRawDataOffsetInfo);


    // 内部成员变量
private:
    // 日志
    CTseLogger *m_poServLog;

    // 文件的线程锁
    CNewThreadLock m_oRawData;
    CNewThreadLock m_oRawDataOffset;
    CNewThreadLock m_oRawDataCurInfo;

    

    // raw_data和raw_data_offset文件的相应信息,使用前必须进行加锁操作
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
