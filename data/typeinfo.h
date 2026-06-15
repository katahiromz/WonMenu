typedef struct _LARGE_UNICODE_STRING
{
  ULONG Length;
  unsigned __int32 MaximumLength : 31;
  unsigned __int32 bAnsi : 1;
  PWSTR Buffer;
} LARGE_UNICODE_STRING, *PLARGE_UNICODE_STRING;

struct _ETHREAD;

typedef struct _TL
{
  struct _TL *next;
  PVOID pobj;
  PVOID pfnFree;
} TL, *PTL;

typedef struct _UNICODE_STRING
{
  unsigned __int16 Length;
  unsigned __int16 MaximumLength;
  unsigned __int16 *Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

struct _KL;
typedef _KL *PKL;

typedef HANDLE HDESK;

typedef struct _DESKTOPINFO
{
  PVOID pvDesktopBase;
  PVOID pvDesktopLimit;
  struct _WND *spwnd;
  DWORD fsHooks;
  LIST_ENTRY aphkStart[14];
  HWND hTaskManWindow;
  HWND hProgmanWindow;
  HWND hShellWindow;
  struct _WND *spwndShell;
  struct _WND *spwndBkGnd;
  struct _PROCESSINFO *ppiShellProcess;
  union
  {
    UINT Dummy;
    struct
    {
      unsigned __int32 LastInputWasKbd : 1;
    };
  };
  WCHAR szDesktopName[1];
} DESKTOPINFO, *PDESKTOPINFO;

typedef struct tagPOPUPMENU
{
    ULONG fIsMenuBar:1;
    ULONG fHasMenuBar:1;
    ULONG fIsSysMenu:1;
    ULONG fIsTrackPopup:1;
    ULONG fDroppedLeft:1;
    ULONG fHierarchyDropped:1;
    ULONG fRightButton:1;
    ULONG fToggle:1;
    ULONG fSynchronous:1;
    ULONG fFirstClick:1;
    ULONG fDropNextPopup:1;
    ULONG fNoNotify:1;
    ULONG fAboutToHide:1;
    ULONG fShowTimer:1;
    ULONG fHideTimer:1;
    ULONG fDestroyed:1;
    ULONG fDelayedFree:1;
    ULONG fFlushDelayedFree:1;
    ULONG fFreed:1;
    ULONG fInCancel:1;
    ULONG fTrackMouseEvent:1;
    ULONG fSendUninit:1;
    ULONG fRtoL:1;
    // ULONG fDesktopMenu:1;
    ULONG iDropDir:5;
    ULONG fUseMonitorRect:1;
    struct _WND *spwndNotify;
    struct _WND *spwndPopupMenu;
    struct _WND *spwndNextPopup;
    struct _WND *spwndPrevPopup;
    PMENU spmenu;
    PMENU spmenuAlternate;
    struct _WND *spwndActivePopup;
    struct tagPOPUPMENU *ppopupmenuRoot;
    struct tagPOPUPMENU *ppmDelayedFree;
    UINT posSelectedItem;
    UINT posDropped;
} POPUPMENU, *PPOPUPMENU;

typedef struct tagMENUSTATE
{
  PPOPUPMENU  pGlobalPopupMenu;
  struct
  {
  ULONG       fMenuStarted:1;
  ULONG       fIsSysMenu:1;
  ULONG       fInsideMenuLoop:1;
  ULONG       fButtonDown:1;
  ULONG       fInEndMenu:1;
  ULONG       fUnderline:1;
  ULONG       fButtonAlwaysDown:1;
  ULONG       fDragging:1;
  ULONG       fModelessMenu:1;
  ULONG       fInCallHandleMenuMessages:1;
  ULONG       fDragAndDrop:1;
  ULONG       fAutoDismiss:1;
  ULONG       fAboutToAutoDismiss:1;
  ULONG       fIgnoreButtonUp:1;
  ULONG       fMouseOffMenu:1;
  ULONG       fInDoDragDrop:1;
  ULONG       fActiveNoForeground:1;
  ULONG       fNotifyByPos:1;
  ULONG       fSetCapture:1;
  ULONG       iAniDropDir:5;
  };
  POINT       ptMouseLast; 
  INT         mnFocus;
  INT         cmdLast;
  PTHREADINFO ptiMenuStateOwner;
  DWORD       dwLockCount;
  struct tagMENUSTATE* pmnsPrev;
  POINT       ptButtonDown;
  ULONG_PTR   uButtonDownHitArea;
  UINT        uButtonDownIndex;
  INT         vkButtonDown;
  ULONG_PTR   uDraggingHitArea;
  UINT        uDraggingIndex;
  UINT        uDraggingFlags;
  HDC         hdcWndAni;
  DWORD       dwAniStartTime;
  INT         ixAni;
  INT         iyAni;
  INT         cxAni;
  INT         cyAni;
  HBITMAP     hbmAni;
  HDC         hdcAni;
} MENUSTATE, *PMENUSTATE;

struct _THREADINFO;
typedef _THREADINFO *PTHREADINFO;

struct tagQ;
typedef tagQ *PQ;

typedef struct _THREADINFO
{
  struct
  {
    _ETHREAD *pEThread;
    unsigned int RefCount;
    _TL *ptlW32;
    void *pgdiDcattr;
    void *pgdiBrushAttr;
    void *pUMPDObjs;
    void *pUMPDHeap;
    void *pUMPDObj;
    _LIST_ENTRY GdiTmpAllocList;
  };
  PTL ptl;
  PVOID ppi;
  PQ pq;
  PKL spklActive;
  PVOID pcti;
  PVOID rpdesk;
  PDESKTOPINFO pDeskInfo;
  PVOID pClientInfo;
  PVOID Data;
  ULONG TIF_flags;
  PUNICODE_STRING pstrAppName;
  PVOID psmsSent;
  PVOID psmsCurrent;
  PVOID psmsReceiveList;
  LONG timeLast;
  ULONG_PTR idLast;
  union
  {
    INT cQuit;
    INT exitCode;
  };
  HDESK hdesk;
  INT cPaintsReady;
  UINT cTimersReady;
  PMENUSTATE pMenuState;
  union
  {
    PVOID ptdb;
    PVOID pwinsta;
  };
  PVOID psiiList;
  ULONG dwExpWinVer;
  ULONG dwCompatFlags;
  ULONG dwCompatFlags2;
  PQ pqAttach;
  PVOID ptiSibling;
  PVOID pmsd;
  ULONG fsHooks;
  PVOID sphkCurrent;
  PVOID pSBTrack;
  HANDLE hEventQueueClient;
  PVOID pEventQueueServer;
  LIST_ENTRY PtiLink;
  INT iCursorLevel;
  POINT ptLast;
  PWND spwndDefaultIme;
  PVOID spDefaultImc;
  HKL hklPrev;
  INT cEnterCount;
} THREADINFO, *PTHREADINFO;

struct _WINSTATION_OBJECT;

struct _DESKTOP
{
  DWORD dwSessionId;
  PDESKTOPINFO pDeskInfo;
  LIST_ENTRY ListEntry;
  struct _WINSTATION_OBJECT *rpwinstaParent;
  DWORD dwDTFlags;
  DWORD_PTR dwDesktopId;
  PVOID spmenuSys;
  PVOID spmenuDialogSys;
  PVOID spmenuHScroll;
  PVOID spmenuVScroll;
  PWND spwndForeground;
  PWND spwndTray;
  PWND spwndMessage;
  PWND spwndTooltip;
  PVOID hsectionDesktop;
  PVOID pheapDesktop;
  ULONG_PTR ulHeapSize;
  LIST_ENTRY PtiList;
};

typedef struct _THRDESKHEAD
{
  struct
  {
    struct
    {
      HANDLE h;
      DWORD cLockObj;
    };
    struct _THREADINFO *pti;
  };
  struct _DESKTOP *rpdesk;
  PVOID pSelf;
} THRDESKHEAD, *PTHRDESKHEAD;

typedef struct _WND
{
  THRDESKHEAD head;
  DWORD state;
  DWORD state2;
  DWORD ExStyle;
  DWORD style;
  HINSTANCE hModule;
  DWORD fnid;
  struct _WND *spwndNext;
  struct _WND *spwndPrev;
  struct _WND *spwndParent;
  struct _WND *spwndChild;
  struct _WND *spwndOwner;
  RECT rcWindow;
  RECT rcClient;
  WNDPROC lpfnWndProc;
  PCLS pcls;
  HRGN hrgnUpdate;
  PVOID ppropList;
  PVOID pSBInfo;
  PVOID spmenuSys;
  PVOID spmenu;
  HRGN hrgnClip;
  LARGE_UNICODE_STRING strName;
  ULONG ULONG;
  struct _WND *spwndLastActive;
  HIMC hImc;
  LONG_PTR dwUserData;
  PVOID pActCtx;
} WND, *PWND;

typedef struct _MLIST
{
  LPVOID pqmsgRead;
  LPVOID pqmsgWriteLast;
  DWORD cMsgs;
} MLIST, *PMLIST;

typedef struct tagQ
{
  MLIST mlInput;
  PTHREADINFO ptiSysLock;
  ULONG_PTR idSysLock;
  ULONG_PTR idSysPeek;
  PTHREADINFO ptiMouse;
  PTHREADINFO ptiKeyboard;
  PWND spwndCapture;
  PWND spwndFocus;
  PWND spwndActive;
  PWND spwndActivePrev;
  DWORD codeCapture;
  DWORD msgDblClk;
  DWORD xbtnDblClk;
  DWORD timeDblClk;
  HWND hwndDblClk;
  POINT ptDblClk;
  BYTE afKeyRecentDown[32];
  BYTE afKeyState[64];
} Q, *PQ;
