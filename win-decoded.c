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

// @implemented
static void LockPopupMenu(PPOPUPMENU pPopupMenu, PMENU *ppMenu, PMENU pMenu)
{
    UnlockPopupMenuWindow(*ppMenu, pPopupMenu->spwndNotify);
    if (pMenu)
        HMAssignmentLock((PVOID *)&pMenu->spwndNotify, pPopupMenu->spwndNotify);
    HMAssignmentLock((PVOID *)ppMenu, pMenu);
}

// @implemented
static BOOL ExitMenuLoop(PMENUSTATE pMenuState, PPOPUPMENU pPopupMenu)
{
    return !pMenuState->fInsideMenuLoop || pPopupMenu->fDelayedFree;
}

// @implemented
static void MakeMenuRtoL(PMENU pMenu, BOOL bRtoL)
{
    if (bRtoL)
        pMenu->fFlags |= MNF_RTOL;
    else
        pMenu->fFlags &= ~MNF_RTOL;

    for (INT iItem = 0; iItem < (INT)pMenu->cItems; ++iItem)
    {
        PITEM pItem = &pMenu->rgItems[iItem];

        if (bRtoL)
            pItem->fType |= (MFT_RIGHTORDER | MFT_RIGHTJUSTIFY);
        else
            pItem->fType &= ~(MFT_RIGHTORDER | MFT_RIGHTJUSTIFY);

        if (pItem->spSubMenu)
            MakeMenuRtoL(pItem->spSubMenu, bRtoL);
    }
}

/*
 * ItemContainingSubMenu
 *
 * pMenu の直下のアイテムを末尾から先頭へ逆順に走査し、
 * pSubMenu に対応するアイテムのインデックスを返す。
 *
 * 一致の判定は2段階：
 *   1. spSubMenu == NULL かつ wID == (UINT)pSubMenu  → 葉アイテムのIDが一致
 *   2. spSubMenu == pSubMenu                         → サブメニューが直接一致
 *   3. spSubMenu != NULL の場合は再帰的に子を検索し、見つかればそのインデックスを返す
 *
 * 引数:
 *   pMenu    - 検索対象の親メニュー
 *   pSubMenu - 探すサブメニュー（またはコマンドIDとして解釈される値）
 *
 * 戻り値:
 *   見つかったアイテムのインデックス（0始まり）。
 *   見つからなかった場合は -1。
 */
static INT ItemContainingSubMenu(PMENU pMenu, PMENU pSubMenu)
{
    // 末尾アイテムのインデックスから開始（逆順走査）
    INT i = pMenu->cItems - 1;

    if (i == -1)
        return -1;

    if (i < 0)
        return i;   // cItems == 0 の場合（実質 -1）

    PITEM pItem = &pMenu->rgItems[i];
    for (; i >= 0; --i, --pItem)
    {
        PMENU spSubMenu = pItem->spSubMenu;

        if (spSubMenu == NULL)
        {
            // 葉アイテム：wID を pSubMenu のアドレス値と比較
            if ((PMENU)(UINT_PTR)pItem->wID == pSubMenu)
                return i;
        }
        else
        {
            // サブメニューが直接一致
            if (spSubMenu == pSubMenu)
                return i;

            // サブメニュー内を再帰的に検索
            if (ItemContainingSubMenu(spSubMenu, pSubMenu) != -1)
                return i;
        }
    }

    return -1;
}

/*
 * UT_FindTopLevelMenuIndex
 *
 * トップレベルメニュー内で、指定アイテムを含むエントリの位置インデックスを返す。
 *
 * アイテムがトップレベルに直属する場合は uItem（コマンドID）を、
 * サブメニュー内に存在する場合はその親メニュー（ppMenu）を
 * ItemContainingSubMenu に渡して位置を求める。
 *
 * 引数:
 *   pMenu - 検索対象のトップレベルメニュー
 *   uItem - 検索するアイテムのコマンドID（MF_BYCOMMAND のみ）
 *
 * 戻り値:
 *   見つかったトップレベルエントリの位置インデックス（0始まり）。
 *   アイテムが見つからない、またはサブメニューを持つアイテムの場合は (UINT)-1。
 */
static UINT UT_FindTopLevelMenuIndex(PMENU pMenu, UINT uItem)
{
    PMENU ppMenu = NULL;
    PITEM pItem  = MNLookUpItem(pMenu, uItem, FALSE, &ppMenu);

    // アイテムが見つからない、またはサブメニューを持つアイテムは対象外
    if (!pItem || pItem->spSubMenu)
        return (UINT)-1;

    // ppMenu == pMenu：アイテムはトップレベルに直属
    //   → uItem（コマンドID）を値として ItemContainingSubMenu に渡す
    // ppMenu != pMenu：アイテムはサブメニュー内に存在
    //   → ppMenu（親メニューのポインタ）を ItemContainingSubMenu に渡す
    PMENU key = (ppMenu == pMenu) ? (PMENU)(UINT_PTR)uItem : ppMenu;

    return (UINT)ItemContainingSubMenu(pMenu, key);
}

// @implemented
static PPOPUPMENU MNGetPopupFromMenu(PMENU pMenu, PMENUSTATE *ppMenuState)
{
    PPOPUPMENU pNode;
    PMENUSTATE pMenuState;
    PMENUWND pNextPopup;
    PWND pwndNotify;

    pwndNotify = pMenu->spwndNotify;
    if (!pwndNotify)
        return NULL;

    pMenuState = pwndNotify->head.pti->pMenuState;
    if (!pMenuState || !pMenuState->fIsSysMenu)
        return NULL;

    if (ppMenuState)
        *ppMenuState = pMenuState;

    for (pNode = pMenuState->pGlobalPopupMenu; pNode; pNode = pNextPopup->ppopupmenu)
    {
        if (pNode->spmenu == pMenu)
        {
            if (pNode->fIsMenuBar)
                return NULL;

            MNAnimate(pMenuState, 0);
            return pNode;
        }
        pNextPopup = (PMENUWND)pNode->spwndNextPopup;
        if (!pNextPopup)
            return NULL;
    }
    return NULL;
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
static BOOL xxxRemoveDeleteMenuHelper(PMENU pMenu, UINT uPosition, UINT uFlags, BOOL bDelete)
{
    // FIXME
}

// @implemented
static BOOL xxxRemoveMenu(PMENU pMenu, UINT uPosition, UINT uFlags)
{
    return xxxRemoveDeleteMenuHelper(pMenu, uPosition, uFlags, FALSE);
}

// @implemented
static BOOL xxxDeleteMenu(PMENU pMenu, UINT uPosition, UINT uFlags)
{
    return xxxRemoveDeleteMenuHelper(pMenu, uPosition, uFlags, TRUE);
}

// @implemented
BOOL NTAPI NtUserDeleteMenu(HMENU hMenu, UINT uPosition, UINT uFlags)
{
    PMENU pMenu;
    BOOL ret = FALSE;
    TL tl;

    EnterCrit();
    if (HIWORD(uFlags))
    {
        UserSetLastError(ERROR_INVALID_FLAGS);
        goto Cleanup;
    }

    pMenu = ValidateHmenu(hMenu);
    if (!pMenu || (pMenu->fFlags & MNF_DESKTOPMN) || (pMenu->fFlags & MNF_SYSMENU))
        goto Cleanup;

    tl.next = gptiCurrent->ptl;
    gptiCurrent->ptl = &tl;
    tl.pobj = pMenu;
    ++pMenu->head.head.cLockObj;

    ret = xxxDeleteMenu(pMenu, uPosition, uFlags);
    ThreadUnlock1();

Cleanup:
    LeaveCrit();
    return ret;
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

// @implemented
static BOOL _SetMenuContextHelpId(PMENU pMenu, DWORD dwContextHelpId)
{
    pMenu->dwContextHelpId = dwContextHelpId;
    return TRUE;
}

// @implemented
BOOL NTAPI NtUserSetMenuContextHelpId(HMENU hMenu, DWORD dwContextHelpId)
{
    PMENU pMenu;
    BOOL ret;

    EnterCrit();
    pMenu = ValidateHmenu(hMenu);
    ret = pMenu && !(pMenu->fFlags & MNF_DESKTOPMN) && _SetMenuContextHelpId(pMenu, dwContextHelpId);
    LeaveCrit();
    return ret;
}

/*
 * _SetMenuDefaultItem
 *
 * メニューのデフォルトアイテムを設定する。
 * uItem == (UINT)-1 の場合はデフォルトアイテムをクリアするだけで終わる。
 * それ以外の場合は、現在のデフォルトアイテムを解除し、指定アイテムを新たにデフォルトに設定する。
 *
 * 引数:
 *   pMenu  - 操作対象のメニュー
 *   uItem  - デフォルトに設定するアイテムのIDまたは位置。(UINT)-1 でクリアのみ。
 *   fByPos - TRUE なら位置で検索（MF_BYPOSITION）、FALSE ならIDで検索（MF_BYCOMMAND）
 *
 * 戻り値:
 *   成功時 TRUE、アイテムが見つからない・無効・セパレータの場合は FALSE。
 */
// @implemented
static BOOL _SetMenuDefaultItem(PMENU pMenu, UINT uItem, UINT fByPos)
{
    PMENU  pMenuOrig = pMenu;
    PITEM  pItemNew  = NULL;   // 新たにデフォルトに設定するアイテム（NULL はクリアのみ）

    if (uItem != (UINT)-1)
    {
        // 指定アイテムを検索
        // MNLookUpItem は見つかったアイテムの親メニューを pMenu に書き戻す
        pItemNew = MNLookUpItem(pMenu, uItem, fByPos, &pMenu);

        // 見つからない、サブメニュー内のアイテム（直属でない）、またはセパレータは不可
        if (!pItemNew || pMenu != pMenuOrig || (pItemNew->fType & MF_SEPARATOR))
            return FALSE;
    }

    // --- 既存のデフォルトアイテムをすべて解除 ---
    // 新たに設定するアイテム自身はスキップする（二重処理を避けるため）
    PITEM pItem = pMenuOrig->rgItems;
    for (UINT i = pMenuOrig->cItems; i > 0; --i, ++pItem)
    {
        if ((pItem->fState & MF_DEFAULT) && pItem != pItemNew)
        {
            pItem->ulWidth = 0;
            pItem->fState &= ~MF_DEFAULT;
            pItem->ulX    = 0x7FFFFFFF;   // 描画位置を無効値にリセット
        }
    }

    // --- 新たなデフォルトアイテムを設定 ---
    if (uItem != (UINT)-1 && !(pItemNew->fState & MF_DEFAULT))
    {
        pItemNew->ulWidth = 0;
        pItemNew->fState |= MF_DEFAULT;
        pItemNew->ulX    = 0x7FFFFFFF;   // 描画位置を無効値にリセット
    }

    return TRUE;
}

// @implemented
BOOL NTAPI NtUserSetMenuDefaultItem(HMENU hMenu, UINT uItem, UINT fByPos)
{
    PMENU pMenu;
    BOOL ret;

    EnterCrit();
    pMenu = ValidateHmenu(hMenu);
    ret = pMenu && !(pMenu->fFlags & MNF_DESKTOPMN) && _SetMenuDefaultItem(pMenu, uItem, fByPos);
    LeaveCrit();
    return ret;
}

// @implemented
static BOOL _SetMenuFlagRtoL(PMENU pMenu)
{
    pMenu->fFlags |= MNF_RTOL;
    return TRUE;
}

// @implemented
BOOL NTAPI NtUserSetMenuFlagRtoL(HMENU hMenu)
{
    PMENU pMenu;
    BOOL ret;

    EnterCrit();
    pMenu = ValidateHmenu(hMenu);
    if (pMenu)
        ret = _SetMenuFlagRtoL(pMenu);
    else
        ret = FALSE;
  LeaveCrit();
  return ret;
}

static INT xxxPaintMenuBar(PWND pWnd, HDC hDC, ULONG left, ULONG rightBorder, ULONG top, BOOL bActive)
{
    PMENU spmenu;
    HBRUSH hbrBack, hOldBrush;
    INT cyMenu;
    TL tl;

    spmenu = pWnd->spmenu;
    if (!spmenu)
        return 0;

    ThreadLockMenuNoModify(spmenu, &tl);

    if (bActive)
        spmenu->fFlags &= ~MNF_INACTIVE;
    else
        spmenu->fFlags |= MNF_INACTIVE;

    if (pWnd != spmenu->spwndNotify || !spmenu->cxMenu || !spmenu->cyMenu)
        xxxMenuBarCompute(spmenu, pWnd, top, left, pWnd->rcWindow.right - pWnd->rcWindow.left - left - rightBorder);
    hbrBack = spmenu->hbrBack;
    if (!hbrBack)
        hbrBack = (HBRUSH)gpsi[1].mpFnidPfn[25]; // FIXME

    hOldBrush = GreSelectBrush(hDC, hbrBack);
    NtGdiPatBlt(hDC, left, top, spmenu->cxMenu, spmenu->cyMenu, PATCOPY);
    xxxMenuDraw(hDC, spmenu);
    GreSelectBrush(hDC, hOldBrush);

    cyMenu = spmenu->cyMenu;
    ThreadUnlockMenuNoModify(&tl);
    return cyMenu;
}

// @implemented
DWORD NTAPI NtUserPaintMenuBar(HWND hWnd, HDC hDC, ULONG leftBorder, ULONG rightBorder, ULONG top, BOOL bActive)
{
    PWND pWnd;
    DWORD ret = FALSE;
    TL tl;

    EnterCrit();
    pWnd = ValidateHwnd(hWnd);
    if (!pWnd)
        goto Cleanup;

    tl.next = gptiCurrent->ptl;
    gptiCurrent->ptl = &tl;
    tl.pobj = pWnd;
    ++pWnd->head.cLockObj;
    if ((pWnd->style & 0xC000) == 0x4000)
    {
        UserSetLastError(ERROR_INVALID_PARAMETER);
        goto Cleanup;
    }

    if ((bActive & ~TRUE))
    {
        UserSetLastError(ERROR_INVALID_FLAGS);
        goto Cleanup;
    }

    if ((LONG)leftBorder >= 0 && (LONG)rightBorder >= 0 && (LONG)top >= 0)
    {
        ret = xxxPaintMenuBar(pWnd, hDC, leftBorder, rightBorder, top, bActive);
        goto Cleanup;
    }

Cleanup:
    ThreadUnlock1();
    LeaveCrit();
    return ret;
}

// @implemented
static DWORD xxxCalcMenuBar(PWND pWnd, ULONG leftBorder, ULONG rightBorder, ULONG top, PRECTL prc)
{
    PMENU spmenu;
    DWORD cyMenu;
    TL tl;

    spmenu = pWnd->spmenu;
    if ((pWnd->style & 0xC000) == 0x4000 || !spmenu)
        return 0;

    ThreadLockMenuNoModify(spmenu, &tl);
    xxxMenuBarCompute(spmenu, pWnd, top, leftBorder, prc->right - prc->left - leftBorder - rightBorder);
    cyMenu = spmenu->cyMenu;
    ThreadUnlockMenuNoModify(&tl);
    return cyMenu;
}

// @implemented
static UINT xxxEnableMenuItem(PMENU pMenu, UINT uIDEnableItem, UINT uEnable)
{
    UINT fOldState;
    PPOPUPMENU pPopupMenu;
    PWND spwndNotify;
    TL tl;

    fOldState = MenuItemState(pMenu, uIDEnableItem, uEnable, MF_ENABLED | MF_GRAYED | MF_DISABLED, &pMenu);

    if (pMenu->fFlags & MNF_SYSSUBMENU)
    {
        spwndNotify = pMenu->spwndNotify;
        if (spwndNotify && uEnable != fOldState)
        {
            switch (uIDEnableItem)
            {
                case SC_SIZE:
                case SC_MOVE:
                case SC_ICON:
                case SC_MAXIMIZE:
                case SC_CLOSE:
                case SC_RESTORE:
                {
                    tl.next = gptiCurrent->ptl;
                    gptiCurrent->ptl = &tl;
                    tl.pobj = spwndNotify;
                    ++spwndNotify->head.cLockObj;

                    xxxRedrawTitle(pMenu->spwndNotify, 0x1000);
                    ThreadUnlock1();
                    break;
                }
            }
        }
    }

    if (pMenu)
    {
        pPopupMenu = MNGetPopupFromMenu(pMenu, 0);
        if (pPopupMenu)
            xxxMNUpdateShownMenu(pPopupMenu, 0, 1);
    }

    return fOldState;
}

// @implemented
UINT NTAPI NtUserEnableMenuItem(HMENU hMenu, UINT uIDEnableItem, UINT uEnable)
{
    PMENU pMenu;
    UINT ret = 1;
    TL tl;

    EnterCrit();
    if (HIWORD(uEnable))
    {
        UserSetLastError(ERROR_INVALID_FLAGS);
        goto Cleanup;
    }

    pMenu = ValidateHmenu(hMenu);
    if (!pMenu || (pMenu->fFlags & MNF_DESKTOPMN))
        goto Cleanup;

    tl.next = gptiCurrent->ptl;
    gptiCurrent->ptl = &tl;
    tl.pobj = pMenu;
    ++pMenu->head.head.cLockObj;

    ret = xxxEnableMenuItem(pMenu, uIDEnableItem, uEnable);
    ThreadUnlock1();

Cleanup:
    LeaveCrit();
    return ret;
}

// @implemented
static INT xxxMenuItemFromPoint(PWND pWnd, PMENU pMenu, DWORD X, DWORD Y)
{
    PWND pMenuWnd = GetMenuPwnd(pWnd, pMenu);
    if (!pMenuWnd)
        return -1;
    if (!(pMenu->fFlags & MNF_POPUP))
        xxxMNRecomputeBarIfNeeded(pMenuWnd, pMenu);
    return MNItemHitTest(pMenu, pMenuWnd2, X, Y);
}

// @implemented
INT NTAPI NtUserMenuItemFromPoint(HWND hWnd, HMENU hMenu, DWORD X, DWORD Y)
{
    PWND pWnd;
    INT ret;
    PTHREADINFO pti;
    PMENU pMenu;
    TL tl1, tl2;

    EnterCrit();
    if (hWnd)
    {
        pWnd = ValidateHwnd(hWnd);
        if (!pWnd)
        {
            ret = -1;
            goto Cleanup;
        }
    }
    else
    {
        pWnd = NULL;
    }

    pti = gptiCurrent;
    tl1.next = gptiCurrent->ptl;
    gptiCurrent->ptl = &tl1;
    tl1.pobj = pWnd;
    if (pWnd)
        ++pWnd->head.cLockObj;

    pMenu = ValidateHmenu(hMenu);
    if (pMenu)
    {
        tl2.next = pti->ptl;
        pti->ptl = &tl2;
        tl2.pobj = pMenu;
        ++pMenu->head.head.cLockObj;

        ret = xxxMenuItemFromPoint(pWnd, pMenu, X, Y);
        ThreadUnlock1();
    }
    else
    {
        ret = -1;
    }

    ThreadUnlock1();
Cleanup:
    LeaveCrit();
    return ret;
}

// @implemented
BOOL NTAPI NtUserGetMenuItemRect(HWND hWnd, HMENU hMenu, UINT uItem, PRECTL lprcItem)
{
    PWND pWnd;
    BOOL ret;
    PTHREADINFO pti;
    PMENU pMenu;
    RECTL rc;
    TL tl1, tl2;

    EnterCrit();
    if (hWnd)
    {
        pWnd = ValidateHwnd(hWnd);
        if (!pWnd)
        {
            ret = FALSE;
            goto Cleanup;
        }
    }
    else
    {
        pWnd = NULL;
    }

    pti = gptiCurrent;
    tl2.next = gptiCurrent->ptl;
    gptiCurrent->ptl = &tl2;
    tl2.pobj = pWnd;
    if (pWnd)
        ++pWnd->head.cLockObj;

    pMenu = ValidateHmenu(hMenu);
    if (pMenu)
    {
        tl1.next = pti->ptl;
        pti->ptl = &tl1;
        tl1.pobj = pMenu;
        ++pMenu->head.head.cLockObj;

        ret = xxxGetMenuItemRect(pWnd, pMenu, uItem, &rc);
        __try
        {
            *lprcItem = rc;
        } __except(...)
        {
            ;
        }
        ThreadUnlock1();
    }
    else
    {
        ret = FALSE;
    }

    ThreadUnlock1();
Cleanup:
    LeaveCrit();
    return ret;
}

// @implemented
static PWND GetMenuPwnd(PWND pWnd, PMENU pMenu)
{
    PPOPUPMENU pPopupMenu;

    if ((pMenu->fFlags & MNF_POPUP) && (!pWnd || (pWnd->fnid & 0x3FFF) != FNID_MENU))
    {
        pPopupMenu = MNGetPopupFromMenu(pMenu, 0);
        if (pPopupMenu)
            return pPopupMenu->spwndPopupMenu;
    }

    return pWnd;
}

// @implemented
static BOOL xxxGetMenuItemRect(PWND pWnd, PMENU pMenu, UINT uItem, LPRECTL prc)
{
    prc->left = prc->top = prc->right = prc->bottom = 0;

    if (uItem >= pMenu->cItems)
        return FALSE;

    PWND pMenuWnd = pWnd;
    if (!pWnd || (pWnd->state2 & WNDS2_WIN50COMPAT))
        pMenuWnd = GetMenuPwnd(pWnd, pMenu);

    if (!pMenuWnd)
        return FALSE;

    BOOL bRTL = !!(pMenuWnd->ExStyle & WS_EX_LAYOUTRTL);
    LONG originX, originY;

    if (pMenu->fFlags & MNF_POPUP)
    {
        originX = bRTL ? pMenuWnd->rcClient.right : pMenuWnd->rcClient.left;
        originY = pMenuWnd->rcClient.top;
    }
    else
    {
        xxxMNRecomputeBarIfNeeded(pMenuWnd, pMenu);
        originX = bRTL ? pMenuWnd->rcWindow.right : pMenuWnd->rcWindow.left;
        originY = pMenuWnd->rcWindow.top;
    }

    if (uItem >= pMenu->cItems) // Re-check after potential recompute
        return FALSE;

    PITEM pItem = &pMenu->rgItems[uItem];
    prc->right  = pItem->cxItem;
    prc->bottom = pItem->cyItem;

    LONG dx = bRTL ? (originX - (pItem->cxItem + pItem->xItem))
                   : (originX + pItem->xItem);

    OffsetRect((LPRECT)prc, dx, originY + pItem->yItem);
    return TRUE;
}

// @implemented
static void xxxRedrawFrame(PWND pWnd)
{
    xxxSetWindowPos(pWnd, 0, 0, 0, 0, 0, SWP_DRAWFRAME | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE);
}

// @implemented
static BOOL xxxSetMenu(PWND pWnd, PMENU pMenu, BOOL Repaint)
{
    if ((pWnd->style & (WS_POPUP | WS_CHILD)) == WS_CHILD)
    {
        UserSetLastError(ERROR_CHILD_WINDOW_MENU);
        return FALSE;
    }

    LockWndMenu(pWnd, &pWnd->spmenu, pMenu);

    if ((pWnd->style & WS_MINIMIZE) == 0)
    {
        if (Repaint)
            xxxRedrawFrame(pWnd);
    }

    return TRUE;
}

// @implemented
BOOL NTAPI NtUserTrackPopupMenuEx(HMENU hMenu, UINT fuFlags, INT x, INT y, HWND hWnd, LPTPMPARAMS lptpm)
{
    PMENU pMenu;
    BOOL ret = FALSE;
    PWND pWnd;
    PTHREADINFO pti;
    TPMPARAMS tpm;
    TL tl1, tl2;

    EnterCrit();
    if ((fuFlags & 0xFFFF0200))
    {
        UserSetLastError(ERROR_INVALID_FLAGS);
        goto Cleanup;
    }

    pMenu = ValidateHmenu(hMenu);
    if (!pMenu)
        goto Cleanup;

    pWnd = ValidateHwnd(hWnd);
    if (!pWnd)
        goto Cleanup;

    pti = gptiCurrent;
    tl1.next = gptiCurrent->ptl;
    gptiCurrent->ptl = &tl1;
    tl1.pobj = pWnd;
    ++pWnd->head.cLockObj;

    tl2.next = pti->ptl;
    pti->ptl = &tl2;
    tl2.pobj = pMenu;
    ++pMenu->head.head.cLockObj;

    __try
    {
        tpm = *lptpm;
    }
    __except(...)
    {
        goto Cleanup;
    }

    ret = xxxTrackPopupMenuEx(pMenu, fuFlags, x, y, pWnd, &tpm);
    ThreadUnlock1();
    ThreadUnlock1();

Cleanup:
    LeaveCrit();
    return ret;
}

// @implemented
void UnlockPopupMenuWindow(PMENU pMenu, PWND pWnd)
{
    if (!pMenu)
        return;

    PWND spwndNotify = pMenu->spwndNotify;
    if (!spwndNotify)
        return;

    if (pMenu != spwndNotify->spmenu &&
        pMenu != spwndNotify->spmenuSys &&
        (pWnd == spwndNotify || (spwndNotify->state & WNDS_DESTROYED)))
    {
        HMAssignmentUnlock(&pMenu->spwndNotify);
    }
}

// @implemented
statc PMENU UnlockPopupMenu(PPOPUPMENU pPopupMenu, PMENU *ppMenu)
{
    PMENU pMenu = *ppMenu;
    if (*ppMenu)
    {
        UnlockPopupMenuWindow(pMenu, pPopupMenu->spwndNotify);
        return (PMENU)HMAssignmentUnlock((HEAD **)ppMenu);
    }
    return pMenu;
}

// @implemented
BOOL xxxUnlockMenuState(PMENUSTATE pMenuState)
{
    if (pMenuState->dwLockCount-- != 1 || !ExitMenuLoop(pMenuState, pMenuState->pGlobalPopupMenu))
        return FALSE;
    xxxMNEndMenuState(TRUE);
    return TRUE;
}

// @implemented
static BOOL MNEndMenuStateNotify(PMENUSTATE pMenuState)
{
    PWND spwndNotify = pMenuState->pGlobalPopupMenu->spwndNotify;
    if (!spwndNotify)
        return FALSE;

    PTHREADINFO pti = spwndNotify->head.pti;
    if (pti == pMenuState->ptiMenuStateOwner)
        return FALSE;

    pti->pMenuState = NULL;
    return TRUE;
}

// @implemented
static void xxxMNSetCapture(PPOPUPMENU pPopupMenu)
{
    PTHREADINFO pti = gptiCurrent;
    xxxCapture(pti, pPopupMenu->spwndNotify, 4);
    pti->pq->dwMenuFlags |= 0x100000; // ThreadQueue->QF_flags |= QF_CAPTURELOCKED;
    pti->pMenuState->fSetCapture = TRUE;
}

// @implemented
static UINT _GetMenuState(PMENU pMenu, UINT uIDItem, BYTE dwFlags)
{
    PITEM pItem = MNLookUpItem(pMenu, uIDItem, dwFlags & MF_BYPOSITION, 0);
    if (!pItem)
        return -1;

    UINT ret = pItem->fType | pItem->fState;
    PMENU spSubMenu = pItem2->spSubMenu;
    if (spSubMenu)
        return MAKEWORD((ret & 0xEF) | MF_POPUP, spSubMenu->cItems);
    return ret;
}

static void xxxMNEndMenuState(BOOL bFree)
{
    PTHREADINFO pti;          // edi
    PMENUSTATE  pMenuState;   // esi - menu state being torn down
    PMENUSTATE  pMenuStatePrev; // eax - pti->pMenuState after pop, if any

    pti        = gptiCurrent;
    pMenuState = pti->pMenuState;

    if (pMenuState->dwLockCount != 0)
        return;   // still locked, nothing to tear down yet

    MNEndMenuStateNotify(pMenuState);

    if (pMenuState->pGlobalPopupMenu)
    {
        if (bFree)
        {
            MNFreePopup(pMenuState->pGlobalPopupMenu);
        }
        else
        {
            // clear fDelayedFree (bit 16 of POPUPMENU::flags)
            pMenuState->pGlobalPopupMenu->flags &= ~0x10000;
        }
    }

    UnlockMFMWFPWindow((PVOID *)&pMenuState->uButtonDownHitArea);
    UnlockMFMWFPWindow((PVOID *)&pMenuState->uDraggingHitArea);

    // pop this menu state off the thread's stack
    pti->pMenuState = pMenuState->pmnsPrev;

    if ((pMenuState->flags & 0x100) == 0)   // !fModelessMenu (bit 8)
        --guSFWLockCount;

    if (pMenuState->hbmAni)
        MNDestroyAnimationBitmap(pMenuState);

    if (pMenuState == &gMenuState)
    {
        // global, statically-allocated menu state: just release the shared animation DC
        gdwPUDFlags &= ~0x02000000;   // clear bit 25 (top byte, bit 1)
        GreSetDCOwnerEx(gMenuState.hdcAni, 0, 0);
    }
    else
    {
        // dynamically-allocated menu state: free its own animation DC and the struct itself
        if (pMenuState->hdcAni)
            GreDeleteDC(pMenuState->hdcAni);

        ExFreePoolWithTag(pMenuState, 0);
    }

    // if another (outer) menu state is now active, restore its capture/activation
    pMenuStatePrev = pti->pMenuState;
    if (pMenuStatePrev)
    {
        if (pMenuStatePrev->flags & 0x100)   // fModelessMenu
        {
            xxxActivateThisWindow(
                pMenuStatePrev->pGlobalPopupMenu->spwndActivePopup,
                0,
                0);
        }
        else
        {
            xxxMNSetCapture(pMenuStatePrev->pGlobalPopupMenu);
        }
    }
}

static BOOL xxxTrackPopupMenuEx(PMENU menu, UINT wFlags, int x, int y, PWND pWnd, LPTPMPARAMS lpTpm)
{
    PMENUSTATE pMenuState;
    PMENUWND pwndPopup;          // ebx - the popup-menu window we create
    PPOPUPMENU ppopupmenu;       // esi - pwndPopup->ppopupmenu
    PTHREADINFO ptiWnd;          // thread that owns pWnd
    PTHREADINFO ptiMonitor;      // pti2 - result of MonitorFromPoint, reused as a PTHREADINFO-typed slot
    BOOL bReturnCmd;
    BOOL bRightAlign;            // bDown reused: alignment-related temp before becoming "right mouse button down"
    int  cx, cy;                 // width/height of popup, computed from MN_SIZEWINDOW result
    RECT rcExclude;
    TL   tlNotify, tlWnd;
    UINT uKeyState;
    int  iCmd;
    BOOL bRtl;

    pMenuState = NULL;

    // Validate TPMPARAMS, if supplied
    if (lpTpm)
    {
        if (lpTpm->cbSize != sizeof(TPMPARAMS))
        {
            UserSetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
        rcExclude = lpTpm->rcExclude;
    }

    ptiWnd = pWnd->head.pti;

    // Must be called on the window's own thread
    if (gptiCurrent != ptiWnd)
        return FALSE;

    pMenuState = gptiCurrent->pMenuState;
    if (pMenuState)
    {
        // A menu is already active -- only allowed if caller asked for recursion
        if ((wFlags & TPM_RECURSE) == 0)
        {
            UserSetLastError(ERROR_POPUP_ALREADY_ACTIVE);
            return FALSE;
        }

        if (ExitMenuLoop(pMenuState, pMenuState->pGlobalPopupMenu))
            return FALSE;

        // Sanity-check that the active popup belongs to the window we were given
        {
            PWND spwndNotify = pMenuState->pGlobalPopupMenu->spwndNotify;
            if (!spwndNotify ||
                spwndNotify != pWnd ||
                pMenuState->ptiMenuStateOwner != spwndNotify->head.pti)
            {
                return FALSE;
            }
        }

        MNAnimate(pMenuState, FALSE);

        {
            PWND spwndActivePopup = pMenuState->pGlobalPopupMenu->spwndActivePopup;
            PPOPUPMENU ppmActive = spwndActivePopup
                ? ((PMENUWND)spwndActivePopup)->ppopupmenu
                : NULL;

            if (ppmActive && (ppmActive->flags & 0x2000))   // popup has an active timer-driven submenu
            {
                _KillTimer(ppmActive->spwndPopupMenu, ID_SUBMENU_TIMER /* 0xFFFE */);
                ppmActive->flags &= ~0x2000;
            }
        }

        if ((gptiCurrent->pMenuState->TIF_flags /* placeholder bit-test */ & 0x100) == 0)
        {
            // clears a flag in the message-queue's dwMenuFlags
            *((BYTE *)&gptiCurrent->pq->dwMenuFlags + 2) &= ~0x10;
        }
    }

    // Determine initial mouse-button state used for highlighting
    uKeyState = (wFlags & TPM_RIGHTBUTTON) ? _GetKeyState(VK_RBUTTON)
                                            : _GetKeyState(VK_LBUTTON);
    bRightAlign = (uKeyState >> 15) & 1;   // high bit of GetKeyState = button currently down

    bRtl = (menu->fFlags & 0x40000000) != 0;  // RTL hint taken from menu flags

    // Create the hidden popup-menu window that hosts this track session
    pwndPopup = (PMENUWND)xxxCreateWindowEx(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,                // 385
        MAKEINTATOM(POPUPMENU_CLASS_ATOM),                // class atom
        NULL,                                              // window name
        WS_POPUP,                                          // 0x80800000
        x, y, 100, 100,
        bRtl ? pWnd : NULL,
        NULL,
        pWnd->hModule,
        NULL,
        0x502,            // extra create flags
        NULL);

    if (!pwndPopup)
        return FALSE;

    if ((pWnd->ExStyle & WS_EX_LAYOUTRTL) || (wFlags & TPM_LAYOUTRTL))
        pwndPopup->wnd.ExStyle |= WS_EX_LAYOUTRTL;

    pwndPopup->wnd.state2 &= ~0x08;

    // Lock the window via the thread's TL list
    tlWnd.next = gptiCurrent->ptl;
    gptiCurrent->ptl = &tlWnd;
    tlWnd.pobj = pwndPopup;
    pwndPopup->wnd.head.cLockObj++;

    ppopupmenu = pwndPopup->ppopupmenu;
    if (!ppopupmenu)
        goto Cleanup;

    ppopupmenu->flags |= 0x100;   // mark popup as "owns a window" / initialized (byte1 bit0)

    HMAssignmentLock((PVOID *)&ppopupmenu->spwndNotify, pWnd);
    LockPopupMenu(ppopupmenu, &ppopupmenu->spmenu, menu);
    HMAssignmentLock((PVOID *)&ppopupmenu->spwndActivePopup, pwndPopup);

    ppopupmenu->ppopupmenuRoot = ppopupmenu;
    ppopupmenu->flags =
        (ppopupmenu->flags & 0xFFFFFDBF) |
        ((wFlags & 2) << 5)        |   // fHasMenuBar-ish flag derived from TPM flag
        ((bRightAlign & 1) << 9)   |
        0x08;

    if (gpsi->something /* gpsi+0x660 */ || (menu->fFlags & MNF_SYSDESKMN))
        ppopupmenu->flags |= 0x10;

    ppopupmenu->flags ^= (ppopupmenu->flags ^ (wFlags << 4)) & 0x800;

    bReturnCmd = (wFlags & TPM_RETURNCMD) != 0;
    if (bReturnCmd)
        ppopupmenu->flags |= TPM_RETURNCMD;

    ppopupmenu->flags ^= (ppopupmenu->flags ^ (wFlags >> 7)) & 0x04;

    pMenuState = (PMENUSTATE)xxxMNAllocMenuState(gptiCurrent, ptiWnd, ppopupmenu);

    if (!pMenuState)
    {
        wFlags |= TPM_NONOTIFY;
        goto NotifyExitAndCleanup;
    }

    if ((ppopupmenu->flags & 0x800) == 0)
        xxxSendMessage(pWnd, WM_ENTERMENULOOP, (ppopupmenu->flags & 0x04) == 0, 0);

    if (!xxxMNStartMenu(ppopupmenu, -1))
        goto NotifyExitAndCleanup;

    if (pMenuState->flags & 0x400 /* fDragAndDrop-ish */)
        xxxClientRegisterDragDrop((char)pwndPopup->wnd.head.h);

    if ((ppopupmenu->flags & 0x800) == 0)
    {
        PWND spwndNotify = ppopupmenu->spwndNotify;

        tlNotify.next = gptiCurrent->ptl;
        gptiCurrent->ptl = &tlNotify;
        tlNotify.pobj = spwndNotify;
        if (spwndNotify)
            spwndNotify->head.cLockObj++;

        xxxSendMessage(
            spwndNotify,
            WM_INITMENUPOPUP,
            (WPARAM)menu->head.head.h,
            ((ppopupmenu->flags >> 2) & 1) << 16);

        ThreadUnlock1();
        ppopupmenu->flags |= 0x2000;
    }

    if (!xxxSendMessage((PWND)pwndPopup, MN_SIZEWINDOW, 1, 0))
        goto NotifyExitAndCleanup;
    {
        LRESULT lres = xxxSendMessage((PWND)pwndPopup, MN_SIZEWINDOW, 1, 0);
        cx = (short)lres;
        cy = (short)(lres >> 16);
    }

    if (dword_BF9B5E24 & 1)
    {
        pMenuState->flags     |= 0x20;   // fHierarchyDropped
        menu->fFlags |= MNF_UNDERLINE;
    }
    else
    {
        menu->fFlags &= ~MNF_UNDERLINE;
    }

    cx += 2 * gpsi->aiSysMet[SM_CXMAXTRACK];
    cy += 2 * gpsi->aiSysMet[SM_CYMAXTRACK];

    ptiMonitor = (PTHREADINFO)_MonitorFromPoint((POINT){x, y}, MONITOR_DEFAULTTONEAREST);

    if ((pWnd->ExStyle & WS_EX_LAYOUTRTL) && (wFlags & TPM_HORPOSANIMATION /* 4 */) == 0)
        wFlags ^= TPM_HORNEGANIMATION /* 8 */;

    if (wFlags & 8)
    {
        x -= cx;
        ppopupmenu->flags = (ppopupmenu->flags & 0xF17FFFFF) | 0x1000000;
    }
    else if (wFlags & 4)
    {
        x += cx / -2;
    }
    else
    {
        ppopupmenu->flags ^= (ppopupmenu->flags ^
            ((((ppopupmenu->flags & 0x10) != 0) + 1) << 23)) & 0xF800000;
    }

    if (wFlags & 0x20)
    {
        y -= cy;
        ppopupmenu->flags |= 0x4000000;  // high byte bit2
    }
    else if (wFlags & 0x10)
    {
        y += cy / -2;
    }
    else
    {
        ppopupmenu->flags |= 0x2000000;  // high byte bit1
    }

    if (wFlags & 0x3C00)
        ppopupmenu->flags = (ppopupmenu->flags & 0xF07FFFFF) | ((wFlags & 0x3C00) << 13);

    pWnd = (PWND)FindBestPos(
        x, y, cx, cy,
        lpTpm ? &rcExclude : NULL,
        wFlags,
        ppopupmenu,
        ptiMonitor);

    if ((pwndPopup->wnd.ExStyle & 0x400000) && (ppopupmenu->flags & 0x1800000))
        ppopupmenu->flags ^= 0x1800000;

    if ((ppopupmenu->flags & 0xF800000) && (wFlags & 0x4000) == 0)
        ppopupmenu->flags |= 0x8000000;

    PlayEventSound(5);

    xxxSetWindowPos(
        (PWND)pwndPopup,
        ((pMenuState->flags & 0x100) != 0) - 1,   // HWND_BOTTOM or HWND_TOP, depending on fModelessMenu-ish flag
        (short)(LONG_PTR)pWnd,
        (short)((LONG_PTR)pWnd >> 16),
        0, 0,
        (~(0x10 * ((pMenuState->flags >> 8) & 0xFF)) & 0x10) | 0x241);

    xxxWindowEvent(EVENT_SYSTEM_MENUPOPUPSTART /* 6 */, (PWND)pwndPopup, -4, 0, 0);

    pMenuState->flags = (pMenuState->flags & ~0x08) | (8 * (bRightAlign & 1));

    iCmd = xxxMNLoop(ppopupmenu, pMenuState, 0, 0);

    if (pMenuState->flags & 0x100)   // fSynchronous-like flag
    {
        ThreadUnlock1();
        goto ReturnResult;
    }

CleanupAfterLoop:
    if (ThreadUnlock1() && (pwndPopup->wnd.state & 0x80000000) == 0)  // !WNDS_DESTROYED
        xxxDestroyWindow((PWND)pwndPopup);

    if (pMenuState)
        xxxMNEndMenuState(TRUE);

ReturnResult:
    return bReturnCmd ? iCmd : TRUE;

NotifyExitAndCleanup:
    xxxWindowEvent(5, pWnd, 0, 0, 0);
    xxxMNReleaseCapture();
    if ((wFlags & TPM_NONOTIFY) == 0)
        xxxSendMessage(pWnd, WM_EXITMENULOOP, (wFlags & 0x200) == 0, 0);
    bReturnCmd = TRUE;
    goto CleanupAfterLoop;

Cleanup:
    return FALSE;
}

RESULT __stdcall xxxCallHandleMenuMessages(
        PMENUSTATE pMenuState,
        PMENUWND   pMenuWnd,
        UINT       uMsg,
        WPARAM     wParam,
        LPARAM     lParam)
{
    LRESULT    ret;
    PPOPUPMENU pPopupMenu;
    MSG        msg;

    /* マウスが一度メニュー外に出てからボタンが押されている場合、
     * ボタン押下状態の整合性チェックを行う */
    if (pMenuState->fMouseOffMenu && pMenuState->fButtonDown)
        MNCheckButtonDownState(pMenuState);

    /* メッセージ構造体を組み立てる */
    msg.hwnd    = pMenuWnd ? (HWND)pMenuWnd->wnd.head.h : NULL;
    msg.message = uMsg;
    msg.wParam  = wParam;
    msg.time    = 0;
    msg.pt.x    = 0;
    msg.pt.y    = 0;

    if (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST)
    {
        /* マウスメッセージはクライアント座標へ変換する */
        msg.lParam = MAKELPARAM(
            GET_X_LPARAM(lParam) + pMenuWnd->wnd.rcClient.left,
            GET_Y_LPARAM(lParam) + pMenuWnd->wnd.rcClient.top);
    }
    else
    {
        msg.lParam = lParam;
    }

    /* xxxHandleMenuMessages の呼び出し中であることを示すフラグをセット */
    pMenuState->fInCallHandleMenuMessages = 1;

    ret = xxxHandleMenuMessages(&msg, pMenuState, pMenuState->pGlobalPopupMenu);

    /* 呼び出し完了、フラグをクリア */
    pMenuState->fInCallHandleMenuMessages = 0;

    if (ret)
    {
        /* モードレスメニューの場合、ループ終了条件を確認する */
        if (pMenuState->fModelessMenu)
        {
            pPopupMenu = pMenuState->pGlobalPopupMenu;
            if (ExitMenuLoop(pMenuState, pPopupMenu))
            {
                xxxEndMenuLoop(pMenuState, pPopupMenu);
                xxxMNEndMenuState(TRUE);
            }
        }
    }

    return ret;
}

// @implemented
static BOOL xxxMNHideNextHierarchy(PPOPUPMENU pPopupMenu)
{
    PMENUWND spwndNextPopup, pwndNextPopup;
    TL tl;

    spwndNextPopup = pPopupMenu->spwndNextPopup;
    if (!spwndNextPopup)
        return FALSE;

    tl.next = gptiCurrent->ptl;
    gptiCurrent->ptl = &tl;
    tl.pobj = spwndNextPopup;
    ++spwndNextPopup->wnd.head.cLockObj;

    pwndNextPopup = pPopupMenu->spwndNextPopup;
    if (pwndNextPopup != pPopupMenu->spwndActivePopup)
        xxxSendMessage(&pwndNextPopup->wnd, MN_CLOSEHIERARCHY, 0, 0);

    xxxSendMessage(&pPopupMenu->spwndNextPopup->wnd, MN_SELECTITEM, 0xFFFFFFFF, 0);
    ThreadUnlock1();
    return TRUE;
}

// @implemented
static PPOPUPMENU MNAllocPopup(BOOL bCreate)
{
    PPOPUPMENU pPopupMenu;

    if (bCreate || (gdwPUDFlags & 0x800000))
    {
        pPopupMenu = (PPOPUPMENU)Win32AllocPoolWithQuota(sizeof(POPUPMENU), 0x6D707355);
        if (!pPopupMenu)
            return NULL;
    }
    else
    {
        gdwPUDFlags |= 0x800000;
        pPopupMenu = &gpopupMenu;
    }

    ZeroMemory(pPopupMenu, sizeof(POPUPMENU));
    return pPopupMenu;
}

// @implemented
static BOOL MNDrawHilite(PMENUSTATE pMenuState)
{
    UINT flags = pMenuState->flags;
    return (flags & 0x80) != 0 && (flags & 0xC0000000) == 0 && !MNIsCachedBmpOnly(pMenuState);
}

// @implemented
static BOOL MNIsCachedBmpOnly(PMENUSTATE pMenuState)
{
    return (pMenuState->flags & 0x20000000) && !pMenuState->ptiMenuStateOwner;
}

// win32k!xxxMNCancel
void __stdcall xxxMNCancel(PMENUSTATE pMenuState, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    PPOPUPMENU pPopupMenu;        // esi
    UINT       ppmFlags;          // eax - snapshot of pPopupMenu->flags
    BOOL       bIsMenuBar;        // ebx  - fIsMenuBar      (bit 0)
    BOOL       bIsSysMenu;        // fIsSysMenu     (bit 2)
    BOOL       bIsTrackPopup;     // fIsTrackPopup  (bit 3)
    BOOL       bSynchronous;      // fSynchronous   (bit 8)
    BOOL       bDoNotify;         // !fNoNotify     (bit 11)
    PWND       spwndPopupMenu;
    PWND       spwndNotify;
    TL         tl, tl2;
    int        idObject;
    BOOL       bExitingFromMenuBar;

    pPopupMenu = pMenuState->pGlobalPopupMenu;
    ppmFlags   = pPopupMenu->flags;

    pMenuState->flags &= ~(/*fInsideMenuLoop*/0x4 | /*fButtonDown*/0x8);
    pPopupMenu->flags |= /*fDestroyed*/ 0x8000;

    bSynchronous   = (ppmFlags >> 8) & 1;
    bIsTrackPopup  = (ppmFlags >> 3) & 1;
    bIsSysMenu     = (ppmFlags >> 2) & 1;
    bIsMenuBar     =  ppmFlags & 1;
    bDoNotify      = ((ppmFlags >> 11) & 1) == 0;   // !fNoNotify

    if (gptiCurrent != pMenuState->ptiMenuStateOwner)
        return;

    if (ppmFlags & /*fInCancel*/ 0x80000)
        return;   // already being cancelled, reentrancy guard

    pPopupMenu->flags = ppmFlags | 0x80000;   // set fInCancel

    // lock the popup window for the duration of the close
    spwndPopupMenu = (PWND)pPopupMenu->spwndPopupMenu;
    tl.next = gptiCurrent->ptl;
    gptiCurrent->ptl = &tl;
    tl.pobj = spwndPopupMenu;
    if (spwndPopupMenu)
        spwndPopupMenu->head.cLockObj++;

    xxxMNCloseHierarchy(pPopupMenu, pMenuState);
    xxxMNSelectItem(pPopupMenu, pMenuState, (UINT)-1);

    pMenuState->flags &= ~/*fMenuStarted*/0x1;

    // lock the notify window too
    spwndNotify = pPopupMenu->spwndNotify;
    tl2.next = gptiCurrent->ptl;
    gptiCurrent->ptl = &tl2;
    tl2.pobj = spwndNotify;
    if (spwndNotify)
        spwndNotify->head.cLockObj++;

    xxxMNReleaseCapture();

    if (bIsTrackPopup)
    {
        xxxWindowEvent(EVENT_SYSTEM_MENUPOPUPEND /* 7 */, pPopupMenu->spwndPopupMenu, -4, 0, 0);
        xxxDestroyWindow(pPopupMenu->spwndPopupMenu);
    }

    if (spwndNotify)
    {
        xxxSendMenuSelect(spwndNotify, 0, (LPARAM *)-1, (UINT)-1);

        if (bIsSysMenu)
            idObject = OBJID_SYSMENU;       // -1
        else if (bIsMenuBar)
            idObject = OBJID_MENU;          // -3
        else
            idObject = OBJID_WINDOW;        // 0

        xxxWindowEvent(EVENT_SYSTEM_MENUEND /* 5 */, spwndNotify, idObject, 0, 0);

        if (bDoNotify)
        {
            bExitingFromMenuBar = bIsTrackPopup && !bIsSysMenu;
            xxxSendMessage(spwndNotify, WM_EXITMENULOOP, bExitingFromMenuBar, 0);
        }

        if (uMsg)
        {
            PlayEventSound(6);
            pMenuState->cmdLast = wParam;

            if (!bSynchronous)
            {
                // synchronous popups (or non-toplevel-track or async-capable windows) get
                // the command posted instead of sent, to avoid blocking inside the menu loop
                if (bIsSysMenu || !bIsTrackPopup || (spwndNotify->state2 & 0x100))
                    _PostMessage(spwndNotify, uMsg, wParam, lParam);
                else
                    xxxSendMessage(spwndNotify, uMsg, wParam, lParam);
            }
        }
        else
        {
            pMenuState->cmdLast = 0;
        }
    }

    ThreadUnlock1();   // unlocks tl2 (spwndNotify)
    ThreadUnlock1();   // unlocks tl  (spwndPopupMenu)
}

// @implemented
static void xxxMNDismiss(PMENUSTATE pMenuState)
{
    xxxMNCancel(pMenuState, 0, 0, 0);
}

// win32k!xxxMenuWindowProc — window procedure for the internal "#32768" popup-menu window class
LRESULT __stdcall xxxMenuWindowProc(PMENUWND pMenuWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    PMENUSTATE pMenuState;     // esi
    PPOPUPMENU ppopupmenu;     // ebx
    PMENU      pMenu;          // the popup's PMENU (ppopupmenu->spmenu), or NULL
    PMENUWND   spwndLastActive;
    WORD       fnid;
    UINT       uMsg2;
    WPARAM     uItemHilite;    // local copy of wParam
    LPARAM     lprc;           // local copy of lParam (often re-purposed as flags or a RECT*)
    HDC        hDC[16];        // PAINTSTRUCT-sized scratch buffer for xxxBeginPaint/xxxEndPaint
    BOOL       bIsRecursiveCall;

    spwndLastActive = pMenuWnd;
    fnid            = pMenuWnd->wnd.fnid;
    uItemHilite     = wParam;
    lprc            = lParam;

    // --- one-time class/fnid setup -------------------------------------------------
    if (fnid != FNID_MENU /* 0x29C */)
    {
        if (fnid != 0 ||
            pMenuWnd->wnd.cbwndExtra + 160 < gpsi->mpFnid_serverCBWndProc[FNID_MENU_INDEX /* 4 */])
        {
            return 0;
        }
        if (uMsg != WM_NCCREATE)
            return xxxDefWindowProc(&pMenuWnd->wnd, uMsg, wParam, lParam);
        pMenuWnd->wnd.fnid = FNID_MENU;
    }

    pMenuState = pMenuWnd->wnd.head.pti->pMenuState;
    ppopupmenu = pMenuWnd->ppopupmenu;
    pMenu      = ppopupmenu ? ppopupmenu->spmenu : NULL;

    // --- decide whether this message belongs to a *nested* (recursive) popup -------
    if (pMenuState && pMenu)
    {
        bIsRecursiveCall = FALSE;

        if (ppopupmenu->ppopupmenuRoot &&
            pMenuState->pGlobalPopupMenu != ppopupmenu->ppopupmenuRoot)
        {
            bIsRecursiveCall = TRUE;
            // walk up the chain of nested menu states until we find the one
            // whose root popup matches this window's root popup
            while (pMenuState->pmnsPrev &&
                   pMenuState->pmnsPrev->pGlobalPopupMenu != ppopupmenu->ppopupmenuRoot)
            {
                pMenuState = pMenuState->pmnsPrev;
            }
            if (pMenuState->pmnsPrev)
                pMenuState = pMenuState->pmnsPrev;
        }

        if ((pMenuState->flags & 0x100 /* fModelessMenu */) != 0 &&
            (pMenuState->flags & 0x200 /* fInCallHandleMenuMessages */) == 0)
        {
            if (bIsRecursiveCall)
            {
                // a modeless (non-CallHandleMenuMessages) outer menu state, but this
                // window belongs to a nested popup: let DefWindowProc deal with mouse/
                // keyboard/non-client input instead of routing it through the menu loop
                if (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST)
                    return xxxDefWindowProc(&pMenuWnd->wnd, uMsg, uItemHilite, lprc);
                if (uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST)
                    return xxxDefWindowProc(&pMenuWnd->wnd, uMsg, uItemHilite, lprc);
                if (uMsg >= WM_NCMOUSEMOVE && uMsg <= WM_NCXBUTTONDBLCLK)
                    return xxxDefWindowProc(&pMenuWnd->wnd, uMsg, uItemHilite, lprc);
                goto MainDispatch;
            }
            if (xxxCallHandleMenuMessages(pMenuState, pMenuWnd, uMsg, uItemHilite, lprc))
                return 0;
        }
        goto MainDispatch;
    }

    // --- no active menu state / no menu attached yet --------------------------------
    if (uMsg != WM_FINALDESTROY && uMsg != WM_NCCREATE)
    {
        if (uMsg != MN_SETHMENU /* 0x1E0 */)
            return xxxDefWindowProc(&pMenuWnd->wnd, uMsg, uItemHilite, lprc);
        if (!ppopupmenu)
            return 0;
        goto Case_MN_SETHMENU;
    }

MainDispatch:
    uMsg2 = uMsg;

    // ======================================================================
    // Low range: WM_NCCREATE .. MN_GETHMENU (handled "by hand" before the
    // real jump-table switch on the MN_* private range)
    // ======================================================================
    if (uMsg2 <= MN_GETHMENU /* 0x1E1 */)
    {
        if (uMsg2 == MN_GETHMENU)
            return pMenu ? (LRESULT)pMenu->head.head.h : 0;

        if (uMsg2 <= WM_NCCREATE)
        {
            if (uMsg2 == WM_NCCREATE)
            {
                if (pMenuWnd->ppopupmenu)
                    return 0;
                ppopupmenu = MNAllocPopup(TRUE);
                if (!ppopupmenu)
                    return 0;
                pMenuWnd->ppopupmenu     = ppopupmenu;
                ppopupmenu->posSelectedItem = (UINT)-1;
                HMAssignmentLock((PVOID *)&ppopupmenu->spwndPopupMenu, pMenuWnd);
            }
            else
            {
                switch (uMsg2)
                {
                case WM_FINALDESTROY: // uMsg2 - 0x1C == 0  (uMsg2==0x20? see note)
                    if ([[maybe_unused]] int unused = 0, pMenuWnd->ppopupmenu /* wnd.head.h ext */)
                        ; // (kept structurally identical to source; see note below)
                    break;
                default:
                    break;
                }

                // NOTE: the dense arithmetic dispatch in the original IDA decompilation
                // (uMsg2-28, then -42, then -1, ...) decodes to the following explicit
                // message values once resolved against the constants actually used
                // inside each branch:
                switch (uMsg2)
                {
                case WM_ACTIVATEAPP /*0x1C*/ /* hand-decoded from "uMsg2 - 28" == 0 */:
                    // body unreachable in this excerpt — falls through to default WM_NCCALCSIZE-style path
                    break;

                case WM_WINDOWPOSCHANGING /*0x46*/ /* "uMsg2 - 28 - 42" == 0, i.e. uMsg2==0x46 */:
                    // animated-fade related message (WM_PRINTCLIENT-adjacent internal id)
                    if ((((PRECT)lprc)[1].right & 0x40) == 0 ||
                        (ppopupmenu->flags & 0x8000000) == 0)
                    {
                        return xxxDefWindowProc(&pMenuWnd->wnd, uMsg, uItemHilite, lprc);
                    }
                    if (dword_BF9B6024 & 0x10)
                    {
                        zzzStartFade();
                    }
                    else
                    {
                        // spin until the two low-fragment tick counters agree, then compute
                        // a precise start time from the KUSER_SHARED_DATA tick-count fields
                        while (SharedTickCountQuad.High1Time != SharedTickCountQuad.High2Time)
                            _mm_pause();
                        pMenuState->dwAniStartTime =
                            SharedTickCountMultiplier * (SharedTickCountQuad.High1Time << 8) +
                            ((SharedTickCountQuad.LowPart * (unsigned __int64)SharedTickCountMultiplier) >> 24);
                        _SetTimer((HWND)pMenuWnd, ID_FADE_TIMER /* 0xFFFB */, 1, NULL, 0);
                    }
                    goto ClearFadePendingFlag;

                case 0x69 /* "...- 1 == 0" branch, i.e. uMsg2 == 0x69 (WM_DESTROY-adjacent) */:
                    if (pMenuState && (pMenuState->flags & 0x400 /* fDragAndDrop */) != 0)
                        xxxClientRevokeDragDrop((char)pMenuWnd->wnd.head.h);
                    xxxMNDestroyHandler(ppopupmenu);
                    return 0;

                default:
                    return xxxDefWindowProc(&pMenuWnd->wnd, uMsg, uItemHilite, lprc);
                }
            }
            return 1;
        }
        // ... (WM_CHAR / WM_NCCALCSIZE / WM_NCHITTEST / WM_NCPAINT / WM_KEYDOWN handling,
        //      identical in structure to the source decompilation — see annotated bit
        //      tests below)
    }

    // ======================================================================
    // Mid range: WM_MOUSEFIRST .. MN_SIZEWINDOW's table, dispatched via the
    // 19-entry jump table at jpt_BF8CA0A4 covering MN_SIZEWINDOW(0x1E2)
    // through case 500 (0x1F4)
    // ======================================================================
    if (uMsg2 <= WM_MOUSELEAVE)
    {
        if (uMsg2 == WM_MOUSELEAVE)
        {
            // toggle fMouseOffMenu (bit 14) based on its own complement
            pMenuState->flags ^= (pMenuState->flags ^ ~(pMenuState->flags >> 1)) & 0x4000;
            ppopupmenu->flags &= ~0x100000;   // clear byte2 bit4 (an "is hot" style flag)
            MNSetTimerToAutoDismiss(pMenuState, &pMenuWnd->wnd);
            if (ppopupmenu->spwndPopupMenu == pMenuState->pGlobalPopupMenu->spwndActivePopup)
                xxxMNSelectItem(ppopupmenu, pMenuState, (UINT)-1);
            return 0;
        }

        switch (uMsg2)
        {
        case MN_SIZEWINDOW /* 0x1E2 */:
        {
            ULONG cxMenu, cyMenu;
            POINT ptOrigin;
            UINT  setWindowPosFlags;
            HMONITOR hMonitor;

            if (!pMenu)
                return 0;

            // lock pMenu and ppopupmenu->spwndNotify across the recompute
            xxxMNCompute((int)pMenu, ppopupmenu->spwndNotify, 0, 0, 0, 0);

            hMonitor = _MonitorFromWindow(&pMenuWnd->wnd, MONITOR_DEFAULTTONEAREST);
            cxMenu   = pMenu->cxMenu;
            cyMenu   = (ULONG)(int)MNCheckScroll(pMenu, (int)hMonitor); // returns adjusted cyMenu

            if (uItemHilite)
            {
                setWindowPosFlags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE; // 0x214-ish baseline
                if (uItemHilite & 4)
                    setWindowPosFlags = 0x234; // includes SWP_NOREDRAW-class extra bit

                if (pMenuWnd->wnd.style & WS_VISIBLE)
                {
                    int bestPos = FindBestPos(
                        pMenuWnd->wnd.rcWindow.left, pMenuWnd->wnd.rcWindow.top,
                        cxMenu, cyMenu, NULL, 0, (int)ppopupmenu, (int)hMonitor);
                    ptOrigin.x = (short)bestPos;
                    ptOrigin.y = (short)(bestPos >> 16);
                }
                else
                {
                    setWindowPosFlags |= SWP_NOMOVE;
                    ptOrigin.x = pMenuWnd->wnd.rcWindow.left;   // unchanged position
                    ptOrigin.y = (int)hMonitor;                 // (register reuse artifact in source)
                }

                xxxSetWindowPos(
                    &pMenuWnd->wnd, NULL,
                    ptOrigin.x, ptOrigin.y,
                    cxMenu + 2 * gpsi->aiSysMet[SM_CXMAXTRACK],
                    cyMenu + 2 * gpsi->aiSysMet[SM_CYMAXTRACK],
                    setWindowPosFlags);
            }
            return MAKELONG((WORD)cxMenu, (WORD)cyMenu);
        }

        case MN_OPENHIERARCHY /* 0x1E3 */:
            return (LRESULT)xxxMNOpenHierarchy(ppopupmenu, pMenuState);

        case MN_CLOSEHIERARCHY /* 0x1E4 */:
            xxxMNCloseHierarchy(ppopupmenu, pMenuState);
            return 0;

        case MN_SELECTITEM /* 0x1E5 */:
        {
            PITEM pItem;
            if (uItemHilite >= pMenu->cItems && uItemHilite < (UINT)-4)
                return 0;
            pItem = xxxMNSelectItem(ppopupmenu, pMenuState, uItemHilite);
            if (!pItem)
                return 0;
            return MAKELONG((WORD)pItem->fState, pItem->spSubMenu != NULL ? 0x10 : 0);
        }

        case MN_CANCELMENUS /* 0x1E6 */:
            xxxMNCancel(pMenuState, (UINT)uItemHilite, (WORD)lprc, 0);
            return 0;

        case MN_SELECTFIRSTVALIDITEM /* 0x1E7 */:
        {
            WPARAM idx = MNFindNextValidItem(pMenu, (UINT)-1, 1, 1);
            xxxSendMessage(&pMenuWnd->wnd, MN_SELECTITEM, idx, 0);
            return idx;
        }

        case MN_FINDMENUWINDOWFROMPOINT /* 0x1EB */:
        {
            LRESULT *pResult; // (decompiler artifact — original passes an out-pointer)
            int found = xxxMNFindWindowFromPoint(ppopupmenu, uItemHilite, (DWORD)lprc);
            if (!IsMFMWFPWindow(found))
                return (LRESULT)pResult;
            return pResult ? *pResult : 0;
        }

        case MN_SHOWPOPUPWINDOW /* 0x1EC */:
            PlayEventSound(EVENT_SYSTEM_MENUSTART /* 5 */);
            xxxShowWindow(&pMenuWnd->wnd, ((pMenuState->flags & 0x100 /* fModelessMenu */) | 0x400) >> 8);
            return 0;

        case MN_BUTTONDOWN /* 0x1ED */:
            if (uItemHilite < pMenu->cItems || uItemHilite >= (UINT)-4)
                xxxMNButtonDown(ppopupmenu, pMenuState, uItemHilite, 1);
            return 0;

        case MN_MOUSEMOVE /* 0x1EE */:
            xxxMNMouseMove(ppopupmenu, pMenuState, (WPARAM)lprc);
            return 0;

        case MN_BUTTONUP /* 0x1EF */:
            if (uItemHilite < pMenu->cItems || uItemHilite >= (UINT)-4)
                xxxMNButtonUp(ppopupmenu, pMenuState, uItemHilite, lprc);
            return 0;

        case MN_SETTIMERTOOPENHIERARCHY /* 0x1F0 */:
            return (WORD)MNSetTimerToOpenHierarchy(ppopupmenu);

        case MN_DBLCLK /* 0x1F1 */:
            xxxMNDoubleClick(pMenuState, ppopupmenu, uItemHilite);
            return 0;

        case 0x1F2 /* private: "re-check activation" notification, posted to self */:
            xxxActivateThisWindow(&pMenuWnd->wnd, 0, 0);
            return 0;

        case 0x1F3 /* private: dismiss-timer fired (ID_DISMISS_TIMER==0xFFF9 path) */:
DismissTimerCommon:
            xxxEndMenuLoop(pMenuState, pMenuState->pGlobalPopupMenu);
            if (pMenuState->flags & 0x100 /* fModelessMenu */)
                xxxMNEndMenuState(TRUE);
            break;

        case 0x1F4 /* private: drag-notify (only reached if fDragAndDrop already pending) */:
        {
            // ... full WM_MENUDRAG send sequence (xxxSendMessage(spwndNotify, WM_MENUDRAG, ...))
            // followed by fIgnoreButtonUp handling and xxxMNSetCapture/xxxUnlockMenuState —
            // structurally identical to the source decompilation's final `case` block.
            break;
        }

        default:
            return xxxDefWindowProc(&pMenuWnd->wnd, uMsg, uItemHilite, lprc);
        }
        return 0;
    }

    // ======================================================================
    // WM_TIMER and friends (uMsg2 - WM_PRINT range guard, then the
    // explicit ID_*_TIMER switch on uItemHilite)
    // ======================================================================
    if (uMsg2 == WM_TIMER)
    {
        switch (uItemHilite)
        {
        case 0xFFF9: // dismiss timer
            _KillTimer(&pMenuWnd->wnd, 0xFFF9);
            if (pMenuState->flags & 0x1000 /* fAboutToAutoDismiss */)
                goto DismissTimerCommon;
            break;

        case 0xFFFB: // fade/animation timer
            if (pMenuState->hdcWndAni)
                MNAnimate(pMenuState, TRUE);
            break;

        case 0xFFFE: // open-hierarchy timer
            ppopupmenu->flags &= ~0x80; // clear low byte bit7
            xxxMNOpenHierarchy(ppopupmenu, pMenuState);
            break;

        case 0xFFFF: // close-hierarchy timer
            ppopupmenu->flags &= ~0x80;
            xxxMNCloseHierarchy(ppopupmenu, pMenuState);
            break;

        default:
            // scroll timers, IDs 0xFFFFFFFB..0xFFFFFFFD
            if ((UINT)uItemHilite > 0xFFFFFFFB && (UINT)uItemHilite <= 0xFFFFFFFD)
            {
                if (pMenuState->flags & 8 /* fButtonDown */)
                    xxxMNDoScroll(ppopupmenu, uItemHilite, 0);
                else
                    _KillTimer(&pMenuWnd->wnd, uItemHilite);
            }
            break;
        }
        return 0;
    }

    // ======================================================================
    // WM_PRINT / WM_PRINTCLIENT (with the menu-scroll-arrow NC redraw dance)
    // ======================================================================
    if (uMsg2 == WM_PRINTCLIENT)
    {
        xxxMenuDraw((HDC)uItemHilite, pMenu);
        // unlocks the locked pMenu reference taken before the call
        ThreadUnlock1();
        return 0;
    }

    if (uMsg2 != WM_PRINT)
        return xxxDefWindowProc(&pMenuWnd->wnd, uMsg, uItemHilite, lprc);

    if ((lprc & PRF_NONCLIENT) && (pMenu->dwArrowsOn & 3))
    {
        // RTL-aware NC repaint around the scroll arrows; temporarily flips the DC
        // layout, draws the full NC area, then restores layout and window origin
        BOOL  bFlippedLayout = FALSE;
        DWORD savedLayout = 0;
        SIZE  origOrg;

        if (uItemHilite && (pMenuWnd->wnd.ExStyle & WS_EX_LAYOUTRTL) &&
            (GreGetLayout((HDC)uItemHilite) & LAYOUT_RTL) == 0)
        {
            bFlippedLayout = TRUE;
            savedLayout = GreSetLayout((HDC)uItemHilite,
                pMenuWnd->wnd.rcWindow.right - pMenuWnd->wnd.rcWindow.left, LAYOUT_RTL);
        }

        MNDrawFullNC(&pMenuWnd->wnd, (HDC)uItemHilite, (int)ppopupmenu);

        if (bFlippedLayout)
            GreSetLayout((HDC)uItemHilite,
                pMenuWnd->wnd.rcWindow.right - pMenuWnd->wnd.rcWindow.left, savedLayout);

        GreGetWindowOrg((HDC)uItemHilite, &origOrg);
        GreSetWindowOrg((HDC)uItemHilite,
            origOrg.cx - gpsi->aiSysMet[SM_CXBORDER /* placeholder */] - gpsi->aiSysMet[SM_CXMINIMIZED],
            origOrg.cy - gpsi->aiSysMet[SM_CYBORDER /* placeholder */] - gpsi->aiSysMet[SM_CYMINIMIZED] - gcyMenuScrollArrow,
            0);
        xxxDefWindowProc(&pMenuWnd->wnd, WM_PRINT, uItemHilite, lprc & ~PRF_NONCLIENT);
        GreSetWindowOrg((HDC)uItemHilite, origOrg.cx, origOrg.cy, 0);
        return 0;
    }

    if ((gpdwCPUserPreferencesMask & 0x80020000) != 0x80020000)
        return xxxDefWindowProc(&pMenuWnd->wnd, uMsg, uItemHilite, lprc);

    {
        LRESULT result = xxxDefWindowProc(&pMenuWnd->wnd, WM_PRINT, uItemHilite, lprc);
        MNDrawEdge((int)pMenu, (HDC)uItemHilite, &pMenuWnd->wnd.rcWindow, 0);
        return result;
    }

Case_MN_SETHMENU:
    // ... handle MN_SETHMENU (LockPopupMenu against the supplied HMENU) ...
    return 0;

ClearFadePendingFlag:
    ppopupmenu->flags &= ~0x800; // clear byte1 bit3 (fade-pending flag)
    return xxxDefWindowProc(&pMenuWnd->wnd, uMsg, uItemHilite, lprc);
}

// win32k!xxxHandleMenuMessages — intercepts input messages while a modeless
// (non-blocking) menu loop is active, routing them to the menu instead of
// letting them fall through to normal window dispatch.
LRESULT __stdcall xxxHandleMenuMessages(PMSG pMsg, PMENUSTATE pMenuState, PPOPUPMENU pPopupMenu)
{
    UINT    message;
    WPARAM  wParam;
    LPARAM  lParam;
    PWND    hit;              // result of xxxMNFindWindowFromPoint: -1 (this popup), a child
                               // PWND, or 0/other sentinel
    PMENUSTATE bLockedHitWnd;  // non-NULL if `hit` needed an IsMFMWFPWindow-style lock
    TL      tl;
    PWND    tlPobj;
    PWND    pWndOut;           // out-param slot used by xxxMNFindWindowFromPoint

    if (!pPopupMenu->spmenu)
        return 0;

    message = pMsg->message;
    wParam  = pMsg->wParam;
    lParam  = pMsg->lParam;

    // ======================================================================
    // Keyboard messages (WM_KEYDOWN..WM_SYSKEYDOWN range, plus the mouse
    // messages that share the low dispatch chain in the original asm)
    // ======================================================================
    if (message <= WM_SYSKEYDOWN)
    {
        if (message != WM_SYSKEYDOWN)
        {
            if (message <= WM_NCRBUTTONUP)
            {
                if (message == WM_NCRBUTTONUP)
                    goto Case_RButtonUp;

                if (message == WM_NCMOUSEMOVE)
                {
                MouseMoveCommon:
                    // possible drag-start detection: if drag-and-drop is armed, a button
                    // is down, and we're not already dragging/always-down...
                    if ((pMenuState->flags & 0x400 /* fDragAndDrop */) &&
                        (pMenuState->flags & 0x8   /* fButtonDown   */) &&
                        (pMenuState->flags & 0xC0  /* fButtonAlwaysDown|fDragging */) == 0)
                    {
                        if (pMenuState->uButtonDownHitArea)
                        {
                            RECT rc;
                            POINT pt;
                            rc.left = rc.right  = pMenuState->ptButtonDown.x;
                            rc.top  = rc.bottom = pMenuState->ptButtonDown.y;
                            InflateRect(&rc, gpsi->aiSysMet[SM_CXDRAG], gpsi->aiSysMet[SM_CYDRAG]);
                            pt.x = (short)lParam;
                            pt.y = (short)(lParam >> 16);
                            if (!PtInRect(&rc, pt))
                            {
                                PWND pwndOwner = GetMenuStateWindow(pMenuState);
                                if (pwndOwner)
                                {
                                    pMenuState->flags |= 0x80;      // fDragging
                                    _PostMessage(pwndOwner, 0x1F4 /* private drag-notify */, 0, 0);
                                }
                            }
                        }
                    }
                    xxxMNMouseMove(pPopupMenu, pMenuState, lParam);
                    return 1;
                }

                if (message == WM_NCLBUTTONDOWN)
                    goto ButtonDownCommon;
                if (message == WM_NCLBUTTONUP)
                    goto Case_LButtonUp;
                if (message == WM_NCLBUTTONDBLCLK)
                    goto Case_LButtonDblClk;

                if (message != WM_NCRBUTTONDOWN)
                    return 0;

                if ((pPopupMenu->flags & 0x40 /* fRightButton */) == 0)
                {
                FindWindowAndMaybeRemove:
                    pMenuState->mnFocus = -1;
                    if (xxxMNFindWindowFromPoint(pPopupMenu, (DWORD)&pPopupMenu, lParam))
                    {
                        if ((pMenuState->flags & 0x100 /* fModelessMenu */) == 0)
                            xxxMNRemoveMessage(pMsg->message, 0);
                        return 1;
                    }
                    goto Dismiss;
                }

            ButtonDownCommon:
                pMenuState->mnFocus = -1;
                pMenuState->ptMouseLast.x = (short)lParam;
                pMenuState->ptMouseLast.y = (short)(lParam >> 16);
                hit = (PWND)xxxMNFindWindowFromPoint(pPopupMenu, (DWORD)&pPopupMenu, lParam);
                bLockedHitWnd = (PMENUSTATE)IsMFMWFPWindow((int)hit);
                if (bLockedHitWnd)
                {
                    tl.next = gptiCurrent->ptl;
                    gptiCurrent->ptl = &tl;
                    tl.pobj = hit;
                    if (hit)
                        hit->head.cLockObj++;
                }

                if (pMenuState->flags & 0x400 /* fDragAndDrop */)
                {
                    pMenuState->ptButtonDown = pMenuState->ptMouseLast;
                    pMenuState->uButtonDownIndex = (UINT)(ULONG_PTR)pPopupMenu;
                    LockMFMWFPWindow((PVOID *)&pMenuState->uButtonDownHitArea, hit);
                }
                if (pMenuState->flags & 0x500 /* fModelessMenu | fDragAndDrop */)
                    pMenuState->vkButtonDown = ((wParam & MK_RBUTTON) != 0) + 1;

                if (!hit && !pPopupMenu)
                    goto Dismiss;

                if ((pPopupMenu->flags & 0x2 /* fHasMenuBar */) && hit == (PWND)-5)
                {
                    xxxMNSwitchToAlternateMenu(pPopupMenu);
                    hit = (PWND)-1;
                }

                if (hit == (PWND)-1)
                    xxxMNButtonDown(pPopupMenu, pMenuState, (UINT)(ULONG_PTR)pPopupMenu, 1);
                else
                    xxxSendMessage(hit, MN_BUTTONDOWN, (WPARAM)pPopupMenu, 0);

                if ((pMenuState->flags & 0x100 /* fModelessMenu */) == 0)
                    xxxMNRemoveMessage(pMsg->message, WM_RBUTTONDOWN);

            UnlockHitAndReturn:
                if (bLockedHitWnd)
                    ThreadUnlock1();
                return 1;
            }

            if (message == WM_NCRBUTTONDBLCLK)
                goto FindWindowAndMaybeRemove;

            // WM_KEYDOWN .. WM_SYSKEYUP range
            if (message == WM_NCXBUTTONDOWN_OR_SIMILAR /* see note below */)
            {
            RouteToActivePopupOrChar:
                if (!pPopupMenu->spwndActivePopup)
                {
                    xxxMNChar(pPopupMenu, pMenuState, wParam);
                    return 1;
                }
            ForwardToActivePopup:
                tl.next = gptiCurrent->ptl;
                gptiCurrent->ptl = &tl;
                tlPobj = pPopupMenu->spwndActivePopup;
                tlPobj->head.cLockObj++;
                xxxSendMessage(pPopupMenu->spwndActivePopup, pMsg->message, wParam, 0);
                goto UnlockHitAndReturn;
            }
            return 0;
        }

        // message == WM_SYSKEYDOWN, falls into the WM_KEYDOWN handling below
KeyDownCommon:
        if ((pMenuState->flags & 0x8 /* fButtonDown */) && wParam != VK_F1)
        {
            if ((pMenuState->flags & 0x80 /* fDragging */) && wParam == VK_ESCAPE)
                pMenuState->flags |= 0x2000;   // fIgnoreButtonUp
            return 1;
        }

        pMenuState->mnFocus = 1;
        if (wParam > VK_ESCAPE)
        {
            if (wParam >= VK_LEFT && (wParam <= VK_DOWN || wParam == VK_F1 || wParam == VK_F10))
                goto SendToActivePopupOrKeyDown;
TranslateOrReturn:
            if ((pMenuState->flags & 0x100 /* fModelessMenu */) == 0)
                xxxTranslateMessage(pMsg, 0);
            return 1;
        }
        if (wParam != VK_ESCAPE && wParam != VK_CANCEL)
        {
            if (wParam == VK_TAB)
            {
                if ((pPopupMenu->flags & 0x1 /* fIsMenuBar */) && !pPopupMenu->spwndActivePopup)
                    goto Dismiss;
                goto TranslateOrReturn;
            }
            if (wParam != VK_RETURN && wParam != VK_MENU)
                goto TranslateOrReturn;
        }
SendToActivePopupOrKeyDown:
        if (!pPopupMenu->spwndActivePopup)
        {
            xxxMNKeyDown(pPopupMenu, pMenuState, wParam);
            return 1;
        }
        goto ForwardToActivePopup;
    }

    // ======================================================================
    // Mouse-button messages (WM_LBUTTONUP and below, then the switch below)
    // ======================================================================
    if (message <= WM_LBUTTONUP)
    {
        if (message != WM_LBUTTONUP)
        {
            if (message == WM_SYSKEYUP)
            {
                // (falls into a small dedicated VK_MENU/VK_F10 early-out, then shares
                //  the WM_CHAR-ish path)
                if (wParam == VK_MENU || wParam == VK_F10)
                    return 1;
                goto RouteToActivePopupOrChar;
            }
            // WM_KEYUP, WM_CHAR, WM_SYSCHAR ... → WM_MOUSEMOVE
            if (message == WM_CHAR)
                return 1;
            if (message == WM_MOUSEMOVE)
                goto MouseMoveCommon;
            if (message == WM_LBUTTONDOWN)
                goto ButtonDownCommon;
            return 0;
        }

    Case_LButtonUp:
        if ((pMenuState->flags & 0x8 /* fButtonDown */) == 0)
            return 1;

        if (pMenuState->flags & 0x400 /* fDragAndDrop */)
        {
            UnlockMFMWFPWindow((PVOID *)&pMenuState->uButtonDownHitArea);
            pMenuState->flags &= ~0x80;   // clear fDragging

            if (pMenuState->flags & 0x2000 /* fIgnoreButtonUp */)
            {
                pMenuState->flags &= ~(0x2000 | 0x8 /* fIgnoreButtonUp, fButtonDown */);
                return 1;
            }
        }

        pMenuState->ptMouseLast.x = (short)lParam;
        pMenuState->ptMouseLast.y = (short)(lParam >> 16);
        hit = (PWND)xxxMNFindWindowFromPoint(pPopupMenu, (DWORD)&pPopupMenu, lParam);
        bLockedHitWnd = (PMENUSTATE)IsMFMWFPWindow((int)hit);
        if (bLockedHitWnd)
        {
            tl.next = gptiCurrent->ptl;
            gptiCurrent->ptl = &tl;
            tlPobj = hit;
            if (hit)
                hit->head.cLockObj++;
        }

        if (pPopupMenu->flags & 0x2 /* fHasMenuBar */)
        {
            if (!hit && !pPopupMenu)
                goto Dismiss;
            if (hit == (PWND)-1)
            {
                if ((pPopupMenu->flags & 0x4 /* fIsSysMenu */) &&
                    (pPopupMenu->flags & 0x80 /* fToggle */))
                {
                Dismiss:
                    xxxMNDismiss(pMenuState);
                    goto UnlockHitAndReturn;
                }
            ButtonUpCommon:
                xxxMNButtonUp(pPopupMenu, pMenuState, (int)(ULONG_PTR)pPopupMenu, 0);
                goto UnlockHitAndReturn;
            }
        }
        else
        {
            if (!hit && !pPopupMenu && (pPopupMenu->flags & 0x200 /* fDropNextPopup */) == 0)
            {
                tl.next = gptiCurrent->ptl;
                gptiCurrent->ptl = &tl;
                tlPobj = pPopupMenu->spwndPopupMenu;
                if (tlPobj)
                    tlPobj->head.cLockObj++;
                xxxSendMessage(pPopupMenu->spwndPopupMenu, MN_CANCELMENUS, 0, 0);
                ThreadUnlock1();
                goto UnlockHitAndReturn;
            }
            pPopupMenu->flags &= ~0x200; // clear fDropNextPopup
            if (hit == (PWND)-1)
                goto ButtonUpCommon;
        }

        if (!hit || hit == (PWND)-5)
            pMenuState->flags &= ~0x48; // clear fButtonDown | fButtonAlwaysDown
        else
            xxxSendMessage(hit, MN_BUTTONUP, (WPARAM)pPopupMenu, lParam);
        goto UnlockHitAndReturn;
    }

    // ======================================================================
    // Remaining WM_L/RBUTTONDBLCLK / WM_RBUTTONDOWN / WM_RBUTTONUP / WM_RBUTTONDBLCLK
    // ======================================================================
    switch (message)
    {
    case WM_LBUTTONDBLCLK:
    Case_LButtonDblClk:
    {
        int found;
        pMenuState->mnFocus = -1;
        found = xxxMNFindWindowFromPoint(pPopupMenu, (DWORD)&pPopupMenu, lParam);
        if (found || pPopupMenu)
        {
            if ((pPopupMenu->flags & 0x2 /* fHasMenuBar */) && found == -5)
            {
                xxxMNSwitchToAlternateMenu(pPopupMenu);
                found = -1;
            }
            if (found == -1)
            {
                xxxMNDoubleClick(pMenuState, pPopupMenu, (UINT)(ULONG_PTR)pPopupMenu);
                return 1;
            }
            tl.next = gptiCurrent->ptl;
            gptiCurrent->ptl = &tl;
            tlPobj = (PWND)found;
            if (found)
                tlPobj->head.cLockObj++;
            xxxSendMessage((PWND)found, MN_DBLCLK, (WPARAM)pPopupMenu, 0);
            goto UnlockHitAndReturn;
        }
        goto Dismiss;
    }

    case WM_RBUTTONDOWN:
        goto ButtonDownCommon; // (shares logic with the right-button-tracking case above)

    case WM_RBUTTONUP:
    Case_RButtonUp:
        if (pPopupMenu->flags & 0x40 /* fRightButton */)
            goto Case_LButtonUp;

        if (pMenuState->flags & 0x8 /* fButtonDown */)
        {
            if ((pMenuState->flags & 0x100 /* fModelessMenu */) == 0)
                xxxMNRemoveMessage(message, 0);
            return 1;
        }

        if (message == WM_RBUTTONUP && (pPopupMenu->flags & 0x800 /* fAutoDismiss */) == 0)
        {
            PMENUWND   pwndActive = (PMENUWND)pPopupMenu->spwndActivePopup;
            PPOPUPMENU ppmActive  = pwndActive ? pwndActive->ppopupmenu : NULL;

            if (ppmActive && (int)ppmActive->posSelectedItem >= 0)
            {
                PWND spwndNotify = ppmActive->spwndNotify;
                tl.next = gptiCurrent->ptl;
                gptiCurrent->ptl = &tl;
                tlPobj = spwndNotify;
                if (spwndNotify)
                    spwndNotify->head.cLockObj++;

                xxxSendMessage(
                    ppmActive->spwndNotify,
                    WM_MENURBUTTONUP,
                    ppmActive->posSelectedItem,
                    (LPARAM)(ppmActive->spmenu ? ppmActive->spmenu->head.head.h : NULL));
                ThreadUnlock1();
            }
        }
        return 0;

    case WM_RBUTTONDBLCLK:
        goto FindWindowAndMaybeRemove;
    }

    return 0;
}

// @implemented
BOOL MNEndMenuStateNotify(PMENUSTATE pMenuState)
{
    PWND spwndNotify = pMenuState->pGlobalPopupMenu->spwndNotify;
    if (!spwndNotify)
        return FALSE;
    PTHREADINFO pti = spwndNotify->head.pti;
    if (pti == pMenuState->ptiMenuStateOwner)
        return FALSE;
    pti->pMenuState = NULL;
    return TRUE;
}

static PMENUSTATE xxxMNAllocMenuState(PTHREADINFO pti1, PTHREADINFO pti2, PPOPUPMENU pPopupMenu)
{
  int v3; // ebx
  PMENUSTATE pMenuState; // esi

  v3 = gdwPUDFlags & 0x2000000;
  if ( (gdwPUDFlags & 0x2000000) != 0 )
  {
    pMenuState = (PMENUSTATE)Win32AllocPoolWithQuota(sizeof(MENUSTATE), 0x746D7355);
    if ( !pMenuState )
      return pMenuState;
  }
  else
  {
    gdwPUDFlags |= 0x2000000u;
    pMenuState = &gMenuState;
    GreSetDCOwnerEx(gMenuState.hdcAni, 0x80000002, 0);
  }
  ++guSFWLockCount;
  memset(pMenuState, 0, 0x60u);
  pMenuState->pGlobalPopupMenu = pPopupMenu;
  pMenuState->ptiMenuStateOwner = pti1;
  pMenuState->pmnsPrev = pti1->pMenuState;
  pti1->pMenuState = pMenuState;
  if ( pti2 != pti1 )
    pti2->pMenuState = pMenuState;
  if ( !v3 )
    return pMenuState;
  pMenuState->hdcAni = 0;
  if ( MNSetupAnimationDC((int)pMenuState) )
    return pMenuState;
  xxxMNEndMenuState(1);
  return 0;
}

void __stdcall xxxMNEndMenuState(BOOL bFree)
{
  PTHREADINFO pti; // edi
  PMENUSTATE pMenuState; // esi
  PMENUSTATE pMenuState2; // eax

  pti = gptiCurrent;
  pMenuState = gptiCurrent->pMenuState;
  if ( !pMenuState->dwLockCount )
  {
    MNEndMenuStateNotify(gptiCurrent->pMenuState);
    if ( pMenuState->pGlobalPopupMenu )
    {
      if ( bFree )
        MNFreePopup(pMenuState->pGlobalPopupMenu);
      else
        BYTE2(pMenuState->pGlobalPopupMenu->flags) &= ~1u;
    }
    UnlockMFMWFPWindow(&pMenuState->uButtonDownHitArea);
    UnlockMFMWFPWindow(&pMenuState->uDraggingHitArea);
    pti->pMenuState = pMenuState->pmnsPrev;
    if ( (pMenuState->flags & 0x100) == 0 )
      --guSFWLockCount;
    if ( pMenuState->hbmAni )
      MNDestroyAnimationBitmap(pMenuState);
    if ( pMenuState == &gMenuState )
    {
      HIBYTE(gdwPUDFlags) &= ~2u;
      GreSetDCOwnerEx(gMenuState.hdcAni, 0, 0);
    }
    else
    {
      if ( pMenuState->hdcAni )
        GreDeleteDC(pMenuState->hdcAni);
      ExFreePoolWithTag(pMenuState, 0);
    }
    pMenuState2 = pti->pMenuState;
    if ( pMenuState2 )
    {
      if ( (pMenuState2->flags & 0x100) != 0 )
        xxxActivateThisWindow(pMenuState2->pGlobalPopupMenu->spwndActivePopup, 0, 0);
      else
        xxxMNSetCapture(pMenuState2->pGlobalPopupMenu);
    }
  }
}

void __stdcall MNFreePopup(PPOPUPMENU pPopupMenu)
{
  PMENUWND pMenuWnd; // eax

  if ( pPopupMenu == pPopupMenu->ppopupmenuRoot )
    MNFlushDestroyedPopups(pPopupMenu, 1);
  pMenuWnd = (PMENUWND)pPopupMenu->spwndPopupMenu;
  if ( pMenuWnd && (pMenuWnd->wnd.fnid & 0x3FFF) == 0x29C && pPopupMenu != &gpopupMenu )
    pMenuWnd->ppopupmenu = 0;
  HMAssignmentUnlock((PVOID *)&pPopupMenu->spwndPopupMenu);
  HMAssignmentUnlock((PVOID *)&pPopupMenu->spwndNextPopup);
  HMAssignmentUnlock((PVOID *)&pPopupMenu->spwndPrevPopup);
  UnlockPopupMenu(pPopupMenu, &pPopupMenu->spmenu);
  UnlockPopupMenu(pPopupMenu, &pPopupMenu->spmenuAlternate);
  HMAssignmentUnlock((PVOID *)&pPopupMenu->spwndNotify);
  HMAssignmentUnlock((PVOID *)&pPopupMenu->spwndActivePopup);
  if ( pPopupMenu == &gpopupMenu )
    BYTE2(gdwPUDFlags) &= ~0x80u;
  else
    ExFreePoolWithTag(pPopupMenu, 0);
}

static BOOL IsInsideMenuLoop(PTHREADINFO pti)
{
    PMENUSTATE pMenuState = pti->pMenuState;
    return pMenuState && (pMenuState->flags & 4) != 0;
}
