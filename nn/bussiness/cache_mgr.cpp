#include "cache_mgr.h"

CCacheMgr* CCacheMgr::m_poCacheMgr = NULL;

CCacheMgr::CCacheMgr()
{

}

CCacheMgr::~CCacheMgr()
{

}

int CCacheMgr::LockCache(const TUINT32 udwCacheType)
{
    return pthread_mutex_lock(&(m_lockCache[udwCacheType]));
}

int CCacheMgr::UnlockCache(const TUINT32 udwCacheType)
{
    return pthread_mutex_unlock(&(m_lockCache[udwCacheType]));
}

int CCacheMgr::LockUid(const TINT64 ddwUid)
{
    TBOOL bWaiting = FALSE;
    map<TINT64, SLockCount*>::iterator it;

    while (1)
    {
        pthread_mutex_lock(&m_lockUid);
        it = m_mapUidLock.find(ddwUid);
        if (it == m_mapUidLock.end())
        {
            SLockCount* poNewLockCount = new SLockCount;
            m_mapUidLock.insert(map<TINT64, SLockCount*>::value_type(ddwUid, poNewLockCount));
            poNewLockCount->m_bOwn = TRUE;
            pthread_mutex_unlock(&m_lockUid);
            break;
        }
        else if(!it->second->m_bOwn)
        {
            if (bWaiting)
            {
                it->second->m_udwWaitCnt--;
            }
            it->second->m_bOwn = TRUE;
            pthread_mutex_unlock(&m_lockUid);
            break;
        }
        else
        {
            if (!bWaiting)
            {
                it->second->m_udwWaitCnt++;
                bWaiting = TRUE;
            }
        }
        pthread_mutex_unlock(&m_lockUid);
    }

    return 0;
}

int CCacheMgr::TryLockUid(const TINT64 ddwUid)
{
    int dwRetCode = 0;

    map<TINT64, SLockCount*>::iterator it;
    pthread_mutex_lock(&m_lockUid);
    it = m_mapUidLock.find(ddwUid);
    if (it == m_mapUidLock.end())
    {
        SLockCount* poNewLockCount = new SLockCount;
        m_mapUidLock.insert(map<TINT64, SLockCount*>::value_type(ddwUid, poNewLockCount));
        poNewLockCount->m_bOwn = TRUE;
    }
    else if (!it->second->m_bOwn)
    {
        it->second->m_bOwn = TRUE;
    }
    else
    {
        dwRetCode = -1;
    }
    pthread_mutex_unlock(&m_lockUid);

    return dwRetCode;
}

int CCacheMgr::UnlockUid(const TINT64 ddwUid)
{
    map<TINT64, SLockCount*>::iterator it;
    pthread_mutex_lock(&m_lockUid);
    it = m_mapUidLock.find(ddwUid);
    assert(it != m_mapUidLock.end());
    assert(it->second->m_bOwn);
    SLockCount* poLockCount = it->second;
    if (poLockCount->m_udwWaitCnt == 0)
    {
        m_mapUidLock.erase(ddwUid);
        delete poLockCount;
    }
    else
    {
        poLockCount->m_bOwn = FALSE;
    }
    pthread_mutex_unlock(&m_lockUid);

    return 0;
}

int CCacheMgr::DoLink(const TUINT32 udwCacheType, SPartition* poReportUser, const TINT64 ddwUid)
{
    LockCache(udwCacheType);
    int dwRetCode = DoLinkNoLock(udwCacheType, poReportUser, ddwUid);
    UnlockCache(udwCacheType);

    return dwRetCode;
}

int CCacheMgr::DoUnlink(const TUINT32 udwCacheType, SPartition* poReportUser, const TINT64 ddwUid)
{
    LockCache(udwCacheType);
    int dwRetCode = DoUnlinkNoLock(udwCacheType, poReportUser, ddwUid);
    UnlockCache(udwCacheType);

    return dwRetCode;
}

int CCacheMgr::DoLinkNoLock(const TUINT32 udwCacheType, SPartition* poReportUser, const TINT64 ddwUid)
{
    if (poReportUser == NULL)
    {
        TSE_LOG_ERROR(m_poLog, ("DoLinkNoLock:The Item to be linked is NULL!"));
        return -1;
    }

    if (udwCacheType >= MAX_NUM_CACHE_TYPE)
    {
        TSE_LOG_ERROR(m_poLog, ("DoLinkNoLock:The cache type[%u] is invalid!",udwCacheType));
        return -2;
    }

    map<TINT64, SPartition*> *mapCache;
    SPartition** pHead = NULL;
    SPartition** pTail = NULL;
    switch (udwCacheType)
    {
    case CACHE_TYPE_PLAYER:
        mapCache = &m_mapPlayerCache;
        pHead = &m_pHeadPlayer;
        pTail = &m_pTailPlayer;
        break;
    case CACHE_TYPE_ALLIANCE:
        mapCache = &m_mapAllianceCache;
        pHead = &m_pHeadAlliance;
        pTail = &m_pTailAlliance;
        break;
    case CACHE_TYPE_SERVER:
        mapCache = &m_mapServerCache;
        pHead = &m_pHeadServer;
        pTail = &m_pTailServer;
        break;
    case CACHE_TYPE_UNLIVE:
        mapCache = &m_mapUnliveCache;
        break;
    default:
        assert(0);
        break;
    }

    if (mapCache->find(ddwUid) == mapCache->end())
    {
        mapCache->insert(map<TINT64, SPartition*>::value_type(ddwUid, poReportUser));
    }
    else
    {
        TSE_LOG_ERROR(m_poLog, ("DoLinkNoLock:The uid[%ld] is already in the cache[%u]!", ddwUid, udwCacheType));
        return -3;
    }

    if (udwCacheType <= CACHE_TYPE_SERVER)
    {
        assert((*pHead && *pTail) || (*pHead == NULL&& *pTail == NULL));
        if (poReportUser == *pHead) return 0;

        poReportUser->m_pPrev = NULL;
        poReportUser->m_pNext = *pHead;

        if (*pHead)
        {
            (*pHead)->m_pPrev = poReportUser;
        }
        else
        {
            *pTail = poReportUser;
        }

        *pHead = poReportUser;

        TSE_LOG_DEBUG(m_poLog, ("DoLinkNoLock:Uid[%ld],HeadUid[%ld],TailUid[%ld]!", poReportUser->m_ddwUid, (*pHead)->m_ddwUid, (*pTail)->m_ddwUid));
    }

    return 0;
}

int CCacheMgr::DoUnlinkNoLock(const TUINT32 udwCacheType, SPartition* poReportUser, const TINT64 ddwUid)
{
    if (poReportUser == NULL)
    {
        TSE_LOG_DEBUG(m_poLog, ("DoUnlinkNoLock:The Item to be unlinked is NULL!"));
        return -1;
    }

    if (udwCacheType >= MAX_NUM_CACHE_TYPE)
    {
        TSE_LOG_ERROR(m_poLog, ("DoUnlinkNoLock:The cache type[%u] is invalid!",udwCacheType));
        return -2;
    }

    map<TINT64, SPartition*> *mapCache;
    SPartition** pHead = NULL;
    SPartition** pTail = NULL;
    switch (udwCacheType)
    {
    case CACHE_TYPE_PLAYER:
        mapCache = &m_mapPlayerCache;
        pHead = &m_pHeadPlayer;
        pTail = &m_pTailPlayer;
        break;
    case CACHE_TYPE_ALLIANCE:
        mapCache = &m_mapAllianceCache;
        pHead = &m_pHeadAlliance;
        pTail = &m_pTailAlliance;
        break;
    case CACHE_TYPE_SERVER:
        mapCache = &m_mapServerCache;
        pHead = &m_pHeadServer;
        pTail = &m_pTailServer;
        break;
    case CACHE_TYPE_UNLIVE:
        mapCache = &m_mapUnliveCache;
        break;
    default:
        assert(0);
        break;
    }

    if (mapCache->find(ddwUid) == mapCache->end())
    {
        TSE_LOG_ERROR(m_poLog, ("DoUnlinkNoLock:The uid[%ld] is not in the cache[%u]!", ddwUid, udwCacheType));
        return -3;
    }

    mapCache->erase(ddwUid);

    if (udwCacheType <= CACHE_TYPE_SERVER)
    {
        TSE_LOG_DEBUG(m_poLog, ("DoUnlinkNoLock:Uid[%ld],HeadUid[%p],TailUid[%p]!", poReportUser->m_ddwUid, (*pHead)->m_ddwUid, (*pTail)->m_ddwUid));
        assert((*pHead != NULL && *pTail != NULL));

        if (poReportUser->m_pPrev)
        {
            poReportUser->m_pPrev->m_pNext = poReportUser->m_pNext;
        }
        else
        {
            assert(poReportUser == *pHead);
            *pHead = poReportUser->m_pNext;
        }

        if (poReportUser->m_pNext)
        {
            poReportUser->m_pNext->m_pPrev = poReportUser->m_pPrev;
        }
        else
        {
            assert(poReportUser == *pTail);
            *pTail = poReportUser->m_pPrev;
        }
    }

    return 0;
}

SPartition* CCacheMgr::DoGet(const TUINT32 udwCacheType, const TINT64 ddwUid)
{
    if (udwCacheType >= MAX_NUM_CACHE_TYPE)
    {
        TSE_LOG_ERROR(m_poLog, ("DoGet:The cache type[%u] is invalid!",udwCacheType));
        return NULL;
    }

    SPartition* poTarget;

    map<TINT64, SPartition*> *mapCache;
    switch (udwCacheType)
    {
    case CACHE_TYPE_PLAYER:
        mapCache = &m_mapPlayerCache;
        break;
    case CACHE_TYPE_ALLIANCE:
        mapCache = &m_mapAllianceCache;
        break;
    case CACHE_TYPE_SERVER:
        mapCache = &m_mapServerCache;
        break;
    case CACHE_TYPE_UNLIVE:
        mapCache = &m_mapUnliveCache;
        break;
    default:
        assert(0);
        break;
    }

    LockCache(udwCacheType);
    map<TINT64, SPartition*>::iterator it = mapCache->find(ddwUid);
    if (it == mapCache->end())
    {
        poTarget = NULL;
        TSE_LOG_DEBUG(m_poLog, ("DoGet:The uid[%ld] is not in the cache[%u]!", ddwUid, udwCacheType));
    }
    else
    {
        poTarget = it->second;
        assert(poTarget->m_ddwUid == ddwUid);//调用DoGet前必须对Uid进行加锁，才能保证该断言
    }
    UnlockCache(udwCacheType);

    return poTarget;
}

SPartition* CCacheMgr::DoAlloc()
{
    LockCache(CACHE_TYPE_PLAYER);
    if (m_udwCachePartitionCnt > m_udwCacheThreshold)
    {
        TSE_LOG_DEBUG(m_poLog, ("DoAlloc:TotalSize[%u]!", m_udwCachePartitionCnt));
        SPartition* poSearch = m_pTailPlayer;
        for (size_t i = 0; i < m_udwCacheLRULoopNum && poSearch != NULL; i++)
        {
            TSE_LOG_DEBUG(m_poLog, ("DoAlloc:PoSearch,uid[%d]!", poSearch->m_ddwUid));
            if (0 != TryLockUid(poSearch->m_ddwUid))
            {
                TSE_LOG_DEBUG(m_poLog, ("DoAlloc:PoSearch,uid[%d],lock fail!", poSearch->m_ddwUid));
                poSearch = poSearch->m_pPrev;
                continue;
            }
            if (poSearch->m_udwRefCnt != 0)
            {
                TSE_LOG_DEBUG(m_poLog, ("DoAlloc:PoSearch,uid[%d],being referenced!", poSearch->m_ddwUid));
                UnlockUid(poSearch->m_ddwUid);
                poSearch = poSearch->m_pPrev;
                continue;
            }
            if (poSearch->DoCheckUpdated())
            {
                TSE_LOG_DEBUG(m_poLog, ("DoAlloc:PoSearch,uid[%d],ItemUpdated!", poSearch->m_ddwUid));
                UnlockUid(poSearch->m_ddwUid);
                poSearch = poSearch->m_pPrev;
                //SPartition* poUnlive = poSearch;
                //poSearch = poSearch->m_pPrev;
                //DoUnlinkNoLock(CACHE_TYPE_PLAYER, poUnlive, poUnlive->m_ddwUid);
                //DoUnlive(poUnlive);
                //UnlockUid(poUnlive->m_ddwUid);
                continue;
            }
            TSE_LOG_DEBUG(m_poLog, ("DoAlloc:PoSearch,uid[%d],Reusable!", poSearch->m_ddwUid));
            DoUnlinkNoLock(CACHE_TYPE_PLAYER, poSearch, poSearch->m_ddwUid);
            TINT64 ddwUid = poSearch->m_ddwUid;
            poSearch->Reset();
            UnlockUid(ddwUid);
            UnlockCache(CACHE_TYPE_PLAYER);

            return poSearch;
        }
    }

    if (m_udwCachePartitionCnt > m_udwCacheLimit)
    {
        TSE_LOG_ERROR(m_poLog, ("DoAlloc:Out of Cache Memory!"));
        UnlockCache(CACHE_TYPE_PLAYER);
        return NULL;
    }

    SPartition* poNewReportUser = new SPartition;
    if (poNewReportUser == NULL)
    {
        TSE_LOG_ERROR(m_poLog, ("DoAlloc:New Object failed!"));
        UnlockCache(CACHE_TYPE_PLAYER);
        return NULL;
    }
    m_udwCachePartitionCnt++;
    TSE_LOG_INFO(m_poLog, ("DoAlloc:Used Cache[%u]",m_udwCachePartitionCnt));
    UnlockCache(CACHE_TYPE_PLAYER);
    return poNewReportUser;
}

int CCacheMgr::DoReleaseNoLock(SPartition** poReportUser)
{
    delete *poReportUser;
    *poReportUser = NULL;
    m_udwCachePartitionCnt--;
    TSE_LOG_INFO(m_poLog, ("DoReleaseNoLock:Used Cache[%u]", m_udwCachePartitionCnt));

    return 0;
}

int CCacheMgr::DoUnlive(SPartition* poReportUser)
{
    DoLink(CACHE_TYPE_UNLIVE, poReportUser, poReportUser->m_ddwUid);

    CWritebackSession *poWritebackSession = NULL;
    if (S_OK != CWritebackSessionMgr::Instance()->WaitTimeSession(&poWritebackSession))
    {
        TSE_LOG_ERROR(m_poLog, ("DoUnlive:Get WritebackSession failed!"));
        return S_FAIL;
    }

    TSE_LOG_INFO(m_poLog, ("DoUnlive:Unlive succeed,Uid[%ld]!",poReportUser->m_ddwUid));

    poWritebackSession->m_vecKeys.push_back(poReportUser->m_ddwUid);
    poWritebackSession->m_ddwRid = -1;
    poWritebackSession->m_udwType = EN_WRITEBACK_TYPE_LRU;

    m_poWritebackQ->WaitTillPush(poWritebackSession);

    return 0;
}

int CCacheMgr::DoOverflow(SPartition* poReportUser)
{
    //1.把poReportUser的第一行的数据拷贝至poOverflowSeq，并对第一行做逻辑删除
    SItemSeq* poOverflowSeq = new SItemSeq;
    poOverflowSeq->m_ddwSeq = poReportUser->m_ddwBaseSeq;
    if (poReportUser->m_udwSize > MAX_ITEM_NUM_PER_SEQ)
    {
        poOverflowSeq->m_udwSize = MAX_ITEM_NUM_PER_SEQ;
    }
    else
    {
        poOverflowSeq->m_udwSize = poReportUser->m_udwSize;
    }
    memcpy(poOverflowSeq->m_szItems, &poReportUser->m_queItem[poReportUser->m_udwRear], poOverflowSeq->m_udwSize*sizeof(SItem));
    poReportUser->m_udwRear = poReportUser->DoForwardCursor(poReportUser->m_udwRear, MAX_ITEM_NUM_PER_SEQ);
    poReportUser->m_ddwBaseSeq++;
    poReportUser->m_udwSize -= poOverflowSeq->m_udwSize;

    //2.把poOverflowSeq放到OverflowMap中
    LockCache(CACHE_TYPE_OVERFLOW);
    map<TINT64, vector<SItemSeq*> >::iterator it = m_mapOverflow.find(poReportUser->m_ddwUid);
    if (it == m_mapOverflow.end())
    {
        vector<SItemSeq*> vecOverflow;
        vecOverflow.push_back(poOverflowSeq);
        m_mapOverflow.insert(map<TINT64, vector<SItemSeq*> >::value_type(poReportUser->m_ddwUid, vecOverflow));
    }
    else 
    {
        it->second.push_back(poOverflowSeq);
    }
    UnlockCache(CACHE_TYPE_OVERFLOW);

    //3.生成回写请求，请求回写队列把Overflow对象回写至Dynamodb
    CWritebackSession *poWritebackSession = NULL;
    if (S_OK != CWritebackSessionMgr::Instance()->WaitTimeSession(&poWritebackSession))
    {
        TSE_LOG_ERROR(m_poLog, ("DoOverflow:Get WritebackSession failed!"));
        return S_FAIL;
    }

    TSE_LOG_DEBUG(m_poLog, ("DoOverflow:Overflow succeed,Uid[%ld],Ridseq[%ld],seqsize[%u],flowsize[%u]!",
        poReportUser->m_ddwUid,poOverflowSeq->m_ddwSeq,poOverflowSeq->m_udwSize,it->second.size()));

    poWritebackSession->m_vecKeys.push_back(poReportUser->m_ddwUid);
    poWritebackSession->m_ddwRid = -1;
    poWritebackSession->m_udwType = EN_WRITEBACK_TYPE_OVERFLOW;

    m_poWritebackQ->WaitTillPush(poWritebackSession);

    return 0;
}

CCacheMgr* CCacheMgr::Instance()
{
    if (m_poCacheMgr == NULL)
    {
        m_poCacheMgr = new CCacheMgr;
    }
    return m_poCacheMgr;
}

int CCacheMgr::Init(CConf *poConf, CTseLogger* pLog, CWritebackQueue *poWritebackQ)
{
    m_poLog = pLog;
    m_poWritebackQ = poWritebackQ;

    m_pHeadPlayer = NULL;
    m_pTailPlayer = NULL;
    m_mapPlayerCache.clear();

    m_pHeadAlliance = NULL;
    m_pTailAlliance = NULL;
    m_mapAllianceCache.clear();

    m_pHeadServer = NULL;
    m_pTailServer = NULL;
    m_mapServerCache.clear();

    m_mapUnliveCache.clear();

    m_mapOverflow.clear();

    m_mapUidLock.clear();

    for (size_t i = 0; i < MAX_NUM_CACHE_TYPE; i++)
    {
        pthread_mutex_init(&m_lockCache[i], NULL);
    }

    pthread_mutex_init(&m_lockUid, NULL);

    m_udwCacheThreshold = poConf->m_udwCacheThreshold;
    m_udwCacheLimit = poConf->m_udwCacheLimit;
    m_udwCacheLRULoopNum = poConf->m_udwCacheLRULoopNum;
    m_udwCachePartitionCnt = 0;

    return 0;
}

int CCacheMgr::Touch(const TUINT32 udwCacheType, const TINT64 ddwUid, const TBOOL bReposition)
{
    int dwRetCode = 0;
    LockUid(ddwUid);
    SPartition* poTarget = DoGet(udwCacheType, ddwUid);
    if (poTarget != NULL)
    {
        poTarget->m_udwRefCnt++;

        if (bReposition && udwCacheType!=CACHE_TYPE_UNLIVE) //update LRU 
        {
            SPartition** pHead = NULL;
            SPartition** pTail = NULL;
            switch (udwCacheType)
            {
            case CACHE_TYPE_PLAYER:
                pHead = &m_pHeadPlayer;
                pTail = &m_pTailPlayer;
                break;
            case CACHE_TYPE_ALLIANCE:
                pHead = &m_pHeadAlliance;
                pTail = &m_pTailAlliance;
                break;
            case CACHE_TYPE_SERVER:
                pHead = &m_pHeadServer;
                pTail = &m_pTailServer;
                break;
            default:
                assert(0);
                break;
            }

            if (poTarget != *pHead)
            {
                DoUnlink(udwCacheType, poTarget, ddwUid);
                DoLink(udwCacheType, poTarget, ddwUid); //移动到head节点
            }
        }
    }
    else
    {
        dwRetCode = -1;
    }

    UnlockUid(ddwUid);

    return dwRetCode;
}

int CCacheMgr::CreateCache(const TUINT32 udwCacheType, const TINT64 ddwUid, const SItemSeq* szItemSeq, const TUINT32 udwItemSeqNum)
{
    int dwRetCode = 0;
    LockUid(ddwUid);
    SPartition* poTarget = DoGet(udwCacheType, ddwUid);
    if (poTarget == NULL)
    {
        TUINT32 udwBegin = 0, udwEnd = 0;

        SPartition* poUnlive = DoGet(CACHE_TYPE_UNLIVE, ddwUid);
        if (poUnlive == NULL)
        {
            udwBegin = udwItemSeqNum <= MAX_SEQ_NUM ? 0 : (udwItemSeqNum - MAX_SEQ_NUM);
            udwEnd = udwItemSeqNum;
            poTarget = DoAlloc();
            if (poTarget != NULL)
            {
                poTarget->m_ddwUid = ddwUid;
                DoLink(udwCacheType, poTarget, ddwUid);
                if (udwItemSeqNum > 0)
                {
                    poTarget->m_udwSize = (udwEnd - udwBegin - 1)*MAX_ITEM_NUM_PER_SEQ + szItemSeq[udwItemSeqNum - 1].m_udwSize;
                    if (poTarget->m_udwSize == MAX_SEQ_NUM*MAX_ITEM_NUM_PER_SEQ)
                    {
                        poTarget->m_udwFront = 0;
                    }
                    else
                    {
                        poTarget->m_udwFront = poTarget->m_udwSize;
                    }
                    poTarget->m_ddwBaseSeq = szItemSeq[udwBegin].m_ddwSeq;
                }
            }

            TSE_LOG_DEBUG(m_poLog, ("CreateCache:Unlive is NULL,Uid[%ld],type[%u],begin[%u],end[%u]", ddwUid, udwCacheType, udwBegin, udwEnd));
        }
        else
        {
            DoUnlink(CACHE_TYPE_UNLIVE, poUnlive, ddwUid);
            DoLink(udwCacheType, poUnlive, ddwUid);
            poTarget = poUnlive;

            if (poTarget->m_udwSize == 0)//当且仅当在同一个用户创建活跃缓存和非活跃缓存且该用户不存在任何report时可能出现非活跃的size为0的情况
            {
                udwBegin = udwItemSeqNum <= MAX_SEQ_NUM ? 0 : (udwItemSeqNum - MAX_SEQ_NUM);
                udwEnd = udwItemSeqNum;
                if (udwItemSeqNum > 0)
                {
                    poTarget->m_udwSize = (udwEnd - udwBegin - 1)*MAX_ITEM_NUM_PER_SEQ + szItemSeq[udwItemSeqNum - 1].m_udwSize;
                    if (poTarget->m_udwSize == MAX_SEQ_NUM*MAX_ITEM_NUM_PER_SEQ)
                    {
                        poTarget->m_udwFront = 0;
                    }
                    else
                    {
                        poTarget->m_udwFront = poTarget->m_udwSize;
                    }
                    poTarget->m_ddwBaseSeq = szItemSeq[udwBegin].m_ddwSeq;
                }
                TSE_LOG_DEBUG(m_poLog, ("CreateCache:Unlive is zero,Target[%p], Uid[%ld],type[%u],begin[%u],end[%u]", poTarget, ddwUid, udwCacheType, udwBegin, udwEnd));
            }
            else if (poUnlive->m_udwSize > (MAX_SEQ_NUM - 1)*MAX_ITEM_NUM_PER_SEQ)//非活跃状态下新增的Report已处于饱和状态，即占用了缓存中分配的所有Seq
            {
                //不需要使用任何从外部获取的数据
                //udwBegin = 0;
                //udwEnd = 0;
                TSE_LOG_DEBUG(m_poLog, ("CreateCache:Unlive is full,Uid[%ld],type[%u],size[%u]", ddwUid, udwCacheType, poUnlive->m_udwSize));
            }
            else//非活跃缓存已经有部分数据，但是没有饱和，最常见的情况
            {
                assert(poTarget->m_udwRear == 0);//由于未满，因此rear端必然处于初始位置，这是下面合并逻辑的根本依据

                if (udwItemSeqNum > 0)
                {
                    assert(poTarget->m_ddwBaseSeq >= szItemSeq[0].m_ddwSeq);
                    udwEnd = poTarget->m_ddwBaseSeq - szItemSeq[0].m_ddwSeq;
                    assert(udwEnd < udwItemSeqNum);//外部数据和非活跃数据至少会有1条seq的数据重复,重复的数据不需要写入缓存
                    TUINT32 udwCurSeqNum = poTarget->GetMaxSeq() - poTarget->m_ddwBaseSeq + 1;
                    TUINT32 udwTotalSeqNum = udwCurSeqNum + (udwEnd - udwBegin);
                    if (udwTotalSeqNum > MAX_SEQ_NUM)
                    {
                        udwBegin = udwTotalSeqNum - MAX_SEQ_NUM;
                    }

                    TUINT32 udwRealItemSeqNum = udwEnd - udwBegin;
                    if (udwRealItemSeqNum > 0)
                    {
                        poTarget->m_udwRear = (MAX_SEQ_NUM - udwRealItemSeqNum)*MAX_ITEM_NUM_PER_SEQ;
                        poTarget->m_ddwBaseSeq = szItemSeq[udwBegin].m_ddwSeq;
                        poTarget->m_udwSize += udwRealItemSeqNum*MAX_ITEM_NUM_PER_SEQ;
                    }
                }
                TSE_LOG_DEBUG(m_poLog, ("CreateCache:Unlive is partial,Target[%p], Uid[%ld],type[%u],begin[%u],end[%u]", poTarget, ddwUid, udwCacheType, udwBegin, udwEnd));
            }
        }
        if (poTarget != NULL)
        {
            TUINT32 udwTargetIdx = 0;
            for (TUINT32 udwIdx = udwBegin; udwIdx < udwEnd; udwIdx++)
            {
                udwTargetIdx = (udwIdx - udwBegin)*MAX_ITEM_NUM_PER_SEQ + poTarget->m_udwRear;
                memcpy(&(poTarget->m_queItem[udwTargetIdx]), szItemSeq[udwIdx].m_szItems, szItemSeq[udwIdx].m_udwSize*sizeof(SItem));
            }

            //将重复的项置零
            map<TINT64, TUINT32> mapRid2Idx;
            mapRid2Idx.clear();
            map<TINT64, TUINT32>::iterator it;
            TUINT32 udwCount = 0;
            for (TUINT32 udwIdx = poTarget->m_udwRear; udwCount<poTarget->m_udwSize; udwIdx = poTarget->DoForwardCursor(udwIdx,1),udwCount++)
            {
                if (poTarget->m_queItem[udwIdx].m_nRid == 0)
                {
                    continue;
                }

                it = mapRid2Idx.find(poTarget->m_queItem[udwIdx].m_nRid);
                if (it == mapRid2Idx.end())
                {
                    mapRid2Idx.insert(map<TINT64, TUINT32>::value_type(poTarget->m_queItem[udwIdx].m_nRid,udwIdx));
                }
                else
                {
                    TSE_LOG_DEBUG(m_poLog, ("CreateCache:Item[%ld] of Index[%u] duplicate, Uid[%ld],type[%u]",
                        it->first, it->second, ddwUid, udwCacheType));
                    poTarget->m_queItem[it->second].m_nRid = 0;
                    it->second = udwIdx;
                }
            }

            TSE_LOG_DEBUG(m_poLog, ("CreateCache:Create Success, Uid[%ld],type[%u],begin[%u],end[%u],baseseq[%ld],size[%u],front[%u],rear[%u]", ddwUid, udwCacheType, udwBegin, udwEnd,
                poTarget->m_ddwBaseSeq,poTarget->m_udwSize,poTarget->m_udwFront,poTarget->m_udwRear));
        }
        else
        {
            dwRetCode = -1;
            TSE_LOG_ERROR(m_poLog, ("CreateCache:Create failed as can not allocate new partition, Uid[%ld],type[%u]", ddwUid, udwCacheType));
        }
    }
    else
    {
        dwRetCode = 1;
        TSE_LOG_DEBUG(m_poLog, ("CreateCache:Partition exist, Uid[%ld],type[%u]", ddwUid, udwCacheType));
    }

    UnlockUid(ddwUid);

    return dwRetCode;
}

int CCacheMgr::Query(const TUINT32 udwCacheType, const TINT64 ddwUid, SItemExt* szItemExt, TUINT32* pudwItemNum, TBOOL bIncludeDel)
{
    int dwRetCode = 0;
    *pudwItemNum = 0;
    LockUid(ddwUid);
    SPartition* poTarget = DoGet(udwCacheType, ddwUid);
    if (poTarget != NULL)
    {
        TUINT32 udwCount = 0;
        //回环查询
        for (TUINT32 udwIdx = poTarget->m_udwRear; udwCount < poTarget->m_udwSize; udwIdx = poTarget->DoForwardCursor(udwIdx, 1))
        {
            if (    poTarget->m_queItem[udwIdx].m_nRid > 0 && 
                ((!(poTarget->m_queItem[udwIdx].m_nStatus & EN_STATUS_DEL) || 
                  bIncludeDel)))
            {
                (szItemExt + *pudwItemNum)->m_objItem = poTarget->m_queItem[udwIdx];
                (szItemExt + *pudwItemNum)->m_ddwSeq = poTarget->Index2Seq(udwIdx);
                (szItemExt + *pudwItemNum)->m_udwPos = udwIdx % MAX_ITEM_NUM_PER_SEQ;
                (*pudwItemNum)++;
            }

            udwCount++;

            //TSE_LOG_DEBUG(m_poLog, ("Query:curItem[rid=%ld,rtype=%u,gtype=%u,status=%u,seq=%ld,pos=%u], Uid[%ld],type[%u]",
            //    poTarget->m_queItem[udwIdx].m_nRid, poTarget->m_queItem[udwIdx].m_nReport_type,
            //    poTarget->m_queItem[udwIdx].m_nType, poTarget->m_queItem[udwIdx].m_nStatus,
            //    poTarget->Index2Seq(udwIdx), udwIdx % MAX_ITEM_NUM_PER_SEQ,
            //    ddwUid, udwCacheType));
        }
        TSE_LOG_DEBUG(m_poLog, ("Query:Query Success, Uid[%ld],type[%u],totalsize[%u],validsize[%u]", ddwUid, udwCacheType, udwCount, *pudwItemNum));
    }
    else
    {
        dwRetCode = -1;
        TSE_LOG_ERROR(m_poLog, ("Query:Query failed as target partition not exist, Uid[%ld],type[%u]", ddwUid, udwCacheType));
    }
    UnlockUid(ddwUid);
    return dwRetCode;
}

int CCacheMgr::AppendItem(const TUINT32 udwMainType, const TUINT32 udwSecondType, const TINT64 ddwUid, const SItem* poNewItem)
{
    int dwRetCode = 0;
    LockUid(ddwUid);
    SPartition* poTarget = DoGet(udwMainType, ddwUid);
    if (poTarget == NULL)
    {
        poTarget = DoGet(udwSecondType, ddwUid);
        dwRetCode = 1;
    }
    if (poTarget == NULL)
    {
        dwRetCode = -1;
        TSE_LOG_ERROR(m_poLog, ("AppendItem:Append failed as target partition both not exist, Uid[%ld],maintype[%u],sndtype[%u]", ddwUid, udwMainType,udwSecondType));
    }
    else
    {
        TSE_LOG_DEBUG(m_poLog, ("AppendItem:Append Uid[%ld],maintype[%u],sndtype[%u],size[%u],baseseq[%ld],front[%u],rear[%u]", ddwUid, udwMainType, udwSecondType,
            poTarget->m_udwSize,poTarget->m_ddwBaseSeq,poTarget->m_udwFront,poTarget->m_udwRear));

        TINT32 dwAppendRet = poTarget->DoAppend(poNewItem, FALSE);

        if (dwAppendRet!=0)//DoAppend失败只有一种可能，即缓存队列已写满，需要淘汰最老的那一行数据
        {
            if (DoOverflow(poTarget) == 0) 
            {
                dwAppendRet = poTarget->DoAppend(poNewItem, FALSE);
                assert(dwAppendRet == 0);
            }
            else
            {
                dwRetCode = -2;
                TSE_LOG_ERROR(m_poLog, ("AppendItem:Append failed as get writeback session failed, Uid[%ld],maintype[%u],sndtype[%u]", ddwUid, udwMainType, udwSecondType));
            }
        }

        TSE_LOG_DEBUG(m_poLog, ("AppendItem:Append ret[%d], Uid[%ld],maintype[%u],sndtype[%u],size[%u],baseseq[%ld],front[%u],rear[%u]", dwRetCode, ddwUid, udwMainType, udwSecondType,
            poTarget->m_udwSize, poTarget->m_ddwBaseSeq, poTarget->m_udwFront, poTarget->m_udwRear));
    }
    UnlockUid(ddwUid);
    return dwRetCode;
}

int CCacheMgr::WriteItem(const TUINT32 udwCacheType, const TINT64 ddwUid, const SItemExt* poItemExt)
{
    int dwRetCode = 0;
    LockUid(ddwUid);
    SPartition* poTarget = DoGet(udwCacheType, ddwUid);
    if (poTarget == NULL)
    {
        dwRetCode = -1;
        TSE_LOG_ERROR(m_poLog, ("WriteItem:write failed as target partition not exist, Uid[%ld],cachetype[%u]", ddwUid, udwCacheType));
    }
    else
    {
        if (poItemExt->m_ddwSeq < poTarget->m_ddwBaseSeq)
        {
            dwRetCode = 1;
            TSE_LOG_DEBUG(m_poLog, ("WriteItem:write failed as seq[%ld] is less than baseseq[%ld], Uid[%ld],cachetype[%u]", poItemExt->m_ddwSeq, poTarget->m_ddwBaseSeq, ddwUid, udwCacheType));
        }
        else
        {
            TUINT32 udwOffset = (poItemExt->m_ddwSeq - poTarget->m_ddwBaseSeq)*MAX_ITEM_NUM_PER_SEQ + poItemExt->m_udwPos;
            if (udwOffset >= poTarget->m_udwSize)
            {
                dwRetCode = -2;
                TSE_LOG_ERROR(m_poLog, ("WriteItem:write failed as over size, Uid[%ld],cachetype[%u],size[%u],seq[%ld],pos[%u]", ddwUid, udwCacheType,
                    poTarget->m_udwSize,poItemExt->m_ddwSeq,poItemExt->m_udwPos));
            }
            else
            {
                TUINT32 udwIdx = poTarget->DoForwardCursor(poTarget->m_udwRear, udwOffset);
                poTarget->m_queItem[udwIdx] = poItemExt->m_objItem;
                TUINT32 udwFlagIdx = poTarget->Index2UpdatedFlagIndex(udwIdx);
                BITSET(poTarget->m_bmapSeqUpdatedFlag, udwFlagIdx);
                TSE_LOG_DEBUG(m_poLog, ("WriteItem:write success, Uid[%ld],cachetype[%u],size[%u],seq[%ld],pos[%u],index[%u],flagindex[%u]", ddwUid, udwCacheType,
                    poTarget->m_udwSize, poItemExt->m_ddwSeq, poItemExt->m_udwPos,udwIdx,udwFlagIdx));
            }
        }
    }
    UnlockUid(ddwUid);
    return dwRetCode;
}

int CCacheMgr::GetItemUpdated(const TUINT32 udwCacheType, const TINT64 ddwUid, SItemSeq* szItemseq, TUINT32* pudwItemSeqNum)
{
    int dwRetCode = 0;
    *pudwItemSeqNum = 0;
    if (udwCacheType == CACHE_TYPE_OVERFLOW)
    {
        TUINT32 udwIdx = 0;
        map<TINT64, vector<SItemSeq*> >::iterator it;
        LockCache(udwCacheType);
        it = m_mapOverflow.find(ddwUid);
        if (it != m_mapOverflow.end())
        {
            for (; udwIdx < it->second.size(); udwIdx++)
            {
                if (*pudwItemSeqNum == MAX_SEQ_NUM)//一次最多只能获取MAX_SEQ_NUM
                {
                    break;
                }
                if (it->second[udwIdx] == NULL)//空指针表示已经被获取过，无需重复获取
                {
                    continue;
                }
                memcpy((szItemseq + *pudwItemSeqNum)->m_szItems, it->second[udwIdx]->m_szItems, it->second[udwIdx]->m_udwSize*sizeof(SItem));
                (szItemseq + *pudwItemSeqNum)->m_udwSize = it->second[udwIdx]->m_udwSize;
                (szItemseq + *pudwItemSeqNum)->m_ddwSeq = it->second[udwIdx]->m_ddwSeq;
                (*pudwItemSeqNum)++;

                delete (it->second[udwIdx]);
                it->second[udwIdx] = NULL;
            }
            if (udwIdx == it->second.size())
            {
                m_mapOverflow.erase(ddwUid);
                TSE_LOG_DEBUG(m_poLog, ("GetItemUpdated:erase cache size[%u], Uid[%ld],cachetype[%u]", udwIdx, ddwUid, udwCacheType));
            }
        }
        UnlockCache(udwCacheType);

        return dwRetCode;
    }


    LockUid(ddwUid);
    SPartition* poTarget = DoGet(udwCacheType, ddwUid);
    if (poTarget !=NULL)
    {
        TUINT32 udwCount = 0;
        TUINT32 udwSeqSize = 0;
        TUINT32 udwFlagIdx = 0;
        for (TUINT32 udwIdx = poTarget->m_udwRear; udwCount < poTarget->m_udwSize; udwIdx=poTarget->DoForwardCursor(udwIdx,MAX_ITEM_NUM_PER_SEQ))
        {
            udwFlagIdx = poTarget->Index2UpdatedFlagIndex(udwIdx);
            TSE_LOG_DEBUG(m_poLog, ("GetItemUpdated:Uid[%ld],cachetype[%u],updatedflag[%u,%u]", ddwUid, udwCacheType,
                udwFlagIdx, BITTEST(poTarget->m_bmapSeqUpdatedFlag, udwFlagIdx)));
            if (BITTEST(poTarget->m_bmapSeqUpdatedFlag,udwFlagIdx))
            {
                if (poTarget->m_udwSize - udwCount > MAX_ITEM_NUM_PER_SEQ)
                {
                    udwSeqSize = MAX_ITEM_NUM_PER_SEQ;
                }
                else
                {
                    udwSeqSize = poTarget->m_udwSize - udwCount;
                }

                memcpy((szItemseq + *pudwItemSeqNum)->m_szItems, &(poTarget->m_queItem[udwIdx]), udwSeqSize*sizeof(SItem));
                (szItemseq + *pudwItemSeqNum)->m_udwSize = udwSeqSize;
                (szItemseq + *pudwItemSeqNum)->m_ddwSeq = poTarget->Index2Seq(udwIdx);
                (*pudwItemSeqNum)++;

                BITCLEAR(poTarget->m_bmapSeqUpdatedFlag, udwFlagIdx);
            }

            udwCount += MAX_ITEM_NUM_PER_SEQ;
        }
    }
    else
    {
        dwRetCode = -1;
    }
    UnlockUid(ddwUid);
    return dwRetCode;
}

int CCacheMgr::DeleteItem(const TUINT32 udwCacheType, const TINT64 ddwUid)
{
    if (udwCacheType == CACHE_TYPE_OVERFLOW) //Overflow型的缓存不需要显式地删除，其删除详见GetItemUpdated
    {
        return 0;
    }

    LockUid(ddwUid);
    SPartition* poTarget = DoGet(udwCacheType, ddwUid);
    if (poTarget != NULL)
    {
        if (!poTarget->DoCheckUpdated() && poTarget->m_udwRefCnt==0) //能删则删，不能删则说明最近被更新过或者使用，等待下一个回写指令触发删除即可
        {
            DoUnlink(udwCacheType, poTarget, ddwUid);
            DoReleaseNoLock(&poTarget);
        }
    }
    UnlockUid(ddwUid);
    return 0;
}

int CCacheMgr::DereferenceItem(const uint32_t udwCacheType, const TINT64 ddwUid)
{
    LockUid(ddwUid);
    SPartition* poTarget = DoGet(udwCacheType, ddwUid);
    if (poTarget != NULL)
    {
        if (poTarget->m_udwRefCnt>0)
        {
            poTarget->m_udwRefCnt--;
        }
    }
    UnlockUid(ddwUid);
    return 0;
}
