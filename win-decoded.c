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

    if ( ppMenuState )
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

void __stdcall xxxMNEndMenuState(BOOL bFlag)
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
      if ( bFlag )
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
    if ( pMenuState == (PMENUSTATE)&gMenuState )
    {
      HIBYTE(gdwPUDFlags) &= ~2u;
      GreSetDCOwnerEx(hDC, 0, 0);
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
  bool bNoRTL; // zf
  unsigned int v16; // eax
  int v17; // edi
  int v18; // eax
  PTHREADINFO v19; // eax
  PMENUSTATE pMenuState2; // edi
  INT v22; // esi
  PPOPUPMENU pPopupMenu_1; // esi
  PPOPUPMENU pGlobalPopupMenu; // esi
  struct _WND *spwndNotify; // eax
  PMENUWND spwndActivePopup; // esi
  struct _THREADINFO *ptiWnd; // eax
  PTHREADINFO pti; // ebx
  PPOPUPMENU pPopupMenu; // esi
  int pti3; // [esp-10h] [ebp-58h]
  PTHREADINFO ptiWnd3; // [esp-Ch] [ebp-54h]
  RECT rcExclude; // [esp+8h] [ebp-40h] BYREF
  TL tl2; // [esp+18h] [ebp-30h] BYREF
  TL tl1; // [esp+24h] [ebp-24h] BYREF
  int v36; // [esp+30h] [ebp-18h]
  PTHREADINFO pti2; // [esp+34h] [ebp-14h]
  struct _THREADINFO *ptiWnd2; // [esp+38h] [ebp-10h]
  unsigned __int32 bReturnCmd; // [esp+3Ch] [ebp-Ch]
  int bDown; // [esp+40h] [ebp-8h]
  PMENUSTATE pMenuState3; // [esp+44h] [ebp-4h]
  int menua; // [esp+50h] [ebp+8h]
  PWND pWnda; // [esp+60h] [ebp+18h]

  pMenuState3 = 0;
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
  MNAnimate(pMenuState, 0);
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
    BYTE2(pti->pq->dwMenuFlags) &= ~0x10u;
LABEL_5:
  if ( (wFlags & TPM_RIGHTBUTTON) != 0 )
    KeyState = _GetKeyState(VK_RBUTTON);
  else
    KeyState = _GetKeyState(VK_LBUTTON);
  v8 = menu->fFlags & 0x40000000;
  bDown = (KeyState >> 15) & 1;
  Window = (PMENUWND)xxxCreateWindowEx(
                       385,
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
                       (LPARAM)pWnd->hModule,
                       0,
                       0x502u,
                       0);
  if ( !Window )
    return 0;
  if ( (pWnd->ExStyle & WS_EX_LAYOUTRTL) != 0 || ((unsigned __int16)wFlags & (unsigned __int16)TPM_LAYOUTRTL) != 0 )
    BYTE2(Window->wnd.ExStyle) |= 0x40u;        // WS_EX_LAYOUTRTL
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
  HMAssignmentLock((PVOID *)&pPopupMenu->spwndNotify, pWnd);
  LockPopupMenu(pPopupMenu, &pPopupMenu->spmenu, menu);
  HMAssignmentLock((PVOID *)&pPopupMenu->spwndActivePopup, Window);
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
  pti3 = (int)pti2;
  pPopupMenu->flags ^= (pPopupMenu->flags ^ (wFlags >> 7)) & 4;
  pMenuState3 = (PMENUSTATE)xxxMNAllocMenuState(pti3, (int)ptiWnd3, (int)pPopupMenu);
  if ( !pMenuState3 )
  {
LABEL_68:
    LOBYTE(wFlags) = wFlags | TPM_NONOTIFY;
LABEL_69:
    v22 = 0;
    xxxWindowEvent(5, pWnd, 0, 0, 0);
    xxxMNReleaseCapture();
    if ( (wFlags & TPM_NONOTIFY) == 0 )
      xxxSendMessage(pWnd, WM_EXITMENULOOP, (wFlags & 0x200) == 0, 0);
    pMenuState2 = pMenuState3;
    bReturnCmd = 1;
LABEL_43:
    if ( ThreadUnlock1() && (Window->wnd.state & 0x80000000) == 0 )// WNDS_DESTROYED
      xxxDestroyWindow(&Window->wnd);
    if ( pMenuState2 )
      xxxMNEndMenuState(1);
    goto LABEL_48;
  }
  if ( (pPopupMenu->flags & 0x800) == 0 )
    xxxSendMessage(pWnd, WM_ENTERMENULOOP, (pPopupMenu->flags & 4) == 0, 0);
  if ( !xxxMNStartMenu(pPopupMenu, -1) )
    goto LABEL_69;
  if ( (pMenuState3->flags & 0x400) != 0 )
    xxxClientRegisterDragDrop((char)Window->wnd.head.h);
  if ( (pPopupMenu->flags & 0x800) == 0 )
  {
    v14 = *p_spwndNotify;
    bNoRTL = *p_spwndNotify == 0;
    tl2.next = gptiCurrent->ptl;
    gptiCurrent->ptl = &tl2;
    tl2.pobj = v14;
    if ( !bNoRTL )
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
    pMenuState3->flags |= 0x20u;                // fHierarchyDropped
    menu->fFlags |= 4u;                         // MNF_UNDERLINE
  }
  else
  {
    menu->fFlags &= ~4u;                        // NOT MNF_UNDERLINE
  }
  v17 = (unsigned __int16)v16;
  v18 = HIWORD(v16) + 2 * gpsi->aiSysMet[SM_CYMAXTRACK];
  bDown = v17 + 2 * gpsi->aiSysMet[SM_CXMAXTRACK];
  menua = v18;
  v19 = (PTHREADINFO)_MonitorFromPoint((POINT)__PAIR64__(y, x), 2);
  bNoRTL = (pWnd->ExStyle & WS_EX_LAYOUTRTL) == 0;
  pti2 = v19;
  if ( !bNoRTL && (wFlags & 4) == 0 )
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
  pMenuState2 = pMenuState3;
  xxxSetWindowPos(
    &Window->wnd,
    ((pMenuState3->flags & 0x100) != 0) - 1,
    (__int16)pWnda,
    SHIWORD(pWnda),
    0,
    0,
    ~(16 * BYTE1(pMenuState3->flags)) & 0x10 | 0x241);
  xxxWindowEvent(6, Window, -4, 0, 0);
  pMenuState2->flags = pMenuState2->flags & 0xFFFFFFF7 | (8 * v36);
  v22 = xxxMNLoop(pPopupMenu, pMenuState2, 0, 0);
  if ( (pMenuState2->flags & 0x100) == 0 )      // !fSynchronous
    goto LABEL_43;
  ThreadUnlock1();
LABEL_48:
  if ( bReturnCmd )
    return v22;
  else
    return 1;
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
