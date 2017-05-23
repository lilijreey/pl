#pragma once
#include "base/log/wtselogger.h"
#include "session.h"
#include "conf.h"

using namespace wtse::log;

/*************************************************************************************
Session 管理
1.单件模式
2.从业务so初始化业务
3.从业务so加载和初始化Session,并封装到SessionWrapper
4.提供取SessionWrapper，和回收SessionWrapper的功能
5.提供运行时替换业务so的功能
**************************************************************************************/
class CWritebackSessionMgr
{
private:
    static CWritebackSessionMgr                * m_poCSessionMgr;
    CWritebackSessionMgr();
public:
    ~CWritebackSessionMgr();

    /*
    *    获取SessionMgr对象
    *    \return
    *        S_OK 成功；S_FAIL 失败
    */
    static CWritebackSessionMgr    * Instance();

    /*
    *    初始化
    *    \return
    *        S_OK 成功；S_FAIL 失败
    */
    HRESULT    Init(CConf * poConf, CTseLogger *poLog);

    /*
    *    反初始化，释放资源
    *    \return
    *        S_OK 成功；S_FAIL 失败
    */
    HRESULT    Uninit();

    /*
    *    取SessionWrapper，无限等待
    *    \return
    *        S_OK 成功；S_FAIL 失败
    */
    HRESULT    WaitTillSession(CWritebackSession **ppoSessionWrapper);

    /*
    *    取SessionWrapper，有超时机制
    *    \return
    *        S_OK 成功；S_FAIL 失败
    */
    HRESULT    WaitTimeSession(CWritebackSession **ppoSessionWrapper);

    /*
    *    回收SessionWrapper
    *    \return
    *        S_OK 成功；S_FAIL 失败
    */
    HRESULT    ReleaseSesion(CWritebackSession *poSessionWrapper);

private:

    CWritebackQueue                 *m_poMemQue;                        // 互斥队列
    CTseLogger                      *m_poLog;                            // 日志对象

    TUINT32                         m_udwSessionNum;                    // Session总个数
    CWritebackSession               *m_apoSessionWrapprList;            // 包装器内存列表
    TBOOL volatile                  m_bReady;                            // 是否可以提供对外服务
};