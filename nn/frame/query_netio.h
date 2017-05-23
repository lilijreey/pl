#ifndef _QUERY_NET_IO_H_
#define _QUERY_NET_IO_H_

#include "my_include.h"
#include "session.h"
#include "conf.h"

/*************************************************************************************
    前端请求网络IO
    1.接收请求数据
    2.获取SessionWrapper，放入工作队列
**************************************************************************************/
class CQueryNetIO : public ITasksGroupCallBack
{
public :
    CQueryNetIO();
    ~CQueryNetIO();

    /*
     *    初始化函数
     */
    TINT32 Init(CConf *poConf, CTseLogger *poLog, CWorkQueue *poWorkQueue, CWritebackQueue *m_poDelayTaskQueue);

    /*
     *    反初始化函数
     */
    TINT32 Uninit();

public :

    /*
     *    网络IO线程启动函数
     */
    static TVOID * RoutineNetIO(TVOID *pParam);

    /*
     *    开始网络服务
     */
    TVOID StartNetServ();

    /*
     *    停止网络服务
     */
    TVOID StopNetServ();

    
    /*
     *    网络消息回调
     */
    virtual TVOID OnTasksFinishedCallBack(LTasksGroup *pstTasksGrp);

    /*
     *    网络消息请求
     */
    virtual TVOID OnUserRequest(LongConnHandle stHandle, const TUCHAR *pszData, TUINT32 udwDataLen, TINT32 &dwWillResponse);

     /*
     *  关闭长连接，特殊时候用
     *    主要可用于想继续提供检索服务，但不想支持更新，
     *   而不想重启程序时用
     */
    TVOID StopLongConn();

     /*
         *   特殊时候用
     *  重新初始化长链接,调用此函数时，需要重开一个线程，运行RoutineNetIO
     */  
    TINT32 RestartLongConn();

private :
    /*
     *    处理请求
     */
    TINT32 OnRequest(LongConnHandle stHandle, TUINT32 udwSeqNum, 
                            const TUCHAR *pszData, TUINT32 udwDataLen, TUINT16 uwServType);

private :
    /*
     *    创建监听socket
     */
    SOCKET CreateListenSocket(TCHAR* pszListenHost,    TUINT16 uwPort);

    /*
     *    关闭监听socket
     */
    TINT32 CloseListenSocket();

    /*
     *    获取IP，端口
     */
    TINT32 GetIp2PortByHandle(LongConnHandle stHandle, TUINT16 *puwPort, TCHAR **ppszIp);

    /*
     *    向上游返回错误
     */
    TINT32 SendBackErr(LongConnHandle stHandle, TUINT32 udwServType, TUINT32 udwSeq);

public :
    ILongConn *m_pLongConn;  //长连接
    CTseLogger *m_poLog;  // Log

    CWorkQueue *m_poTaskQueue;
    CWritebackQueue *m_poDelayTaskQueue;

    TINT32 m_hListenSock;  // 监听socket

    CBaseProtocolUnpack *m_poUnpack;  // 解包器
    CBaseProtocolPack *m_poPack;  // 打包器

    TBOOL m_bServNew;  // 是否继续新的服务
    TBOOL m_bRelease;  //看是否能释放长链接资源
    static TBOOL m_bRunLongConn;  //是否关掉长链接

    TUINT32 m_udwSeqno;


};

#endif ///< _KEY_VALUE_QUERY_NETIO_HH

