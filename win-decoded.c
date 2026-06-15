// @implemented
static PVOID LockWndMenu(PWND pWnd, PMENU *ppMenu, PMENU pNewMenu)
{
    PMENU pOldMenu = *ppMenu;
    if (*ppMenu && pOldMenu->spwndNotify == pWnd)
        HMAssignmentUnlock(&pOldMenu->spwndNotify);
    if (pNewMenu && !pNewMenu->spwndNotify)
        HMAssignmentLock((PVOID *)&pNewMenu->spwndNotify, pWnd);
    return HMAssignmentLock((PVOID *)ppMenu, pNewMenu);
}

// @implemented
static void UnlockWndMenu(PWND pWnd, PMENU *ppMenu)
{
    PWND *ppwndNotify;
    if (*ppMenu)
    {
        ppwndNotify = &(*ppMenu)->spwndNotify;
        if (pWnd == *ppwndNotify)
            HMAssignmentUnlock(ppwndNotify);
        HMAssignmentUnlock(ppMenu);
    }
}

/*
 * MNLookUpItem
 *
 * メニュー内のアイテムをコマンドID（MF_BYCOMMAND）または位置（MF_BYPOSITION）で検索する。
 * サブメニューを持つアイテムに一致した場合は、再帰的にサブメニューを優先して検索し、
 * サブメニュー内で見つからなかった場合に限り、そのアイテム自身を返す。
 *
 * 引数:
 *   pMenu        - 検索対象のメニュー
 *   uIDItem      - 検索するコマンドID、またはアイテム位置インデックス
 *   bByPosition  - TRUE の場合は位置で検索（MF_BYPOSITION）、FALSE の場合はIDで検索（MF_BYCOMMAND）
 *   ppMenu       - 見つかったアイテムが属するメニューを受け取る省略可能なポインタ
 *
 * 戻り値:
 *   見つかったアイテムへのポインタ。見つからなかった場合は NULL。
 */
// @implemented
static PITEM MNLookUpItem(PMENU pMenu, UINT uIDItem, BOOL bByPosition, PMENU *ppMenu)
{
    // ppMenu が指定されていれば、まず NULL で初期化する
    if (ppMenu)
        *ppMenu = NULL;

    // メニューが NULL、またはアイテムが 0 個、または無効な ID の場合は即座に失敗
    if (!pMenu || !pMenu->cItems || uIDItem == (UINT)-1)
        return NULL;

    // --- 位置による検索（MF_BYPOSITION）---
    if (bByPosition)
    {
        if (uIDItem >= pMenu->cItems)
            return NULL;

        PITEM pItem = &pMenu->rgItems[uIDItem];
        if (ppMenu)
            *ppMenu = pMenu;
        return pItem;
    }

    // --- コマンドIDによる検索（MF_BYCOMMAND）---
    // サブメニューを持つアイテムに一致した場合の情報を保持する
    PMENU  pMatchedParentMenu = NULL;   // 一致したサブメニュー持ちアイテムの親メニュー
    PITEM  pMatchedItem       = NULL;   // 一致したサブメニュー持ちアイテム自身

    PITEM rgItems = pMenu->rgItems;
    for (UINT iItem = 0; iItem < pMenu->cItems; iItem++, rgItems++)
    {
        PMENU spSubMenu = rgItems->spSubMenu;

        if (spSubMenu)
        {
            // サブメニューを持つアイテム：
            // まず IDが一致するかを記録しておき（後でサブメニュー内に見つからなければ返す）、
            // 続けてサブメニューを再帰的に検索する
            if (rgItems->wID == uIDItem)
            {
                // サブメニュー内の検索が失敗した場合のフォールバック用に保存
                pMatchedParentMenu = pMenu;
                pMatchedItem       = rgItems;
            }

            PITEM pResult = MNLookUpItem(spSubMenu, uIDItem, FALSE, ppMenu);
            if (pResult)
                return pResult;     // サブメニュー内で見つかった
        }
        else
        {
            // サブメニューを持たない葉アイテム：IDが一致すれば返す
            if (rgItems->wID == uIDItem)
            {
                if (ppMenu)
                    *ppMenu = pMenu;
                return rgItems;
            }
        }
    }

    // ループ内でサブメニュー持ちアイテムへのIDの一致があった場合のフォールバック
    if (!pMatchedItem)
        return NULL;

    if (ppMenu)
        *ppMenu = pMatchedParentMenu;

    return pMatchedItem;
}

// @implemented
static UINT MenuItemState(PMENU pMenu, UINT uIDCheckItem, UINT uCheck, UINT fStateMask, PMENU *ppMenu)
{
    PITEM pItem = MNLookUpItem(pMenu, uIDCheckItem, (uCheck & MF_BYPOSITION), ppMenu);
    if (!pItem)
        return (UINT)-1;

    DWORD fState = pItem->fState;
    DWORD fOldState = (fStateMask & fState);
    pItem->fState = (fState & ~fStateMask) | (uCheck & fStateMask);
    return fOldState;
}

// @implemented
static DWORD _CheckMenuItem(PMENU pMenu, UINT uIDCheckItem, UINT uCheck)
{
    return MenuItemState(pMenu, uIDCheckItem, uCheck, MF_CHECKED, NULL);
}

// @implemented
static BOOL xxxHiliteMenuItem(PWND pWnd, PMENU pMenu, UINT uItemHilite, UINT uHilite)
{
    if (!(uHilite & MF_BYPOSITION))
        uItemHilite = UT_FindTopLevelMenuIndex(pMenu, uItemHilite);
    if (!(pMenu->fFlags & MNF_POPUP))
        xxxMNRecomputeBarIfNeeded(pWnd, pMenu);
    xxxMNInvertItem(0, pMenu, uItemHilite, pWnd, uHilite & MF_HILITE);
    return TRUE;
}

// @implemented
DWORD NTAPI NtUserCheckMenuItem(HMENU hMenu, UINT uIDCheckItem, UINT uCheck)
{
    PMENU pMenu;
    DWORD ret;

    EnterCrit();
    if (HIWORD(uCheck))
    {
        UserSetLastError(ERROR_INVALID_FLAGS);
        ret = -1;
        goto Quit;
    }

    pMenu = ValidateHmenu(hMenu);
    if (!pMenu || (pMenu->fFlags & MNF_DESKTOPMN))
    {
        ret = -1;
        goto Quit;
    }

    ret = _CheckMenuItem(pMenu, uIDCheckItem, uCheck);

Quit:
    LeaveCrit();
    return ret;
}

// @unimplemented
static BOOL xxxRemoveDeleteMenuHelper(PMENU pMenu, UINT uPosition, UINT uFlags, UINT a4)
{
    // FIXME
}

// @implemented
static BOOL xxxRemoveMenu(PMENU pMenu, UINT uPosition, UINT uFlags)
{
    return xxxRemoveDeleteMenuHelper(pMenu, uPosition, uFlags, 0);
}

// @implemented
BOOL NTAPI NtUserRemoveMenu(HMENU hMenu, UINT uPosition, UINT uFlags)
{
    BOOL ret = FALSE;
    PMENU pMenu;
    TL tl;

    EnterCrit();

    if (HIWORD(uFlags))
    {
        UserSetLastError(ERROR_INVALID_FLAGS);
        goto Quit;
    }

    pMenu = ValidateHmenu(hMenu);
    if (!pMenu)
        goto Quit;

    if ((pMenu->fFlags & MNF_DESKTOPMN) || (pMenu->fFlags & MNF_SYSMENU))
        goto Quit;

    tl.next = gptiCurrent->ptl;
    gptiCurrent->ptl = &tl;
    tl.pobj = pMenu;
    ++pMenu->head.head.cLockObj;

    ret = xxxRemoveMenu(pMenu, uPosition, uFlags);
    ThreadUnlock1();

Quit:
    LeaveCrit();
    return ret;
}

// @implemented
static BOOL xxxSetSystemMenu(PWND pWnd, PMENU pMenu)
{
    PMENU spmenuSys;

    if (!(pWnd->style & WS_SYSMENU))
    {
        UserSetLastError(ERROR_NO_SYSTEM_MENU);
        return FALSE;
    }

    spmenuSys = pWnd->spmenuSys;
    if (LockWndMenu(pWnd, &pWnd->spmenuSys, pMenu))
        _DestroyMenu(spmenuSys);

    MNPositionSysMenu(pWnd, pMenu);
    return TRUE;
}

// @implemented
BOOL NTAPI NtUserSetSystemMenu(HWND hWnd, HMENU hMenu)
{
    PWND pWnd;
    PTHREADINFO pti;
    PMENU pMenu;
    BOOL ret = FALSE;
    TL tl1, tl2;

    EnterCrit();
    pWnd = ValidateHwnd(hWnd);
    if (!pWnd)
    {
        ret = FALSE;
        goto Quit;
    }

    pti = gptiCurrent;
    tl1.next = gptiCurrent->ptl;
    gptiCurrent->ptl = &tl1;
    tl1.pobj = pWnd;
    ++pWnd->head.cLockObj;

    pMenu = ValidateHmenu(hMenu);
    if (pMenu)
    {
        tl2.next = pti->ptl;
        pti->ptl = &tl2;
        tl2.pobj = pMenu;
        ++pMenu->head.head.cLockObj;
        ret = xxxSetSystemMenu(pWnd, pMenu);
        ThreadUnlock1();
    }

    ThreadUnlock1();

Quit:
    LeaveCrit();
    return ret;
}

// @implemented
static PWND GetMenuStateWindow(PMENUSTATE pMenuState)
{
    if (!pMenuState)
        return NULL;

    PPOPUPMENU pGlobalPopupMenu = pMenuState->pGlobalPopupMenu;
    if (pMenuState->pGlobalPopupMenu->fInsideMenuLoop)
        return pGlobalPopupMenu->spwndPopupMenu;

    PWND pwndNextPopup = pGlobalPopupMenu->spwndNextPopup;
    if (pwndNextPopup)
        return pwndNextPopup;

    return pGlobalPopupMenu->spwndActivePopup;
}

// @implemented
BOOL NTAPI NtUserEndMenu(VOID)
{
    PWND pWnd;

    EnterCrit();
    if (gptiCurrent->pMenuState)
    {
        pWnd = GetMenuStateWindow(gptiCurrent->pMenuState);
        if (pWnd)
            _PostMessage(pWnd, 0x1F3, 0, 0);
        else
            gptiCurrent->pMenuState->fInsideMenuLoop = FALSE;
    }
    LeaveCrit();
    return TRUE;
}

/*
 * NtUserGetMenuIndex
 *
 * メニュー内で指定のサブメニューを持つアイテムの位置インデックスを返す。
 *
 * 引数:
 *   hMenu    - 検索対象の親メニューのハンドル
 *   hSubMenu - 探すサブメニューのハンドル
 *
 * 戻り値:
 *   見つかったアイテムの位置インデックス（0始まり）。
 *   いずれかのハンドルが無効な場合は 0、見つからなかった場合は (UINT)-1。
 */
// @implemented
UINT NTAPI NtUserGetMenuIndex(HMENU hMenu, HMENU hSubMenu)
{
    UINT ret = (UINT)-1;
    PMENU pMenu, pSubMenu;

    EnterSharedCrit();

    pMenu = ValidateHmenu(hMenu);
    pSubMenu = pMenu ? ValidateHmenu(hSubMenu) : NULL;

    if (!pMenu || !pSubMenu)
    {
        ret = 0;
        goto Cleanup;
    }

    for (UINT iItem = 0; iItem < pMenu->cItems; iItem++)
    {
        if (pMenu->rgItems[iItem].spSubMenu == pSubMenu)
        {
            ret = iItem;
            break;
        }
    }

Cleanup:
    LeaveCrit();
    return ret;
}

// @implemented
HMENU NTAPI NtUserGetSystemMenu(HWND hWnd, BOOL bRevert)
{
    PWND pWnd;
    PMENU pSysMenu;
    HMENU hSysMenu;
    TL tl;

    EnterCrit();

    pWnd = ValidateHwnd(hWnd);
    if (pWnd)
    {
        tl.next = gptiCurrent->ptl;
        gptiCurrent->ptl = &tl;
        tl.pobj = pWnd;
        ++pWnd->head.cLockObj;

        pSysMenu = xxxGetSystemMenu(pWnd, bRevert);
        if (pSysMenu)
            hSysMenu = (HMENU)pSysMenu->head.head.h;
        else
            hSysMenu = NULL;

        ThreadUnlock1();
    }
    else
    {
        hSysMenu = NULL;
    }

    LeaveCrit();
    return hSysMenu;
}
