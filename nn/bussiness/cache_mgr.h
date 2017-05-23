#pragma once
#include <map>
#include <vector>
#include "base/log/wtselogger.h"
#include "conf.h"
#include "writeback_session_mgr.h"
#include "aws_table_report_user_cache.h"
#include "my_define.h"


using namespace std;
using namespace wtse::log;

//#define CHAR_BIT  (8)
#define BITMASK(b) (1 << ((b) % CHAR_BIT))
#define BITSLOT(b) ((b) / CHAR_BIT)
#define BITSET(a, b) ((a)[BITSLOT(b)] |= BITMASK(b))
#define BITCLEAR(a, b) ((a)[BITSLOT(b)] &= ~BITMASK(b))
#define BITTEST(a, b) ((a)[BITSLOT(b)] & BITMASK(b))
#define BITNSLOTS(nb) ((nb + CHAR_BIT - 1) / CHAR_BIT)

#define MAX_ITEM_NUM_PER_SEQ 50
#define MAX_SEQ_NUM 20

#define MAX_NUM_CACHE_TYPE 5

enum ECacheType
{
    CACHE_TYPE_PLAYER = 0,
    CACHE_TYPE_ALLIANCE,
    CACHE_TYPE_SERVER,
    CACHE_TYPE_UNLIVE,
    CACHE_TYPE_OVERFLOW
};


typedef SReportInfo SItem;

struct SPartition
{
    TINT64 m_ddwUid; //8B
    TINT64 m_ddwBaseSeq; //8B
    TUINT32 m_udwSize; //4B
    TUINT32 m_udwFront; //4B
    TUINT32 m_udwRear; //4B
    TUINT32 m_udwRefCnt; //4B
    SPartition* m_pNext; //8B
    SPartition* m_pPrev; //8B
    char m_bmapSeqUpdatedFlag[BITNSLOTS(MAX_SEQ_NUM)]; // BITNSLOTS(MAX_SEQ_NUM)
    SItem m_queItem[MAX_SEQ_NUM * MAX_ITEM_NUM_PER_SEQ]; //MAX_SEQ_NUM * MAX_ITEM_NUM_PER_SEQ * sizeof(SItem)

    SPartition() {
        Reset();
    }

    TVOID Reset() {
        memset(this, 0, sizeof(SPartition));
    }

    TUINT32 DoForwardCursor(TUINT32 udwCursor, TUINT32 udwOffset) {
        assert(udwCursor < MAX_SEQ_NUM * MAX_ITEM_NUM_PER_SEQ);
        assert(udwOffset <= MAX_SEQ_NUM * MAX_ITEM_NUM_PER_SEQ);

        udwCursor += udwOffset;

        if (udwCursor >= MAX_SEQ_NUM * MAX_ITEM_NUM_PER_SEQ)
        {
            udwCursor -= MAX_SEQ_NUM * MAX_ITEM_NUM_PER_SEQ;
        }

        return udwCursor;
    }

    TINT64 Index2Seq(TUINT32 udwIdx) {
        assert(m_udwSize > 0);
        if (m_udwFront > m_udwRear)
        {
            assert(udwIdx >= m_udwRear && udwIdx <= m_udwFront);

            return m_ddwBaseSeq + (udwIdx - m_udwRear) / MAX_ITEM_NUM_PER_SEQ;
        }
        else
        {
            if (udwIdx > m_udwRear)
            {
                assert(udwIdx < MAX_SEQ_NUM*MAX_ITEM_NUM_PER_SEQ);
                return m_ddwBaseSeq + (udwIdx - m_udwRear) / MAX_ITEM_NUM_PER_SEQ;
            }
            else if (udwIdx < m_udwRear)
            {
                assert(udwIdx <= m_udwFront);
                return m_ddwBaseSeq + (MAX_SEQ_NUM*MAX_ITEM_NUM_PER_SEQ + udwIdx - m_udwRear) / MAX_ITEM_NUM_PER_SEQ;
            }
            else
            {
                return m_ddwBaseSeq;
            }
        }

    }

    TINT64 GetMaxSeq() {
        if (m_udwSize == 0)
        {
            return m_ddwBaseSeq;
        }
        return (m_udwSize - 1) / MAX_ITEM_NUM_PER_SEQ + m_ddwBaseSeq;
    }

    TUINT32 Seq2Index(TINT64 ddwSeq) {
        assert(ddwSeq >= m_ddwBaseSeq);
        assert(ddwSeq <= GetMaxSeq());
        TUINT32 udwOffset = (ddwSeq - m_ddwBaseSeq)*MAX_ITEM_NUM_PER_SEQ;

        return DoForwardCursor(m_udwRear, udwOffset);
    }

    TUINT32 Index2UpdatedFlagIndex(TUINT32 udwIdx) {
        assert(m_udwSize > 0);
        return udwIdx / MAX_ITEM_NUM_PER_SEQ;
    }

    TBOOL DoCheckUpdated(TINT64 ddwSeq) {
        assert(ddwSeq >= m_ddwBaseSeq);
        assert(ddwSeq <= GetMaxSeq());
        TUINT32 udwFlagIdx = Index2UpdatedFlagIndex(Seq2Index(ddwSeq));
        if (BITTEST(m_bmapSeqUpdatedFlag,udwFlagIdx))
        {
            return TRUE;
        }
        else
        {
            return FALSE;
        }
    }

    TBOOL DoCheckUpdated() {
        TBOOL bUpdated = FALSE;
        if (m_udwSize == 0)
        {
            return bUpdated;
        }
        for (TINT64 ddwSeq = m_ddwBaseSeq; ddwSeq <= GetMaxSeq(); ddwSeq++)
        {
            bUpdated |= DoCheckUpdated(ddwSeq);
        }

        return bUpdated;
    }

    TINT32 DoAppend(const SItem* poNewItem, TBOOL bOverwrite) {
        TINT32 dwRetCode = 0;
        TBOOL bExist = FALSE;
        TUINT32 udwCount = 0, udwIdx = m_udwRear;

        //����Rid�ж�д����¶����Ƿ��Ѵ���
        while (udwCount < m_udwSize)
        {
            if (m_queItem[udwIdx].m_nRid == poNewItem->m_nRid)
            {
                bExist = TRUE;
                break;
            }

            udwIdx = DoForwardCursor(udwIdx, 1);
            udwCount++;
        }

        if (bExist)
        {
            return dwRetCode;
        }

        //��������Ƿ���д��
        if (m_udwSize >= MAX_SEQ_NUM * MAX_ITEM_NUM_PER_SEQ)
        {
            //��һ�������Ƿ���Dynamodb�е�����һ�£�һ�����ʾ��ֱ�����߼�ɾ��
            if (DoCheckUpdated(m_ddwBaseSeq))
            {
                //������Ϊ�ɸ�д����ôҲ����ֱ�����߼�ɾ��
                if (!bOverwrite)
                {
                    dwRetCode = -1;
                    return dwRetCode;
                }
            }

            //�߼�ɾ����һ�е����ݣ�UpdateFlag�������������
            m_udwRear = DoForwardCursor(m_udwRear, MAX_ITEM_NUM_PER_SEQ);
            m_ddwBaseSeq++;
            m_udwSize -= MAX_ITEM_NUM_PER_SEQ;
        }

        m_queItem[m_udwFront] = *poNewItem;//����Ҫȡֵ������ȡָ�룬����ǳ����
        m_udwSize++;
        TUINT32 udwCurUpdatedFlagIdx = Index2UpdatedFlagIndex(m_udwFront);
        BITSET(m_bmapSeqUpdatedFlag, udwCurUpdatedFlagIdx);
        m_udwFront = DoForwardCursor(m_udwFront, 1);

        return dwRetCode;
    }
};

struct SItemExt
{
    SItem m_objItem;
    TINT64 m_ddwSeq;
    TUINT32 m_udwPos;

    SItemExt() {
        Reset();
    }

    TVOID Reset() {
        memset(this, 0, sizeof(SItemExt));
    }
};

struct SItemSeq
{
    SItem m_szItems[MAX_ITEM_NUM_PER_SEQ];
    TINT64 m_ddwSeq;
    TUINT32 m_udwSize;

    SItemSeq() {
        Reset();
    }

    TVOID Reset() {
        memset(this, 0, sizeof(SItemSeq));
    }
};

struct SLockCount {
    TBOOL m_bOwn;
    TUINT32 m_udwWaitCnt;

    SLockCount() {
        Reset();
    }

    TVOID Reset() {
        m_bOwn = FALSE;
        m_udwWaitCnt = 0;
    }
};

class CCacheMgr
{
private:
    SPartition* m_pHeadPlayer; //һ������map�е�����Ҳ�������У�ʵ��LRU
    SPartition* m_pTailPlayer;
    map<TINT64/*UID*/, SPartition*> m_mapPlayerCache;

    SPartition* m_pHeadAlliance;
    SPartition* m_pTailAlliance;
    map<TINT64, SPartition*> m_mapAllianceCache;

    SPartition* m_pHeadServer;
    SPartition* m_pTailServer;
    map<TINT64, SPartition*> m_mapServerCache;

    map<TINT64, SPartition*> m_mapUnliveCache;
    map<TINT64, vector<SItemSeq*> > m_mapOverflow;

    pthread_mutex_t m_lockCache[MAX_NUM_CACHE_TYPE];

    map<TINT64, SLockCount*> m_mapUidLock;
    pthread_mutex_t m_lockUid;

    TUINT32        m_udwCacheThreshold;
    TUINT32        m_udwCacheLimit;
    TUINT32        m_udwCacheLRULoopNum;
    TUINT32        m_udwCachePartitionCnt;

    CWritebackQueue *m_poWritebackQ;
    CTseLogger *m_poLog; // ��־����

    static CCacheMgr *m_poCacheMgr;

    CCacheMgr();
    ~CCacheMgr();
    int LockCache(const TUINT32 udwCacheType);
    int UnlockCache(const TUINT32 udwCacheType);
    int LockUid(const TINT64 ddwUid);
    int TryLockUid(const TINT64 ddwUid);
    int UnlockUid(const TINT64 ddwUid);
    int DoLink(const TUINT32 udwCacheType, SPartition* poReportUser, const TINT64 ddwUid);
    int DoUnlink(const TUINT32 udwCacheType, SPartition* poReportUser, const TINT64 ddwUid);
    int DoLinkNoLock(const TUINT32 udwCacheType, SPartition* poReportUser, const TINT64 ddwUid);
    int DoUnlinkNoLock(const TUINT32 udwCacheType, SPartition* poReportUser, const TINT64 ddwUid);
    SPartition* DoGet(const TUINT32 udwCacheType, const TINT64 ddwUid);
    SPartition* DoAlloc();
    int DoReleaseNoLock(SPartition** poReportUser);
    int DoUnlive(SPartition* poReportUser);
    int DoOverflow(SPartition* poReportUser);

public:
    static CCacheMgr* Instance();
    int Init(CConf *poConf, CTseLogger* pLog, CWritebackQueue *poWritebackQ);
    int Touch(const TUINT32 udwCacheType, const TINT64 ddwUid, const TBOOL bReposition);
    int CreateCache(const TUINT32 udwCacheType, const TINT64 ddwUid, const SItemSeq* szItemSeq, const TUINT32 udwItemSeqNum);
    int Query(const TUINT32 udwCacheType, const TINT64 ddwUid, SItemExt* szItemExt, TUINT32* pudwItemNum, TBOOL bIncludeDel = FALSE);
    int AppendItem(const TUINT32 udwMainType, const TUINT32 udwSecondType, const TINT64 ddwUid, const SItem* poNewItem);
    int WriteItem(const TUINT32 udwCacheType, const TINT64 ddwUid, const SItemExt* poItemExt);
    int GetItemUpdated(const TUINT32 udwCacheType, const TINT64 ddwUid, SItemSeq* szItemseq, TUINT32* pudwItemSeqNum);
    int DeleteItem(const TUINT32 udwCacheType, const TINT64 ddwUid);
    int DereferenceItem(const uint32_t udwCacheType, const TINT64 ddwUid);
};
