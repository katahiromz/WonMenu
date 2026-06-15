
BOOL __stdcall NtUserTrackPopupMenuEx(HMENU hMenu, UINT fuFlags, int x, int y, HWND hWnd, LPTPMPARAMS lptpm)
{
  PMENU pMenu; // ebx
  BOOL ret; // esi
  PWND pWnd; // eax
  PTHREADINFO pti; // ecx
  TPMPARAMS *lptpm2; // ecx
  LPTPMPARAMS v11; // edx
  char v13[20]; // [esp+Ch] [ebp-44h] BYREF
  TL tl2; // [esp+20h] [ebp-30h] BYREF
  TL tl1; // [esp+2Ch] [ebp-24h] BYREF
  CPPEH_RECORD ms_exc; // [esp+38h] [ebp-18h]

  EnterCrit();
  if ( (fuFlags & 0xFFFF0200) != 0 )
  {
    UserSetLastError(ERROR_INVALID_FLAGS);
    ret = 0;
  }
  else
  {
    pMenu = (PMENU)ValidateHmenu(hMenu);
    ret = 0;
    if ( pMenu )
    {
      pWnd = ValidateHwnd(hWnd);
      if ( pWnd )
      {
        pti = gptiCurrent;
        tl1.next = gptiCurrent->ptl;
        gptiCurrent->ptl = &tl1;
        tl1.pobj = pWnd;
        ++pWnd->head.cLockObj;
        tl2.next = pti->ptl;
        pti->ptl = &tl2;
        tl2.pobj = pMenu;
        ++pMenu->head.head.cLockObj;
        ms_exc.registration.TryLevel = 0;
        lptpm2 = lptpm;
        if ( lptpm )
        {
          v11 = (LPTPMPARAMS)W32UserProbeAddress;
          if ( lptpm < (LPTPMPARAMS)W32UserProbeAddress )
            v11 = lptpm;
          qmemcpy(v13, v11, sizeof(v13));
          lptpm2 = (TPMPARAMS *)v13;
        }
        ms_exc.registration.TryLevel = -1;
        ret = xxxTrackPopupMenuEx(pMenu, fuFlags, x, y, pWnd, lptpm2);
        ThreadUnlock1();
        ThreadUnlock1();
      }
    }
  }
  LeaveCrit();
  return ret;
}

BOOL __stdcall xxxTrackPopupMenuEx(PMENU menu, UINT wFlags, int x, int y, PWND pWnd, LPTPMPARAMS lpTpm)
{
  PMENUSTATE pMenuState; // edi
  unsigned int KeyState; // eax
  ULONG v8; // edx
  PMENUWND Window; // ebx
  struct _WND **p_spwndNotify; // edi
  UINT v11; // ecx
  UINT v12; // eax
  UINT flags; // eax
  PWND v14; // ecx
  bool v15; // zf
  unsigned int v16; // eax
  int v17; // edi
  int v18; // eax
  PTHREADINFO v19; // eax
  int v21; // edi
  int v22; // esi
  PPOPUPMENU pPopupMenu_1; // esi
  PPOPUPMENU pGlobalPopupMenu; // esi
  struct _WND *spwndNotify; // eax
  PMENUWND spwndActivePopup; // esi
  struct _THREADINFO *ptiWnd; // eax
  PTHREADINFO pti; // ebx
  PPOPUPMENU pPopupMenu; // esi
  PTHREADINFO pti3; // [esp-10h] [ebp-58h]
  PTHREADINFO ptiWnd3; // [esp-Ch] [ebp-54h]
  RECT rcExclude; // [esp+8h] [ebp-40h] BYREF
  TL tl2; // [esp+18h] [ebp-30h] BYREF
  TL tl1; // [esp+24h] [ebp-24h] BYREF
  int v36; // [esp+30h] [ebp-18h]
  PTHREADINFO pti2; // [esp+34h] [ebp-14h]
  struct _THREADINFO *ptiWnd2; // [esp+38h] [ebp-10h]
  unsigned __int32 bReturnCmd; // [esp+3Ch] [ebp-Ch]
  int bDown; // [esp+40h] [ebp-8h]
  int v41; // [esp+44h] [ebp-4h]
  int menua; // [esp+50h] [ebp+8h]
  PWND pWnda; // [esp+60h] [ebp+18h]

  v41 = 0;
  if ( lpTpm )
  {
    if ( lpTpm->cbSize != sizeof(TPMPARAMS) )
    {
      UserSetLastError(ERROR_INVALID_PARAMETER);
      return 0;
    }
    rcExclude = lpTpm->rcExclude;
  }
  ptiWnd = pWnd->head.pti;
  pti = gptiCurrent;
  pti2 = gptiCurrent;
  ptiWnd2 = ptiWnd;
  if ( gptiCurrent != ptiWnd )
    return 0;
  pMenuState = gptiCurrent->pMenuState;
  if ( !pMenuState )
    goto LABEL_5;
  if ( (wFlags & TPM_RECURSE) == 0 )
  {
    UserSetLastError(ERROR_POPUP_ALREADY_ACTIVE);
    return 0;
  }
  pGlobalPopupMenu = pMenuState->pGlobalPopupMenu;
  if ( ExitMenuLoop(pMenuState, pMenuState->pGlobalPopupMenu) )
    return 0;
  spwndNotify = pGlobalPopupMenu->spwndNotify;
  if ( !spwndNotify || spwndNotify != pWnd || pMenuState->ptiMenuStateOwner != spwndNotify->head.pti )
    return 0;
  MNAnimate((int *)pMenuState, 0);
  spwndActivePopup = (PMENUWND)pGlobalPopupMenu->spwndActivePopup;
  if ( spwndActivePopup )
    pPopupMenu_1 = spwndActivePopup->ppopupmenu;
  else
    pPopupMenu_1 = 0;
  if ( pPopupMenu_1 && (pPopupMenu_1->flags & 0x2000) != 0 )
  {
    _KillTimer(pPopupMenu_1->spwndPopupMenu, 0xFFFE);
    BYTE1(pPopupMenu_1->flags) &= ~0x20u;
  }
  if ( (pti->pMenuState->flags & 0x100) == 0 )
    BYTE2(pti->pq[1].hwndDblClk) &= ~0x10u;
LABEL_5:
  if ( (wFlags & TPM_RIGHTBUTTON) != 0 )
    KeyState = _GetKeyState(VK_RBUTTON);
  else
    KeyState = _GetKeyState(VK_LBUTTON);
  v8 = menu->fFlags & 0x40000000;
  bDown = (KeyState >> 15) & 1;
  Window = (PMENUWND)xxxCreateWindowEx(
                       129,
                       TPM_LAYOUTRTL,
                       TPM_LAYOUTRTL,
                       0,
                       0x80800000,
                       x,
                       y,
                       100,
                       100,
                       v8 != 0 ? pWnd : 0,
                       0,
                       pWnd->hModule,
                       0,
                       1282,
                       0);
  if ( !Window )
    return 0;
  if ( (pWnd->ExStyle & WS_EX_LAYOUTRTL) != 0 || ((unsigned __int16)wFlags & (unsigned __int16)TPM_LAYOUTRTL) != 0 )
    BYTE2(Window->wnd.ExStyle) |= 0x40u;
  LOBYTE(Window->wnd.state2) &= ~8u;
  tl1.next = gptiCurrent->ptl;
  gptiCurrent->ptl = &tl1;
  tl1.pobj = Window;
  pPopupMenu = Window->ppopupmenu;
  ++Window->wnd.head.cLockObj;
  if ( !pPopupMenu )
    goto LABEL_68;
  BYTE2(pPopupMenu->flags) |= 1u;
  p_spwndNotify = &pPopupMenu->spwndNotify;
  HMAssignmentLock(&pPopupMenu->spwndNotify, pWnd);
  LockPopupMenu(pPopupMenu, &pPopupMenu->spmenu, menu);
  HMAssignmentLock(&pPopupMenu->spwndActivePopup, Window);
  v11 = pPopupMenu->flags;
  v36 = bDown & 1;
  v12 = (32 * (wFlags & 2)) | v11 & 0xFFFFFDBF | (v36 << 9) | 8;
  pPopupMenu->ppopupmenuRoot = pPopupMenu;
  pPopupMenu->flags = v12;
  if ( gpsi->argbSystemUnmatched[9] || (menu->fFlags & 0x20) != 0 )
    pPopupMenu->flags = v12 | 0x10;
  pPopupMenu->flags ^= (pPopupMenu->flags ^ (16 * wFlags)) & 0x800;
  flags = pPopupMenu->flags;
  bReturnCmd = wFlags & TPM_RETURNCMD;
  if ( (wFlags & TPM_RETURNCMD) != 0 )
    pPopupMenu->flags = flags | TPM_RETURNCMD;
  ptiWnd3 = ptiWnd2;
  pti3 = pti2;
  pPopupMenu->flags ^= (pPopupMenu->flags ^ (wFlags >> 7)) & 4;
  v41 = xxxMNAllocMenuState(pti3, ptiWnd3, pPopupMenu);
  if ( !v41 )
  {
LABEL_68:
    LOBYTE(wFlags) = wFlags | TPM_NONOTIFY;
LABEL_69:
    v22 = 0;
    xxxWindowEvent(5, pWnd, 0, 0, 0);
    xxxMNReleaseCapture();
    if ( (wFlags & TPM_NONOTIFY) == 0 )
      xxxSendMessage(pWnd, WM_EXITMENULOOP, (wFlags & 0x200) == 0, 0);
    v21 = v41;
    bReturnCmd = 1;
LABEL_43:
    if ( ThreadUnlock1() && (Window->wnd.state & 0x80000000) == 0 )
      xxxDestroyWindow(&Window->wnd);
    if ( v21 )
      xxxMNEndMenuState(1);
    goto LABEL_48;
  }
  if ( (pPopupMenu->flags & 0x800) == 0 )
    xxxSendMessage(pWnd, WM_ENTERMENULOOP, (pPopupMenu->flags & 4) == 0, 0);
  if ( !xxxMNStartMenu(pPopupMenu, -1) )
    goto LABEL_69;
  if ( (*(_BYTE *)(v41 + 5) & 4) != 0 )
    xxxClientRegisterDragDrop((char)Window->wnd.head.h);
  if ( (pPopupMenu->flags & 0x800) == 0 )
  {
    v14 = *p_spwndNotify;
    v15 = *p_spwndNotify == 0;
    tl2.next = gptiCurrent->ptl;
    gptiCurrent->ptl = &tl2;
    tl2.pobj = v14;
    if ( !v15 )
      ++v14->head.cLockObj;
    xxxSendMessage(*p_spwndNotify, WM_INITMENUPOPUP, (WPARAM)menu->head.head.h, ((pPopupMenu->flags >> 2) & 1) << 16);
    ThreadUnlock1();
    BYTE2(pPopupMenu->flags) |= 0x20u;
  }
  v16 = xxxSendMessage(&Window->wnd, MN_SIZEWINDOW, 1u, 0);
  if ( !v16 )
    goto LABEL_69;
  if ( (dword_BF9B5E24 & 1) != 0 )
  {
    *(_DWORD *)(v41 + 4) |= 0x20u;
    menu->fFlags |= 4u;
  }
  else
  {
    menu->fFlags &= ~4u;
  }
  v17 = (unsigned __int16)v16;
  v18 = HIWORD(v16) + 2 * gpsi->aiSysMet[SM_CYMAXTRACK];
  bDown = v17 + 2 * gpsi->aiSysMet[SM_CXMAXTRACK];
  menua = v18;
  v19 = (PTHREADINFO)_MonitorFromPoint((POINT)__PAIR64__(y, x), 2);
  v15 = (pWnd->ExStyle & 0x400000) == 0;
  pti2 = v19;
  if ( !v15 && (wFlags & 4) == 0 )
    wFlags ^= 8u;
  if ( (wFlags & 8) != 0 )
  {
    x -= bDown;
    pPopupMenu->flags = pPopupMenu->flags & 0xF07FFFFF | 0x1000000;
  }
  else if ( (wFlags & 4) != 0 )
  {
    x += bDown / -2;
  }
  else
  {
    pPopupMenu->flags ^= (pPopupMenu->flags ^ ((((pPopupMenu->flags & 0x10) != 0) + 1) << 23)) & 0xF800000;
  }
  if ( (wFlags & 0x20) != 0 )
  {
    y -= menua;
    HIBYTE(pPopupMenu->flags) |= 4u;
  }
  else if ( (wFlags & 0x10) != 0 )
  {
    y += menua / -2;
  }
  else
  {
    HIBYTE(pPopupMenu->flags) |= 2u;
  }
  if ( (wFlags & 0x3C00) != 0 )
    pPopupMenu->flags = pPopupMenu->flags & 0xF07FFFFF | ((wFlags & 0x3C00) << 13);
  pWnda = (PWND)FindBestPos(x, y, bDown, menua, lpTpm != 0 ? &rcExclude : 0, wFlags, pPopupMenu, pti2);
  if ( (pWnd->ExStyle & 0x400000) != 0 && (pPopupMenu->flags & 0x1800000) != 0 )
    pPopupMenu->flags ^= 0x1800000u;
  if ( (pPopupMenu->flags & 0xF800000) != 0 && (wFlags & 0x4000) == 0 )
    pPopupMenu->flags |= 0x8000000u;
  PlayEventSound(5);
  v21 = v41;
  xxxSetWindowPos(
    &Window->wnd,
    ((*(_DWORD *)(v41 + 4) & 0x100) != 0) - 1,
    (__int16)pWnda,
    SHIWORD(pWnda),
    0,
    0,
    ~(16 * BYTE1(*(_DWORD *)(v41 + 4))) & 0x10 | 0x241);
  xxxWindowEvent(6, Window, -4, 0, 0);
  *(_DWORD *)(v21 + 4) = *(_DWORD *)(v21 + 4) & 0xFFFFFFF7 | (8 * v36);
  v22 = xxxMNLoop(pPopupMenu, (PMENUSTATE)v21, 0, 0);
  if ( (*(_BYTE *)(v21 + 5) & 1) == 0 )
    goto LABEL_43;
  ThreadUnlock1();
LABEL_48:
  if ( bReturnCmd )
    return v22;
  else
    return 1;
}

BOOL __stdcall ExitMenuLoop(PMENUSTATE pMenuState, PPOPUPMENU pPopupMenu)
{
  return (pMenuState->flags & 4) == 0 || (pPopupMenu->flags & 0x8000) != 0;
}

      if ( (pPopupMenu->flags & 8) == 0 )
      {
        spwndActive = pti->MessageQueue->spwndActive;
        if ( spwndActive != pPopupMenu->spwndNotify && (!spwndActive || !IntIsChildWindow(pPopupMenu->spwndNotify, spwndActive)) )
        {
          LOWORD(pMenuState->flags) &= 0xFEFBu;
          xxxEndMenuLoop(pMenuState, pPopupMenu);
          xxxMNReleaseCapture();
          xxxInternalGetMessage(&msg, 0, WM_MOUSEMOVE, WM_MOUSEMOVE, 2, 0);
          return pMenuState->cmdLast;
        }
      }
