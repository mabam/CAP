/*
 * $Author: djh $ $Date: 91/03/14 13:45:20 $
 * $Header: afpdsi.c,v 2.2 91/03/14 13:45:20 djh Exp $
 * $Revision: 2.2 $
 *
 */

/*
 * afpdsi.c - Data Stream Interface
 *
 * AFP via a Transport Protocol (eg: TCP/IP)
 *
 * AppleTalk package for UNIX
 *
 * The following routines implement a lightweight extra
 * layer between AFP (as embodied in the AUFS code), the
 * original ASP via ATP layer, and delivery via other
 * Transport Protocol layers, currently only TCP/IP.
 *
 * Refer: "AppleTalk Filing Protocol 2.2 &
 *         AFP over TCP/IP Specification"
 * 
 * SSS == Server Session Socket
 * SLS == Session Listening Socket
 * WSS == Workstation Session Socket
 *
 * Copyright (c) 1997 The University of Melbourne
 * David Hornsby <djh@munnari.OZ.AU>
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <netat/appletalk.h>
#include "../../lib/cap/abasp.h"	/* urk */
#include "afpdsi.h"

#ifdef HAVE_WRITEV
#include <sys/uio.h>
#endif /* HAVE_WRITEV */

/*
 * aufs AFP routines
 *
 */
int dsiInit();
int dsiGetSession();
int dsiFork();
int dsiTickleUserRoutine();
int dsiGetRequest();
int dsiWrtContinue();
int dsiWrtReply();
int dsiCmdReply();
int dsiAttention();
int dsiGetNetworkInfo();
int dsiGetParms();
int dsiCloseSession();
int dsiShutdown();
int dsiTCPIPCloseSLS();

/*
 * AppleTalk Session Protocol routines
 * (including some formerly 'private')
 *
 */
int SPInit();
int SPGetSession();
int SPFork();
int SPTickleUserRoutine();
int SPGetRequest();
int SPWrtContinue();
int SPWrtReply();
int SPCmdReply();
int SPAttention();
int SPGetNetworkInfo();
int SPGetParms();
int SPCloseSession();
int SPShutdown();

ASPSSkt *aspsskt_find_slsrefnum();
ASPSkt *aspskt_find_sessrefnum();
ASPSkt *aspskt_find_active();
ASPQE *create_aq();

void stopasptickle();
void stop_ttimer();
void delete_aq();
void Timeout();

/*
 * local TCP/IP routines
 *
 */
int dsiTCPIPInit();
int dsiTCPIPFork();
int dsiTCPIPGetRequest();
int dsiTCPWrtContinue();
int dsiTCPIPWrtReply();
int dsiTCPIPCmdReply();
int dsiTCPIPReply();
int dsiTCPIPTickleUserRoutine();
int dsiTCPIPCloseSession();
int dsiTCPIPAttention();
int dsiTCPIPGetNetworkInfo();
int dsiTCPIPWrite();
int dsiTCPIPCloseSLS();

/*
 * globals
 *
 */
#ifdef DEBUG_AFP_CMD
extern FILE *dbg;
#endif /* DEBUG_AFP_CMD */

extern int errno;
extern int asip_enable;
extern char *dsiTCPIPFilter;
private struct dsi_sess *dsi_session = NULL;

/*
 * DSI transport-layer demultiplexing
 *
 */

/*
 * Set up a Server Listening Socket (SLS) for both
 * ASP/ATP and TCP/IP.
 *
 * SLSEntityIdentifier - server AppleTalk address
 * ServiceStatusBlock - pointer to ServerInfo data
 * ServiceStatusBlockSize - ServerInfo data size
 * SLSRefNum - return a session RefNum
 *
 */

int
dsiInit(SLSEntityIdentifier, ServiceStatusBlock,
 ServiceStatusBlockSize, SLSRefNum)
AddrBlock *SLSEntityIdentifier; /* SLS Net id */
char *ServiceStatusBlock;       /* block with status info */
int ServiceStatusBlockSize;     /* size of status info */
int *SLSRefNum;                 /* sls ref num return place */
{
  int result;
  extern int numasp;

  /*
   * allocate & initialise space for DSI session data
   *
   */
  if (numasp <= 0)
    return(-1);

  if ((dsi_session = (struct dsi_sess *)
   malloc(sizeof(struct dsi_sess)*numasp)) == NULL)
    return(-1);

  bzero((char *)dsi_session, sizeof(struct dsi_sess)*numasp);

  /*
   * allocate SLSRefNum, initialise AppleTalk Session Protocol SLS
   *
   */
  result = SPInit(SLSEntityIdentifier, ServiceStatusBlock,
            ServiceStatusBlockSize, SLSRefNum);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    fprintf(dbg, "** SPInit(): (PID %d)\n", getpid());
    fprintf(dbg, "\tSLSRefNum: %d\n", *SLSRefNum);
    fprintf(dbg, "\tServiceStatusBlockSize: %d\n", ServiceStatusBlockSize);
    fprintf(dbg, "\tresult: %d\n", result);
    fprintf(dbg, "\n\n");
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (result != noErr)
    return(result);

  /*
   * if enabled, setup TCP/IP SLS (uses same SLSRefNum)
   *
   */
  if (asip_enable)
    if (dsiTCPIPInit(SLSEntityIdentifier, ServiceStatusBlock,
     ServiceStatusBlockSize, SLSRefNum) != noErr)
      asip_enable = 0;

  return(noErr);
}

/*
 * set up to wait for a new session to start
 *
 * SLSRefNum - Session Listening Socket RefNum
 * SessRefNum - returns new session reference number
 * comp - completion flag/error
 *
 */

int
dsiGetSession(SLSRefNum, SessRefNum, comp)
int SLSRefNum;
int *SessRefNum;
int *comp;
{
  int result;

  /*
   * get a session reference number,
   *
   */
  result = SPGetSession(SLSRefNum, SessRefNum, comp);

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    fprintf(dbg, "** SPGetSession(): (PID %d)\n", getpid());
    fprintf(dbg, "\tSLSRefNum: %d\n", SLSRefNum);
    fprintf(dbg, "\tSessRefNum: %d\n", *SessRefNum);
    fprintf(dbg, "\tcomp: %d\n", *comp);
    fprintf(dbg, "\tresult: %d\n", result);
    fprintf(dbg, "\n\n");
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  if (result != noErr)
    return(result);

  /*
   * assume that this session is going to be
   * AppleTalk, until we find out otherwise
   * (this depends on what type of OpenSession
   * packet actually arrives)
   *
   */
  dsi_session[*SessRefNum].sesstype = DSI_SESSION_ATALK;

  /*
   * initialise data structure for DSI state
   *
   */
  dsi_session[*SessRefNum].timeout = 0;
  dsi_session[*SessRefNum].aspqe = NULL;
  dsi_session[*SessRefNum].sess_id_in = 0;
  dsi_session[*SessRefNum].sess_id_out = 0;
  dsi_session[*SessRefNum].state = DSI_STATE_HDR;
  dsi_session[*SessRefNum].lenleft = sizeof(struct dsi_hdr);
  dsi_session[*SessRefNum].ptr = (char *)&dsi_session[*SessRefNum].hdr;

  return(noErr);
}

/*
 * fork and create new process to handle session
 *
 * SessRefNum - session reference number
 * stickle - want server tickle
 * ctickle - want client tickle
 *
 */

int
dsiFork(SessRefNum, stickle, ctickle)
int SessRefNum;
int stickle;
int ctickle;
{
  /*
   * if AppleTalk, hand off to Session Protocol
   *
   */
  if (dsi_session[SessRefNum].sesstype == DSI_SESSION_ATALK)
    return(SPFork(SessRefNum, stickle, ctickle));

  /*
   * handle locally for TCP/IP
   *
   */
  if (dsi_session[SessRefNum].sesstype == DSI_SESSION_TCPIP)
    return(dsiTCPIPFork(SessRefNum, stickle, ctickle));

  return(ParamErr);
}

/*
 * set the user-timeout routine and argument
 *
 * this needs to be handled in the parent
 * process for AppleTalk connections and in
 * the child process for TCP/IP connections
 *
 * 'pid' was obtained from the approriate fork()
 *
 */

int
dsiTickleUserRoutine(SessRefNum, routine, pid)
int SessRefNum;
int (*routine)();
int pid;
{
  /*
   * AppleTalk
   *
   */
  if (pid)
    if (dsi_session[SessRefNum].sesstype == DSI_SESSION_ATALK)
      return(SPTickleUserRoutine(SessRefNum, routine, pid));

  /*
   * TCP/IP
   *
   */
  if (!pid)
    if (dsi_session[SessRefNum].sesstype == DSI_SESSION_TCPIP)
      return(dsiTCPIPTickleUserRoutine(SessRefNum, routine, getpid()));

  return(noErr);
}

/*
 * set up to wait for a request on SSS
 *
 * SessRefNum - session reference number
 * ReqBuff - request command buffer
 * ReqBuffSize - request command buffer size
 * ReqRefNum - pointer to a special command block
 * SPReqType - returns command request type
 * ActRcvdReqLen - returns command length
 * comp - completion flag/error
 *
 */

int
dsiGetRequest(SessRefNum, ReqBuff, ReqBuffSize,
 ReqRefNum, SPReqType, ActRcvdReqLen, comp)
int SessRefNum;
char *ReqBuff;
int ReqBuffSize;
ASPQE **ReqRefNum;
int *SPReqType;
int *ActRcvdReqLen;
int *comp;
{
  /*
   * AppleTalk
   *
   */
  if (dsi_session[SessRefNum].sesstype == DSI_SESSION_ATALK)
    return(SPGetRequest(SessRefNum, ReqBuff, ReqBuffSize,
     ReqRefNum, SPReqType, ActRcvdReqLen, comp));

  /*
   * TCP/IP
   *
   */
  if (dsi_session[SessRefNum].sesstype == DSI_SESSION_TCPIP)
    return(dsiTCPIPGetRequest(SessRefNum, ReqBuff, ReqBuffSize,
     ReqRefNum, SPReqType, ActRcvdReqLen, comp));

  return(ParamErr);
}

/*
 * 'read' data sent by client
 *
 * SessRefNum - session reference number
 * ReqRefNum - client connection details (addr, TID)
 * Buffer - final location for data
 * BufferSize - maximum amount of data we can handle
 * ActLenRcvd - actual amount of date received
 * atptimeout - ATP get data timeout
 * comp - completion flag/error
 *
 */

int
dsiWrtContinue(SessRefNum, ReqRefNum, Buffer, BufferSize,
 ActLenRcvd, atptimeout, comp)
int SessRefNum;
ASPQE *ReqRefNum;
char *Buffer;
int BufferSize;
int *ActLenRcvd;
int atptimeout;
int *comp;
{
  /*
   * AppleTalk
   *
   */
  if (dsi_session[SessRefNum].sesstype == DSI_SESSION_ATALK)
    return(SPWrtContinue(SessRefNum, ReqRefNum, Buffer,
     BufferSize, ActLenRcvd, atptimeout, comp));

  /*
   * TCP/IP
   *
   */
  if (dsi_session[SessRefNum].sesstype == DSI_SESSION_TCPIP)
    return(dsiTCPIPWrtContinue(SessRefNum, ReqRefNum, Buffer,
     BufferSize, ActLenRcvd, atptimeout, comp));

  return(ParamErr);
}

/*
 * reply to a write request sent to our SSS
 *
 * SessRefNum - session reference number
 * ReqRefNum - client connection details (addr, TID)
 * CmdResult - return result
 * CmdReplyData - return data
 * CmdReplyDataSize - return data size
 * comp - completion flag/error
 *
 */

int
dsiWrtReply(SessRefNum, ReqRefNum, CmdResult,
 CmdReplyData, CmdReplyDataSize, comp)
int SessRefNum;
ASPQE *ReqRefNum;
dword CmdResult;
char *CmdReplyData;
int CmdReplyDataSize;
int *comp;
{
  /*
   * AppleTalk
   *
   */
  if (dsi_session[SessRefNum].sesstype == DSI_SESSION_ATALK)
    return(SPWrtReply(SessRefNum, ReqRefNum, CmdResult,
     CmdReplyData, CmdReplyDataSize, comp));

  /*
   * TCP/IP
   *
   */
  if (dsi_session[SessRefNum].sesstype == DSI_SESSION_TCPIP)
    return(dsiTCPIPWrtReply(SessRefNum, ReqRefNum, CmdResult,
     CmdReplyData, CmdReplyDataSize, comp));

  return(ParamErr);
}

/*
 * Reply to a command request sent to our SSS
 *
 * SessRefNum - session reference number
 * ReqRefNum - client connection details (addr, TID)
 * CmdResult - return result
 * CmdReplyData - return data
 * CmdReplyDataSize - return data size
 * comp - completion flag/error
 *
 */

int
dsiCmdReply(SessRefNum, ReqRefNum, CmdResult,
 CmdReplyData, CmdReplyDataSize, comp)
int SessRefNum;
ASPQE *ReqRefNum;
dword CmdResult;
char *CmdReplyData;
int CmdReplyDataSize;
int *comp;
{
  /*
   * AppleTalk
   *
   */
  if (dsi_session[SessRefNum].sesstype == DSI_SESSION_ATALK)
    return(SPCmdReply(SessRefNum, ReqRefNum, CmdResult,
     CmdReplyData, CmdReplyDataSize, comp));

  /*
   * TCP/IP
   *
   */
  if (dsi_session[SessRefNum].sesstype == DSI_SESSION_TCPIP)
    return(dsiTCPIPCmdReply(SessRefNum, ReqRefNum, CmdResult,
     CmdReplyData, CmdReplyDataSize, comp));

  return(ParamErr);
}

/*
 * send an Attention signal to WSS
 *
 * SessRefNum - session reference number
 * AttentionCode - attention message
 * atpretries - ATP Retries
 * atptimeout - ATP Timeout
 * comp - completion falg/error
 *
 */

int
dsiAttention(SessRefNum, AttentionCode, atpretries, atptimeout, comp)
int SessRefNum;
word AttentionCode;
int atpretries;
int *comp;
{
  /*
   * AppleTalk
   *
   */
  if (dsi_session[SessRefNum].sesstype == DSI_SESSION_ATALK)
    return(SPAttention(SessRefNum, AttentionCode,
     atpretries, atptimeout, comp));

  /*
   * TCP/IP
   *
   */
  if (dsi_session[SessRefNum].sesstype == DSI_SESSION_TCPIP)
    return(dsiTCPIPAttention(SessRefNum, AttentionCode,
     atpretries, atptimeout, comp));

  return(ParamErr);
}

/*
 * return remote address of session client
 *
 */

int
dsiGetNetworkInfo(SessRefNum, addr)
int SessRefNum;
AddrBlock *addr;
{
  /*
   * AppleTalk
   *
   */
  if (dsi_session[SessRefNum].sesstype == DSI_SESSION_ATALK)
    return(SPGetNetworkInfo(SessRefNum, addr));

  /*
   * TCP/IP
   *
   */
  if (dsi_session[SessRefNum].sesstype == DSI_SESSION_TCPIP)
    return(dsiTCPIPGetNetworkInfo(SessRefNum, addr));

  return(ParamErr);
}

/*
 * Get server operating parameters (these numbers are used
 * to malloc() the appropriate amount of buffer space).
 *
 * MaxCmdSize - maximum single packet size
 * QuantumSize - maximum outstanding data
 *
 * For ASP/ATP:
 *	MaxCmdSize = atpMaxData (578)
 *	QuantumSize = atpMaxData*atpMaxNum (578*8)
 *
 * For TCP/IP:
 *	MaxCmdSize = AFP Command Size (1500)
 *	QuantumSize = Data Chunk Size (65536)
 *
 */

int
dsiGetParms(MaxCmdSize, QuantumSize)
int *MaxCmdSize;
int *QuantumSize;
{
  if (asip_enable) {
    *MaxCmdSize = DSI_SRVR_CMD;
    *QuantumSize = DSI_SRVR_MAX;
    return(noErr);
  }

  return(SPGetParms(MaxCmdSize, QuantumSize));
}

/*
 * Close down a SSS
 *
 * SessRefNum - Session reference number
 * atpretries - ATP Retries
 * atptimeout - ATP Timeout
 * comp - completion flag/error
 *
 */

int
dsiCloseSession(SessRefNum, atpretries, atptimeout, comp)
int SessRefNum;
int atpretries;
int atptimeout;
int *comp;
{
  /*
   * AppleTalk
   *
   */
  if (dsi_session[SessRefNum].sesstype == DSI_SESSION_ATALK)
    return(SPCloseSession(SessRefNum, atpretries, atptimeout, comp));

  /*
   * TCP/IP
   *
   */
  if (dsi_session[SessRefNum].sesstype == DSI_SESSION_TCPIP)
    return(dsiTCPIPCloseSession(SessRefNum, atpretries, atptimeout, comp));

  return(ParamErr);
}

/*
 * shutdown session
 * 
 * SessRefNum - session reference number
 *
 */

int
dsiShutdown(SessRefNum)
int SessRefNum;
{
  /*
   * clean up our session data
   *
   */
  if (dsi_session[SessRefNum].aspqe != NULL)
    delete_aspaqe(dsi_session[SessRefNum].aspqe);

  dsi_session[SessRefNum].timeout = 0;
  dsi_session[SessRefNum].aspqe = NULL;
  dsi_session[SessRefNum].sess_id_in = 0;
  dsi_session[SessRefNum].sess_id_out = 0;
  dsi_session[SessRefNum].state = DSI_STATE_HDR;
  dsi_session[SessRefNum].sesstype = DSI_SESSION_ATALK;
  dsi_session[SessRefNum].lenleft = sizeof(struct dsi_hdr);
  dsi_session[SessRefNum].ptr = (char *)&dsi_session[SessRefNum].hdr;

  /*
   * then clean up the ASP stuff
   *
   */
  return(SPShutdown(SessRefNum));
}

#ifdef DEBUG_AFP_CMD
/*
 * return descriptive command name string
 *
 */
char *
dsi_cmd(cmd)
int cmd;
{
  switch (cmd) {
    case DSIGetStatus:
      return("DSIGetStatus");
      break;
    case DSIOpenSession:
      return("DSIOpenSession");
      break;
    case DSICommand:
      return("DSICommand");
      break;
    case DSIWrite:
      return("DSIWrite");
      break;
    case DSIAttention:
      return("DSIAttention");
      break;
    case DSITickle:
      return("DSITickle");
      break;
    case DSICloseSession:
      return("DSICloseSession");
      break;
  }
  return("UNKNOWN");
}
#endif /* DEBUG_AFP_CMD */

/*
 * TCP/IP related routines
 *
 */

/*
 * open and initialise TCP/IP SLS port
 *
 *  "The interface will register the AFP server on a well-known
 *   (static) data stream port. In case of TCP, it will be TCP
 *   port number 548".
 *
 */

private int slsskt = -1;
private struct sockaddr_in lsin;

int
dsiTCPIPInit(SLSEntityIdentifier, ServiceStatusBlock,
 ServiceStatusBlockSize, SLSRefNum)
AddrBlock *SLSEntityIdentifier; /* SLS Net id */
char *ServiceStatusBlock;       /* block with status info */
int ServiceStatusBlockSize;     /* size of status info */
int *SLSRefNum;                 /* sls ref num return place */
{
  int aport, len;
  extern u_int asip_addr;
  extern u_short asip_port;
  private int dsiTCPIPSLSListener();
  struct protoent *t, *getprotobyname();

  /*
   * open a stream socket
   *
   */
  if ((slsskt = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    return(slsskt);

  bzero((char *)&lsin, sizeof(lsin));

  lsin.sin_family = AF_INET;
  lsin.sin_port = htons(asip_port);
  lsin.sin_addr.s_addr = htonl(asip_addr);

  /*
   * want to send data as it becomes available
   *
   */
  len = 1;
  t = getprotobyname("tcp");
  aport = (t == NULL) ? IPPROTO_TCP : t->p_proto;
  setsockopt(slsskt, aport, TCP_NODELAY, (char *)&len, sizeof(int));

  /*
   * bind to ipaddr:port selected by AUFS -B option
   * (defaults to INADDR_ANY:548)
   *
   */
  if (bind(slsskt, (struct sockaddr *)&lsin, sizeof(lsin)) != 0) {
    close(slsskt);
    return(-1);
  }

  /*
   * start listening for connection attempts
   *
   */
  if (listen(slsskt, 5) != 0) {
    close(slsskt);
    return(-1);
  }

  /*
   * register a callback routine to handle SLS connection requests
   *
   */
  fdlistener(slsskt, dsiTCPIPSLSListener, NULL, *SLSRefNum);

  return(noErr);
}

/*
 * fdlistener() callback routine for TCP/IP connection attempts
 *
 * accept() the connection and register a data listener for
 * incoming connection/getstatus packets.
 *
 */

private int
dsiTCPIPSLSListener(fd, none, SLSRefNum)
int fd;
caddr_t none;
int SLSRefNum;
{
  int len, acc;
  int illegal = 0;
  struct sockaddr_in rsin;
  extern u_short asip_port;
  private int dsiTCPIPIllegalIP();
  private int dsiTCPIPSSSListener();

  len = sizeof(struct sockaddr_in);
  if ((acc = accept(fd, (struct sockaddr *)&rsin, &len)) < 0)
    return(acc);

  /*
   * check our IP address filter for
   * a disallowed source IP address
   *
   */
  if (!dsiTCPIPIllegalIP(&rsin))
    fdlistener(acc, dsiTCPIPSSSListener, NULL, SLSRefNum);
  else
    close(acc), illegal = 1;

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    fprintf(dbg,
     "** AppleShareIP connection attempt to port %d from %s:%d (PID %d)\n",
     asip_port, inet_ntoa(rsin.sin_addr), ntohs(rsin.sin_port), getpid());
    if (illegal)
      fprintf(dbg, "** Rejected by IP address filter (%s)\n", dsiTCPIPFilter);
    fprintf(dbg, "\n\n");
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  return(noErr);
}

/*
 * fdlistener() callback routine for incoming DSI requests
 *
 *  "An AFP server expects two command types, that is, DSIOpenSession
 *   or DSIGetStatus after the data stream connection establishment.
 *   DSIOpenSession command confirms the clients commitment to open an
 *   actual DSI session. There is a 1-to-1 mapping between the data
 *   stream connection and a DSI session. DSIGetSTatus command replies
 *   the server status followed by the connection tear down by an AFP
 *   server".
 *
 * This handler is interested only in DSIGetStatus or DSIOpenSession
 * requests. Once the session is open, we unregister the fd with this
 * handler and re-register it with the generic session handler.
 *
 */

private int
dsiTCPIPSSSListener(fd, none, SLSRefNum)
int fd;
caddr_t none;
int SLSRefNum;
{
  int len;
  int optlen;
  ASPSkt *as;
  ASPSSkt *sas;
  int SessRefNum;
  char reply_opt[8];
  struct dsi_hdr hdr;
  char *optptr, *reqst_opt;
  private int dsiTCPIPSessListener();

  /*
   * hopefully there are at least sizeof(hdr) bytes available to read
   * (of course, there may not be, but the extra trouble of keeping a
   * per stream partial header for just DSIGetStatus and DSIOpenSession
   * isn't really worth it).
   *
   */
  if ((len = read(fd, (char *)&hdr, sizeof(hdr))) != sizeof(hdr)) {
    fdunlisten(fd);
    close(fd);
    return(-1);
  }

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    char *dsi_cmd();
    fprintf(dbg, "<< AppleShareIP DSI header (PID %d, session startup):\n",
     getpid());
    fprintf(dbg, "\tFlags: %02x (%s)\n", hdr.dsi_flags,
     (hdr.dsi_flags == DSI_REQ_FLAG) ? "Request" : "REPLY!!");
    fprintf(dbg, "\tCommand: %02x (%s)\n", hdr.dsi_command,
     dsi_cmd(hdr.dsi_command));
    fprintf(dbg, "\tRequestID: %d\n", ntohs(hdr.dsi_requestID));
    fprintf(dbg, "\tErrCode/DataOffset: %d\n", ntohl(hdr.dsi_err_offset));
    fprintf(dbg, "\tDataLength: %d\n", ntohl(hdr.dsi_data_len));
    fprintf(dbg, "\tReserved: %d\n\n\n", ntohl(hdr.dsi_reserved));
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  /*
   * not interested in Replies
   * (should be none)
   *
   */
  if (hdr.dsi_flags != DSI_REQ_FLAG)
    return(noErr);

  /*
   * process the request
   *
   */
  switch (hdr.dsi_command) {
    case DSIGetStatus:
      /* dig out saved server status info for this SLS */
      if ((sas = aspsskt_find_slsrefnum(SLSRefNum)) != NULL) {
        hdr.dsi_flags = DSI_REP_FLAG;
        hdr.dsi_err_offset = htonl(noErr);
        hdr.dsi_data_len = htonl(sas->ssbl);
        hdr.dsi_reserved = htonl(0x00000000);
        /* send server status information */
        dsiTCPIPWrite(fd, &hdr, (char *)sas->ssb, sas->ssbl);
      }
      /* fall through */
    default: /* tear down connection */
      fdunlisten(fd);
      close(fd);
      break;
    case DSIOpenSession:
      /* search for SLS next waiting session */
      if ((as = aspskt_find_active(SLSRefNum)) == NULL) {
        hdr.dsi_flags = DSI_REP_FLAG;
        hdr.dsi_err_offset = htonl(ServerBusy);
        hdr.dsi_data_len = htonl(0x00000000);
        hdr.dsi_reserved = htonl(0x00000000);
        dsiTCPIPWrite(fd, &hdr, NULL, 0);
        fdunlisten(fd);
        close(fd);
        break;
      }
      /* check for incoming OpenSession options */
      if ((optlen = ntohl(hdr.dsi_data_len)) > 0) {
        if ((reqst_opt = (char *)malloc(optlen)) != NULL) {
          optptr = reqst_opt;
          while (optlen > 0) {
            if ((len = read(fd, optptr, optlen)) < 0) {
              fdunlisten(fd);
              close(fd);
              break;
            }
            optlen -= len;
            optptr += len;
          }
          /*
           * one day we might actually care
           *
           dsi_parse_option(optptr);
           *
           */
          free(reqst_opt);
        }
      }
      /* start session */
      as->ss = fd;
      as->state = SP_STARTED;
      SessRefNum = as->SessRefNum;
      /* mark this session as type TCP/IP */
      dsi_session[SessRefNum].sesstype = DSI_SESSION_TCPIP;
      /* set up state for this session */
      dsi_session[SessRefNum].state = DSI_STATE_HDR;
      dsi_session[SessRefNum].lenleft = sizeof(struct dsi_hdr);
      dsi_session[SessRefNum].ptr = (char *)&dsi_session[SessRefNum].hdr;
      dsi_session[SessRefNum].sess_id_in = ntohs(hdr.dsi_requestID)+1;
      dsi_session[SessRefNum].sess_id_out = 0;
      dsi_session[SessRefNum].aspqe = NULL;
      dsi_session[SessRefNum].timeout = 0;
      /* set OpenSession reply option */
      optlen = DSI_OPT_REQLEN+2;
      reply_opt[0] = DSI_OPT_REQQ;
      reply_opt[1] = DSI_OPT_REQLEN;
      reply_opt[2] = (DSI_SRVR_MAX >> 24) & 0xff;
      reply_opt[3] = (DSI_SRVR_MAX >> 16) & 0xff;
      reply_opt[4] = (DSI_SRVR_MAX >>  8) & 0xff;
      reply_opt[5] = (DSI_SRVR_MAX) & 0xff;
      /* setup response header */
      hdr.dsi_flags = DSI_REP_FLAG;
      hdr.dsi_err_offset = htonl(noErr);
      hdr.dsi_data_len = htonl(optlen);
      hdr.dsi_reserved = htonl(0x00000000);
      /* send OpenSession Reply */
      dsiTCPIPWrite(fd, &hdr, reply_opt, optlen);
      /* move fd to session handler */
      fdunlisten(fd);
      fdlistener(fd, dsiTCPIPSessListener, (caddr_t)as, SessRefNum);
      *as->comp = noErr;
      return(noErr);
      break;
  }

  return(noErr);
}

/*
 * data listener for opened sessions
 *
 * At any time the data listener can be in one of four states,
 * waiting until the expected amount of data has arrived, or a
 * reply has been sent, freeing the header for re-use:
 *
 *	DSI_STATE_HDR - reading the 16-byte DSI header
 *	DSI_STATE_AFP - reading the AFP command data
 *	DSI_STATE_DAT - reading the DSIWrite data
 *	DSI_STATE_REP - waiting until reply is sent
 *
 */

private int
dsiTCPIPSessListener(fd, as, SessRefNum)
int fd;
ASPSkt *as;
int SessRefNum;
{
  int len;
  int comp;
  char *ptr;
  ASPQE *aspqe;
  atpProto *ap;
  struct dsi_hdr *hdr;

  /*
   * better have a waiting request
   *
   */
  if ((aspqe = dsi_session[SessRefNum].aspqe) == NULL) {
    logit(0, "Incoming TCP/IP data but no pending request");
    dsiTCPIPCloseSession(SessRefNum, 0, 0, &comp);
    return(-1);
  }

  /*
   * ignore available data until reply is sent
   * (reply uses the sessionID in DSI header) or
   * dsiTCPIPWrtContinue() changes the state to
   * DSI_STATE_DAT
   *
   */
  if (dsi_session[SessRefNum].state == DSI_STATE_REP)
    return(noErr);

  /*
   * read DSI header or data from the
   * tcp/ip stream as it comes in
   *
   */
  len = dsi_session[SessRefNum].lenleft;
  ptr = dsi_session[SessRefNum].ptr;

  if ((len = read(fd, ptr, len)) < 0) {
    logit(0, "TCP/IP read() returned %d (errno %d)", len, errno);
    dsiTCPIPCloseSession(SessRefNum, 0, 0, &comp);
    *aspqe->comp = SessClosed;
    return(len);
  }

  dsi_session[SessRefNum].lenleft -= len;
  dsi_session[SessRefNum].ptr += len;

  if (dsi_session[SessRefNum].lenleft > 0)
    return(noErr);

  /*
   * sanity check
   *
   */
  if (dsi_session[SessRefNum].lenleft < 0) {
    logit(0, "mismatch in expected amount of read data");
    dsiTCPIPCloseSession(SessRefNum, 0, 0, &comp);
    *aspqe->comp = SessClosed;
    return(-1);
  }

  hdr = &dsi_session[SessRefNum].hdr;

  /*
   * finished reading something, deal with it
   *
   */
  switch (dsi_session[SessRefNum].state) {
    case DSI_STATE_HDR:
      /* now have a complete DSI hdr */
      if (ntohl(hdr->dsi_data_len) > 0) {
        /* and AFP hdr to follow */
        ap = &aspqe->abr.proto.atp;
        dsi_session[SessRefNum].ptr = ap->atpDataPtr;
        if (hdr->dsi_command == DSIWrite && ntohl(hdr->dsi_err_offset) != 0)
          dsi_session[SessRefNum].lenleft = ntohl(hdr->dsi_err_offset);
        else
          dsi_session[SessRefNum].lenleft = ntohl(hdr->dsi_data_len);
        dsi_session[SessRefNum].state = DSI_STATE_AFP;
        return(noErr);
        break;
      }
      /* fall through */
    case DSI_STATE_AFP:
      /* have DSI hdr and optional AFP header */
      dsi_session[SessRefNum].ptr = (char *)hdr;
      dsi_session[SessRefNum].lenleft = sizeof(struct dsi_hdr);
      if (hdr->dsi_flags == DSI_REQ_FLAG)
        dsi_session[SessRefNum].state = DSI_STATE_REP;
      else
        dsi_session[SessRefNum].state = DSI_STATE_HDR;
      break;
    case DSI_STATE_DAT:
      /* have all DSIWrite data, reset state, tell client */
      dsi_session[SessRefNum].aspqe = NULL;
      dsi_session[SessRefNum].ptr = (char *)hdr;
      dsi_session[SessRefNum].lenleft = sizeof(struct dsi_hdr);
      dsi_session[SessRefNum].state = DSI_STATE_REP;
      *aspqe->ActRcvdReqLen = ntohl(hdr->dsi_data_len);
      *aspqe->ActRcvdReqLen -= ntohl(hdr->dsi_err_offset);
      *aspqe->comp = noErr;
      delete_aspaqe(aspqe);
      return(noErr);
      break;
    default:
      /* huh ? */
      break;
  }

  /*
   * process DSI header and optional AFP data
   *
   */

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    char *dsi_cmd();
    fprintf(dbg, "<< AppleShareIP DSI header (PID %d, session #%d):\n",
     getpid(), SessRefNum);
    fprintf(dbg, "\tFlags: %02x (%s)\n", hdr->dsi_flags,
     (hdr->dsi_flags == DSI_REQ_FLAG) ? "Request" : "Reply");
    fprintf(dbg, "\tCommand: %02x (%s)\n", hdr->dsi_command,
     dsi_cmd(hdr->dsi_command));
    fprintf(dbg, "\tRequestID: %d\n", ntohs(hdr->dsi_requestID));
    fprintf(dbg, "\tErrCode/DataOffset: %d\n", ntohl(hdr->dsi_err_offset));
    fprintf(dbg, "\tDataLength: %d\n", ntohl(hdr->dsi_data_len));
    fprintf(dbg, "\tReserved: %d\n\n\n", ntohl(hdr->dsi_reserved));
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  /*
   * reset tickle timer
   *
   */
  dsi_session[SessRefNum].timeout = 0;

  /*
   * ignore packet replies, rely on TCP to
   * deliver in-order, that's what it's for.
   *
   */
  if (hdr->dsi_flags == DSI_REP_FLAG)
    return(noErr);

  /*
   * must be request, check if incoming
   * session ID is what we are expecting
   *
   */
  if (ntohs(hdr->dsi_requestID) != dsi_session[SessRefNum].sess_id_in) {
    logit(0, "unexpected incoming TCP/IP session ID");
    *aspqe->comp = ParamErr;
    return(-1);
  }
  dsi_session[SessRefNum].sess_id_in++;

  /*
   * only 3 valid commands to pass on to client
   * handle DSITickle locally, 'cause it's simple
   *
   */
  switch (hdr->dsi_command) {
    case DSICommand:
    case DSIWrite:
     *aspqe->comp = noErr;
      break;
    case DSICloseSession:
     *aspqe->comp = SessClosed;
      break;
    case DSITickle:
      hdr->dsi_flags = DSI_REP_FLAG;
      dsiTCPIPWrite(fd, hdr, NULL, 0);
      dsi_session[SessRefNum].state = DSI_STATE_HDR;
      return(noErr);
      break;
    default:
      logit(0, "unexpected incoming DSI cond (%d)", hdr->dsi_command);
      *aspqe->comp = ParamErr;
      break;
  }

  /*
   * tell the client how much data
   * came in and the command type
   *
   */
  if (hdr->dsi_command == DSIWrite && ntohl(hdr->dsi_err_offset) != 0)
    *aspqe->ActRcvdReqLen = ntohl(hdr->dsi_err_offset);
  else
    *aspqe->ActRcvdReqLen = ntohl(hdr->dsi_data_len);
  *aspqe->SPReqType = hdr->dsi_command;
  *aspqe->ReqRefNum = aspqe;

  /*
   * free previous GetRequest aspqe
   *
   */
  delete_aspaqe(aspqe);
  dsi_session[SessRefNum].aspqe = NULL;

  return(noErr);
}

/*
 * fork and create new process to handle TCP/IP session
 *
 * SessRefNum - session reference number
 * stickle - want server tickle
 * ctickle - want client tickle
 *
 * In the server code (parent) close all but SLS
 * In the child code forget about SLS, just listen
 * for SSS requests
 *
 */

int
dsiTCPIPFork(SessRefNum, stickle, ctickle)
int SessRefNum;
int stickle;
int ctickle;
{
  int i, pid;
  ASPSSkt *sas;
  extern int sqs;
  ASPSkt *as, *bs;
  extern int numasp;
  private void dsiTCPIPTimer();

  if ((as = aspskt_find_sessrefnum(SessRefNum)) == NULL)
    return(-1);

  if (as->state != SP_STARTED)
    return(-1);

  /*
   * make a new process
   *
   */
  if ((pid = fork()) < 0)
    return(pid);

  /*
   * if in parent process:
   *   close SSS
   *
   * if in child process:
   *   close both SLS (AppleTalk and TCP/IP)
   *   start tickle timer for our client session
   *   stop tickle timers for sibling ATALK sessions
   *
   */
  if (pid) {
    /* close SSS */
    if (as->ss != -1) {
      fdunlisten(as->ss);
      close(as->ss);
      as->ss = -1;
    }
    as->state = SP_HALFCLOSED;
  } else { /* in child */
    if (as->type != SP_SERVER)
      return(noErr);
    /* close TCP/IP SLS */
    dsiTCPIPCloseSLS();
    /* kill sibling AT timeouts */
    for (i = 0; i < numasp; i++) {
      if (i != SessRefNum) {
        if ((bs = aspskt_find_sessrefnum(i)) != NULL) {
          if (bs->tickling)
            stopasptickle(bs);
          stop_ttimer(bs);
        }
      }
    }
    /* close AppleTalk SLS */
    if ((sas = aspsskt_find_slsrefnum(as->SLSRefNum)) != NULL)
      ATPCloseSocket(sas->addr.skt);
    /* set a new read quantum */
    sqs = DSI_SRVR_MAX;
    /* start our tickle timer */
    Timeout(dsiTCPIPTimer, numasp, DSI_TIMEOUT);
  }

  return(pid);
}

/*
 * set up to wait for a request on TCP/IP SSS
 *
 * SessRefNum - session reference number
 * ReqBuff - request command buffer
 * ReqBuffSize - request command buffer size
 * ReqRefNum - pointer to a special command block
 * SPReqType - returns command request type
 * ActRcvdReqLen - returns command length
 * comp - completion flag/error
 *
 */

int
dsiTCPIPGetRequest(SessRefNum, ReqBuff, ReqBuffSize,
 ReqRefNum, SPReqType, ActRcvdReqLen, comp)
int SessRefNum;
char *ReqBuff;
int ReqBuffSize;
ASPQE **ReqRefNum;
int *SPReqType;
int *ActRcvdReqLen;
int *comp;
{
  ASPSkt *as;
  atpProto *ap;
  ASPQE *aspqe;

  /*
   * check state of connection
   * and validity of descriptor
   *
   */
  if ((as = aspskt_find_sessrefnum(SessRefNum)) == NULL) {
    *comp = ParamErr;
    return(ParamErr);
  }
  if (as->state != SP_STARTED) {
    *comp = SessClosed;
    return(SessClosed);
  }
  if (as->ss == -1) {
    *comp = ParamErr;
    return(ParamErr);
  }

  /*
   * subsequent GetRequests from TREL_TIMEOUT code
   * on this SessRefNum will never get a callback
   * (because we don't need them for TCP/IP use)
   *
   */
  if (dsi_session[SessRefNum].aspqe != NULL) {
    *comp = 1;
    return(noErr);
  }

  /*
   * save GetRequest args for data arrival
   *
   */
  aspqe = create_aspaqe();

  aspqe->type = tSPGetRequest;
  aspqe->SessRefNum = SessRefNum;
  aspqe->ReqRefNum = ReqRefNum;
  aspqe->SPReqType = SPReqType;
  aspqe->ActRcvdReqLen = ActRcvdReqLen;
  aspqe->comp = comp;

  ap = &aspqe->abr.proto.atp;
  ap->atpReqCount = ReqBuffSize;
  ap->atpDataPtr = ReqBuff;

  dsi_session[SessRefNum].aspqe = aspqe;

  *comp = 1;
  return(noErr);
}

/*
 * arrange to put the 'read' data into Buffer
 *
 * SessRefNum - session reference number
 * ReqRefNum - client connection details (addr, TID)
 * Buffer - final location for data
 * BufferSize - maximum amount of data we can handle
 * ActLenRcvd - actual amount of date received
 * atptimeout - ATP get data timeout
 * comp - completion flag/error
 *
 */

dsiTCPIPWrtContinue(SessRefNum, ReqRefNum, Buffer,
 BufferSize, ActLenRcvd, atptimeout, comp)
int SessRefNum;
ASPQE *ReqRefNum;
char *Buffer;
int BufferSize;
int *ActLenRcvd;
int atptimeout;
int *comp;
{
  ASPSkt *as;
  ASPQE *aspqe;
  struct dsi_hdr *hdr;

  /*
   * sanity checks
   *
   */
  if (BufferSize < 0) {
    *comp = ParamErr;
    return(ParamErr);
  }
  if ((as = aspskt_find_sessrefnum(SessRefNum)) == NULL) {
    *comp = ParamErr;
    return(ParamErr);
  }
  if (as->state != SP_STARTED) {
    *comp = SessClosed;
    return(SessClosed);
  }
  if (as->ss == -1) {
    *comp = ParamErr;
    return(ParamErr);
  }

  /*
   * save WrtContinue args for
   * completion of data arrival
   *
   */
  aspqe = create_aspaqe();

  aspqe->type = tSPWrtContinue;
  aspqe->SessRefNum = SessRefNum;
  aspqe->ActRcvdReqLen = ActLenRcvd;
  aspqe->comp = comp;

  /*
   * reset state & continue data reads
   *
   */
  hdr = &dsi_session[SessRefNum].hdr;
  dsi_session[SessRefNum].aspqe = aspqe;
  dsi_session[SessRefNum].state = DSI_STATE_DAT;
  dsi_session[SessRefNum].lenleft = ntohl(hdr->dsi_data_len);
  dsi_session[SessRefNum].lenleft -= ntohl(hdr->dsi_err_offset);
  dsi_session[SessRefNum].ptr = Buffer;

  *comp = 1;
  return(noErr);
}

/*
 * reply to a write request sent to our TCP/IP SSS
 *
 * SessRefNum - session reference number
 * ReqRefNum - client connection details (addr, TID)
 * CmdResult - return result
 * CmdReplyData - return data
 * CmdReplyDataSize - return data size
 * comp - completion flag/error
 *
 */

int
dsiTCPIPWrtReply(SessRefNum, ReqRefNum, CmdResult,
 CmdReplyData, CmdReplyDataSize, comp)
int SessRefNum;
ASPQE *ReqRefNum;
dword CmdResult;
char *CmdReplyData;
int CmdReplyDataSize;
int *comp;
{
  return(dsiTCPIPReply(DSIWrite, SessRefNum, ReqRefNum,
   CmdResult, CmdReplyData, CmdReplyDataSize, comp));
}

/*
 * Reply to a command request sent to our TCP/IP SSS
 *
 * SessRefNum - session reference number
 * ReqRefNum - client connection details (addr, TID)
 * CmdResult - return result
 * CmdReplyData - return data
 * CmdReplyDataSize - return data size
 * comp - completion flag/error
 *
 */

int
dsiTCPIPCmdReply(SessRefNum, ReqRefNum, CmdResult,
 CmdReplyData, CmdReplyDataSize, comp)
int SessRefNum;
ASPQE *ReqRefNum;
dword CmdResult;
char *CmdReplyData;
int CmdReplyDataSize;
int *comp;
{
  return(dsiTCPIPReply(DSICommand, SessRefNum, ReqRefNum,
   CmdResult, CmdReplyData, CmdReplyDataSize, comp));
}

/*
 * common reply code
 *
 */

int
dsiTCPIPReply(dsiType, SessRefNum, ReqRefNum, CmdResult,
 CmdReplyData, CmdReplyDataSize, comp)
int dsiType;
int SessRefNum;
ASPQE *ReqRefNum;
dword CmdResult;
char *CmdReplyData;
int CmdReplyDataSize;
int *comp;
{
  ASPSkt *as;
  struct dsi_hdr hdr;

  /*
   * some sanity checking
   *
   */
  if (CmdReplyDataSize < 0) {
    *comp = ParamErr;
    return(ParamErr);
  }
  if (CmdReplyDataSize > DSI_SRVR_MAX) {
    *comp = SizeErr;
    return(SizeErr);
  }
  if ((as = aspskt_find_sessrefnum(SessRefNum)) == NULL) {
    *comp = ParamErr;
    return(ParamErr);
  }
  if (as->state != SP_STARTED) {
    *comp = ParamErr;
    return(ParamErr);
  }
  if (as->ss == -1) {
    *comp = ParamErr;
    return(ParamErr);
  }

  /*
   * setup DSI response header
   * (the requestID is already
   * in network byte order)
   *
   */
  hdr.dsi_flags = DSI_REP_FLAG;
  hdr.dsi_command = dsiType;
  hdr.dsi_requestID = dsi_session[SessRefNum].hdr.dsi_requestID;
  hdr.dsi_err_offset = htonl(CmdResult);
  hdr.dsi_data_len = htonl(CmdReplyDataSize);
  hdr.dsi_reserved = htonl(0x00000000);

  /*
   * session hdr can be re-used now
   *
   */
  dsi_session[SessRefNum].state = DSI_STATE_HDR;

  /*
   * send it ...
   *
   */
  if (dsiTCPIPWrite(as->ss, &hdr, CmdReplyData, CmdReplyDataSize) < 0) {
    *comp = ParamErr;
    return(ParamErr);
  }

  *comp = noErr;
  return(noErr);
}

/*
 * setup tickle timeout callback
 *
 */

int
dsiTCPIPTickleUserRoutine(SessRefNum, routine, arg)
int SessRefNum;
int (*routine)();
int arg;
{
  ASPSkt *as;

  if ((as = aspskt_find_sessrefnum(SessRefNum)) == NULL)
    return(ParamErr);

  as->tickle_timeout_user = routine;
  as->ttu_arg = arg;

  return(noErr);
}

/*
 * Close down a TCP/IP Service Socket socket
 *
 * SessRefNum - Session reference number
 * atpretries - ATP Retries
 * atptimeout - ATP Timeout
 * comp - completion flag/error
 *
 */

private struct dsi_hdr shut_hdr;

int
dsiTCPIPCloseSession(SessRefNum, atpretries, atptimeout, comp)
int SessRefNum;
int atpretries;
int atptimeout;
int *comp;
{
  ASPSkt *as;

  if ((as = aspskt_find_sessrefnum(SessRefNum)) == NULL) {
    *comp = ParamErr;
    return(ParamErr);
  }

  switch (as->state) {
    case SP_STARTED:
      break;
    case SP_HALFCLOSED:
      break;
    default:
      as->active = FALSE;	/* aspskt_free(as); */
      return(noErr);
      break;
  }

  /*
   * set up the DSI header
   *
   */
  shut_hdr.dsi_flags = DSI_REQ_FLAG;
  shut_hdr.dsi_command = DSICloseSession;
  shut_hdr.dsi_requestID = htons(dsi_session[SessRefNum].sess_id_out++);
  shut_hdr.dsi_err_offset = htonl(0x00000000);
  shut_hdr.dsi_data_len = htonl(0x00000000);
  shut_hdr.dsi_reserved = htonl(0x00000000);

  /*
   * and send it ...
   *
   */
  if (dsiTCPIPWrite(as->ss, &shut_hdr, NULL, 0) < 0) {
    *comp = ParamErr;
    return(ParamErr);
  }

  as->state = SP_INACTIVE;
  as->active = FALSE;		/* aspskt_free(as); */

  if (as->ss != -1) {
    fdunlisten(as->ss);
    close(as->ss);
    as->ss = -1;
  }

  *comp = noErr;
  return(noErr);
}

/*
 * send a TCP/IP Attention signal to WSS
 *
 * SessRefNum - session reference number
 * AttentionCode - attention message
 * atpretries - ATP Retries
 * atptimeout - ATP Timeout
 * comp - completion falg/error
 *
 */

private struct dsi_hdr attn_hdr;

int
dsiTCPIPAttention(SessRefNum, AttentionCode, atpretries, atptimeout, comp)
int SessRefNum;
word AttentionCode;
int atpretries;
int *comp;
{
  ASPSkt *as;
  char attn[2];

  /*
   * some sanity checking
   *
   */
  if (AttentionCode == 0x00) {
    *comp = ParamErr;
    return(ParamErr);
  }
  if ((as = aspskt_find_sessrefnum(SessRefNum)) == NULL) {
    *comp = ParamErr;
    return(ParamErr);
  }
  if (as->state == SP_STARTING) {
    *comp = ParamErr;
    return(ParamErr);
  }

  /*
   * set up the DSI attention header,
   *
   */
  attn_hdr.dsi_flags = DSI_REQ_FLAG;
  attn_hdr.dsi_command = DSIAttention;
  attn_hdr.dsi_requestID = htons(dsi_session[SessRefNum].sess_id_out++);
  attn_hdr.dsi_err_offset = htonl(0x00000000);
  attn_hdr.dsi_data_len = htonl(sizeof(attn));
  attn_hdr.dsi_reserved = htonl(0x00000000);

  /*
   * the attention field
   *
   */
  attn[0] = AttentionCode >> 8;
  attn[1] = AttentionCode & 0xff;

  /*
   * and send it ...
   *
   */
  if (dsiTCPIPWrite(as->ss, &attn_hdr, attn, sizeof(attn)) < 0) {
    *comp = ParamErr;
    return(ParamErr);
  }

  *comp = noErr;
  return(noErr);
}

/*
 * return peer name of session client
 *
 * (NB: function return value is positive TCP/IP port number,
 * to distinguish this from a real AppleTalk GetNetworkInfo
 * call which returns noErr. The IP address is returned in
 * the four bytes of the AddrBlock)
 *
 */

int
dsiTCPIPGetNetworkInfo(SessRefNum, addr)
int SessRefNum;
AddrBlock *addr;
{
  ASPSkt *as;
  struct sockaddr_in name;
  int len = sizeof(struct sockaddr);

  if ((as = aspskt_find_sessrefnum(SessRefNum)) == NULL)
    return(ParamErr);

  if (as->ss == -1)
    return(ParamErr);

  if (getpeername(as->ss, (struct sockaddr *)&name, &len) != 0)
    return(ParamErr);

  if (name.sin_family != AF_INET)
    return(ParamErr);

  name.sin_addr.s_addr = ntohl(name.sin_addr.s_addr);
  addr->net  = ((name.sin_addr.s_addr & 0xff000000) >> 16);
  addr->net |= ((name.sin_addr.s_addr & 0x00ff0000) >> 16);
  addr->node = ((name.sin_addr.s_addr & 0x0000ff00) >> 8);
  addr->skt  =  (name.sin_addr.s_addr & 0x000000ff);

  return(ntohs(name.sin_port));
}
 
/*
 * write data to client via TCP/IP stream
 *
 * We deliberately don't use non-blocking I/O
 * because the majority of the large data transfers
 * happen in a process dedicated to a single client.
 *
 */

int
dsiTCPIPWrite(fd, hdr, data, len)
int fd;
struct dsi_hdr *hdr;
char *data;
int len;
{
  int cc, cd;

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    char *dsi_cmd();
    fprintf(dbg, ">> AppleShareIP DSI header (PID %d)\n", getpid());
    fprintf(dbg, "\tFlags: %02x (%s)\n", hdr->dsi_flags,
     (hdr->dsi_flags == DSI_REQ_FLAG) ? "Request" : "Reply");
    fprintf(dbg, "\tCommand: %02x (%s)\n", hdr->dsi_command,
     dsi_cmd(hdr->dsi_command));
    fprintf(dbg, "\tRequestID: %d\n", ntohs(hdr->dsi_requestID));
    fprintf(dbg, "\tErrCode/DataOffset: %d\n", ntohl(hdr->dsi_err_offset));
    fprintf(dbg, "\tDataLength: %d\n", ntohl(hdr->dsi_data_len));
    fprintf(dbg, "\tReserved: %d\n", ntohl(hdr->dsi_reserved));
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  /*
   * writev() is more efficient but
   * is less portable than write()
   *
   */
#ifdef HAVE_WRITEV
  { struct iovec iov[2];
    iov[0].iov_base = (caddr_t)hdr;
    iov[0].iov_len = sizeof(struct dsi_hdr);
    iov[1].iov_base = (caddr_t)data;
    iov[1].iov_len = len;
    cc = writev(fd, iov, (data == NULL) ? 1 : 2);
  }
#else  /* HAVE_WRITEV */
  if ((cc = write(fd, (char *)hdr, sizeof(struct dsi_hdr))) >= 0) {
    if (data != NULL) {
      if ((cd = write(fd, data, len)) >= 0)
        cc += cd;
      else
        cc = cd;
    }
  }
#endif /* HAVE_WRITEV */

#ifdef DEBUG_AFP_CMD
  if (dbg != NULL) {
    extern int errno;
    if (cc < 0)
      fprintf(dbg, "** dsiTCPIPWrite(): %d bytes returns %d (errno %d)",
       len+sizeof(struct dsi_hdr), cc, errno);
    fprintf(dbg, "\n\n\n");
    fflush(dbg);
  }
#endif /* DEBUG_AFP_CMD */

  return(cc);
}

/*
 * Tickle Timeout timer
 *
 */

private void
dsiTCPIPTimer(numsess)
int numsess;
{
  int i;
  ASPSkt *as;
  static int inited = 0;
  static struct dsi_hdr tick_hdr;
  void Timeout();

  /*
   * set-up the invariant
   * fields of the tickle hdr
   *
   */
  if (!inited) {
    tick_hdr.dsi_flags = DSI_REQ_FLAG;
    tick_hdr.dsi_command = DSITickle;
    tick_hdr.dsi_err_offset = htonl(0x00000000);
    tick_hdr.dsi_data_len = htonl(0x00000000);
    tick_hdr.dsi_reserved = htonl(0x00000000);
    inited = 1;
  }

  /*
   * check for idle TCP/IP sessions
   *
   */
  for (i = 0; i < numsess; i++) {
    if (dsi_session[i].sesstype == DSI_SESSION_TCPIP) {
      dsi_session[i].timeout += DSI_TIMEOUT;
      if (dsi_session[i].timeout >= ASPCONNECTIONTIMEOUT) {
        if ((as = aspskt_find_sessrefnum(i)) != NULL) {
          if (as->tickle_timeout_user != NULL)
            (*as->tickle_timeout_user)(i, as->ttu_arg);
          else { /* no user routine */
            as->state = SP_INACTIVE;
            as->active = FALSE;		/* aspskt_free(as); */
            dsiShutdown(i);
          }
        }
      } else { /* not timed out, but time to send a tickle ? */
        if ((dsi_session[i].timeout % (ASPTICKLETIMEOUT)) == 0) {
          if ((as = aspskt_find_sessrefnum(i)) != NULL) {
            tick_hdr.dsi_requestID = htons(dsi_session[i].sess_id_out++);
            dsiTCPIPWrite(as->ss, &tick_hdr, NULL, 0);
          }
        }
      }
    }
  }

  Timeout(dsiTCPIPTimer, numsess, DSI_TIMEOUT);

  return;
}

/*
 * close the SLS (called from either the
 * AppleTalk or TCP/IP child processes)
 *
 */

int
dsiTCPIPCloseSLS()
{
  if (slsskt != -1) {
    fdunlisten(slsskt);
    close(slsskt);
    slsskt = -1;
  }

  return(noErr);
}

/*
 * IP address filter
 *
 * compatible with, and stolen from,
 * the ARNS remote access package
 *
 * http://www.cs.mu.OZ.AU/appletalk/atalk.html
 *
 */

private int ipFilters = 0;
private struct ipFilter *ipFilter = NULL;

/*
 * read the specified IP address filter file
 *
 */

private void
dsiTCPIPBuildFilterList()
{
  FILE *fp;
  char line[160];
  char *mask, *addr;
  unsigned long inet_addr();

  ipFilters = 0;

  if (dsiTCPIPFilter != NULL) {
    if (ipFilter == NULL)
      if ((ipFilter =
       (struct ipFilter *)malloc(MAXIPFILTSIZ*MAXIPFILTERS)) == NULL)
        return;
    if ((fp = fopen(dsiTCPIPFilter, "r")) != NULL) {
      while (fgets(line, sizeof(line), fp) != NULL) {
        if (line[0] == '#')
          continue;
        if ((mask = (char *)index(line, '\n')) != NULL)
          *mask = '\0';
        mask = line+1;
        while (*mask != '\0' && isspace(*mask))
          mask++; /* skip spaces */
        addr = mask;
        while (*addr != '\0' && !isspace(*addr))
          addr++; /* skip mask */
        while (*addr != '\0' && isspace(*addr))
          addr++; /* skip spaces */
        if (line[0] == '+' || line[0] == '*' || line[0] == '-') {
          ipFilter[ipFilters].perm = line[0];
          ipFilter[ipFilters].addr = (*addr == '\0') ? 0L : inet_addr(addr);
          ipFilter[ipFilters].mask = (*mask == '\0') ? 0L : inet_addr(mask);
          if (++ipFilters >= MAXIPFILTERS)
            break;
        }
      }
      (void)fclose(fp);
    }
  }

  return;
}

/*
 * check the IP address filter, if any
 *
 */

private int
dsiTCPIPIllegalIP(from)
struct sockaddr_in *from;
{
  int i;
  u_long addr;

  dsiTCPIPBuildFilterList();

  if (ipFilters == 0
   || ipFilter == NULL)
    return(0);

  addr = from->sin_addr.s_addr;

  for (i = 0 ; i < ipFilters ; i++) {
    if (ipFilter[i].addr != 0L) {
      if ((addr & ipFilter[i].mask) == ipFilter[i].addr)
        return(ipFilter[i].perm == '-');
    } else {
      if ((addr & ipFilter[i].mask) == addr)
        return(ipFilter[i].perm == '-');
    }
  }

  return(0);
}
