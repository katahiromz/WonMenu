BOOL APIENTRY
NtUserTrackPopupMenuEx(
    _In_ HMENU hMenu,
    _In_ UINT fuFlags,
    _In_ INT x,
    _In_ INT y,
    _In_ HWND hWnd,
    _In_opt_ LPTPMPARAMS lptpm);
BOOL APIENTRY
NtUserThunkedMenuItemInfo(
   HMENU hMenu,
   UINT uItem,
   BOOL fByPosition,
   BOOL bInsert,
   LPMENUITEMINFOW lpmii,
   PUNICODE_STRING lpszCaption);
BOOL APIENTRY
NtUserThunkedMenuInfo(
   HMENU hMenu,
   LPCMENUINFO lpcmi);
BOOL APIENTRY
NtUserSetMenuFlagRtoL(
   HMENU hMenu);
BOOL APIENTRY
NtUserSetMenuDefaultItem(
   HMENU hMenu,
   UINT uItem,
   UINT fByPos);
BOOL APIENTRY
NtUserSetMenuContextHelpId(
   HMENU hMenu,
   DWORD dwContextHelpId);
BOOL APIENTRY
NtUserSetMenu(
   HWND hWnd,
   HMENU Menu,
   BOOL Repaint);
BOOL APIENTRY
NtUserRemoveMenu(
   HMENU hMenu,
   UINT uPosition,
   UINT uFlags);
DWORD
APIENTRY
NtUserPaintMenuBar(
    HWND hWnd,
    HDC hDC,
    ULONG leftBorder,
    ULONG rightBorder,
    ULONG top,
    BOOL bActive);
int APIENTRY
NtUserMenuItemFromPoint(
   HWND hWnd,
   HMENU hMenu,
   DWORD X,
   DWORD Y);
DWORD
APIENTRY
NtUserDrawMenuBarTemp(
   HWND hWnd,
   HDC hDC,
   PRECT pRect,
   HMENU hMenu,
   HFONT hFont);
BOOL APIENTRY
NtUserHiliteMenuItem(
   HWND hWnd,
   HMENU hMenu,
   UINT uItemHilite,
   UINT uHilite);
BOOL APIENTRY
NtUserGetMenuItemRect(
   HWND hWnd,
   HMENU hMenu,
   UINT uItem,
   PRECTL lprcItem);
UINT APIENTRY
NtUserGetMenuIndex(
   HMENU hMenu,
   HMENU hSubMenu);
BOOL APIENTRY
NtUserGetMenuBarInfo(
   HWND hwnd,
   LONG idObject,
   LONG idItem,
   PMENUBARINFO pmbi);

//
BOOL APIENTRY
NtUserEndMenu(VOID);

UINT APIENTRY
NtUserEnableMenuItem(
   HMENU hMenu,
   UINT uIDEnableItem,
   UINT uEnable);

BOOL APIENTRY
NtUserDestroyMenu(
   HMENU hMenu);

BOOL FASTCALL UserDestroyMenu(HMENU hMenu);

BOOLEAN APIENTRY
NtUserGetTitleBarInfo(
    HWND hwnd,
    PTITLEBARINFO bti);
BOOL APIENTRY
NtUserSetSystemMenu(HWND hWnd, HMENU hMenu);
HMENU APIENTRY
NtUserGetSystemMenu(HWND hWnd, BOOL bRevert);
BOOL APIENTRY
NtUserDeleteMenu(
   HMENU hMenu,
   UINT uPosition,
   UINT uFlags);
DWORD APIENTRY
NtUserCheckMenuItem(
   HMENU hMenu,
   UINT uIDCheckItem,
   UINT uCheck);
DWORD
APIENTRY
NtUserCalcMenuBar(
    HWND   hwnd,
    DWORD  leftBorder,
    DWORD  rightBorder,
    DWORD  top,
    LPRECT prc );

BOOL FASTCALL
IntSetMenu(
   PWND Wnd,
   HMENU Menu,
   BOOL *Changed);

BOOL FASTCALL
IntSetSystemMenu(PWND Window, PMENU Menu);
PMENU FASTCALL
IntGetSystemMenu(PWND Window, BOOL bRevert);
PMENU FASTCALL MENU_GetSystemMenu(PWND Window, PMENU Popup);
BOOL FASTCALL
IntGetMenuItemRect(
   PWND pWnd,
   PMENU Menu,
   UINT uItem,
   PRECTL Rect);
BOOL FASTCALL
UserMenuInfo(
   PMENU Menu,
   PROSMENUINFO UnsafeMenuInfo,
   BOOL SetOrGet);
BOOL FASTCALL
UserMenuItemInfo(
   PMENU Menu,
   UINT Item,
   BOOL ByPosition,
   PROSMENUITEMINFO UnsafeItemInfo,
   BOOL SetOrGet,
   PUNICODE_STRING lpstr);
BOOL FASTCALL
IntMenuItemInfo(
   PMENU Menu,
   UINT Item,
   BOOL ByPosition,
   PROSMENUITEMINFO ItemInfo,
   BOOL SetOrGet,
   PUNICODE_STRING lpstr);
HMENU FASTCALL UserCreateMenu(PDESKTOP Desktop, BOOL PopupMenu);
UINT FASTCALL IntFindSubMenu(HMENU *hMenu, HMENU hSubTarget );
HMENU FASTCALL IntGetSubMenu( HMENU hMenu, int nPos);
UINT FASTCALL IntGetMenuState( HMENU hMenu, UINT uId, UINT uFlags);
DWORD FASTCALL
UserInsertMenuItem(
   PMENU Menu,
   UINT uItem,
   BOOL fByPosition,
   LPCMENUITEMINFOW UnsafeItemInfo,
   PUNICODE_STRING lpstr);
BOOLEAN APIENTRY
intGetTitleBarInfo(PWND pWindowObject, PTITLEBARINFO bti);
BOOL FASTCALL
IntHiliteMenuItem(PWND WindowObject,
                  PMENU MenuObject,
                  UINT uItemHilite,
                  UINT uHilite);
BOOL WINAPI
PopupMenuWndProc(
   PWND Wnd,
   UINT Message,
   WPARAM wParam,
   LPARAM lParam,
   LRESULT *lResult);

BOOL FASTCALL
IntTrackPopupMenuEx(
    _Inout_ PMENU menu,
    _In_ UINT wFlags,
    _In_ INT x,
    _In_ INT y,
    _In_ PWND pWnd,
    _In_opt_ const TPMPARAMS *lpTpm);

PMENU FASTCALL
IntGetMenuObject(HMENU hMenu);

PMENU FASTCALL VerifyMenu(PMENU pMenu);

BOOL
FASTCALL
IntIsMenu(HMENU Menu);

PMENU WINAPI
IntGetMenu(HWND hWnd);

BOOL IntDestroyMenu( PMENU pMenu, BOOL bRecurse);

BOOLEAN
UserDestroyMenuObject(PVOID Object);

BOOL FASTCALL
IntDestroyMenuObject(PMENU Menu, BOOL bRecurse);

BOOL
MenuInit(VOID);

BOOL FASTCALL
IntRemoveMenuItem( PMENU pMenu, UINT nPos, UINT wFlags, BOOL bRecurse );

BOOL FASTCALL
IntInsertMenuItem(
    _In_ PMENU MenuObject,
    UINT uItem,
    BOOL fByPosition,
    PROSMENUITEMINFO ItemInfo,
    PUNICODE_STRING lpstr);

PMENU FASTCALL
IntCreateMenu(
    _Out_ PHANDLE Handle,
    _In_ BOOL IsMenuBar,
    _In_ PDESKTOP Desktop,
    _In_ PPROCESSINFO ppi);

BOOL FASTCALL
IntCloneMenuItems(PMENU Destination, PMENU Source);

PMENU FASTCALL
IntCloneMenu(PMENU Source);

BOOL FASTCALL
IntSetMenuFlagRtoL(PMENU Menu);

BOOL FASTCALL
IntSetMenuContextHelpId(PMENU Menu, DWORD dwContextHelpId);

BOOL FASTCALL
IntGetMenuInfo(PMENU Menu, PROSMENUINFO lpmi);

BOOL FASTCALL
IntSetMenuInfo(PMENU Menu, PROSMENUINFO lpmi);

BOOL FASTCALL
IntGetMenuItemInfo(PMENU Menu, /* UNUSED PARAM!! */
                   PITEM MenuItem, PROSMENUITEMINFO lpmii);

BOOL FASTCALL
IntSetMenuItemInfo(PMENU MenuObject, PITEM MenuItem, PROSMENUITEMINFO lpmii, PUNICODE_STRING lpstr);

UINT FASTCALL
IntEnableMenuItem(PMENU MenuObject, UINT uIDEnableItem, UINT uEnable);

DWORD FASTCALL
IntCheckMenuItem(PMENU MenuObject, UINT uIDCheckItem, UINT uCheck);

BOOL FASTCALL
UserSetMenuDefaultItem(PMENU MenuObject, UINT uItem, UINT fByPos);

UINT FASTCALL
IntGetMenuDefaultItem(PMENU MenuObject, UINT fByPos, UINT gmdiFlags, DWORD *gismc);

PMENU
FASTCALL
co_IntGetSubMenu(
  PMENU pMenu,
  int nPos);

INT FASTCALL IntMenuItemFromPoint(PWND pWnd, HMENU hMenu, POINT ptScreen);

LONG
IntGetDialogBaseUnits(VOID);

DWORD WINAPI
IntDrawMenuBarTemp(PWND pWnd, HDC hDC, LPRECT Rect, PMENU pMenu, HFONT Font);
