#include "stdafx.h"
#include "WeaselTSF.h"
#include "EditSession.h"
#include "ResponseParser.h"
#include "CandidateList.h"
#include <vector>

/* Start Composition */
class CStartCompositionEditSession : public CEditSession {
 public:
  CStartCompositionEditSession(com_ptr<WeaselTSF> pTextService,
                               com_ptr<ITfContext> pContext,
                               BOOL fCUASWorkaroundEnabled,
                               BOOL inlinePreeditEnabled)
      : CEditSession(pTextService, pContext),
        _inlinePreeditEnabled(inlinePreeditEnabled) {
    _fCUASWorkaroundEnabled = fCUASWorkaroundEnabled;
  }

  /* ITfEditSession */
  STDMETHODIMP DoEditSession(TfEditCookie ec);

 private:
  BOOL _fCUASWorkaroundEnabled;
  BOOL _inlinePreeditEnabled;
};

STDAPI CStartCompositionEditSession::DoEditSession(TfEditCookie ec) {
  HRESULT hr = E_FAIL;
  com_ptr<ITfInsertAtSelection> pInsertAtSelection;
  com_ptr<ITfRange> pRangeComposition;
  if (_pContext->QueryInterface(IID_ITfInsertAtSelection,
                                (LPVOID*)&pInsertAtSelection) != S_OK)
    return hr;
  if (pInsertAtSelection->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, NULL, 0,
                                                &pRangeComposition) != S_OK)
    return hr;

  com_ptr<ITfContextComposition> pContextComposition;
  com_ptr<ITfComposition> pComposition;
  if (_pContext->QueryInterface(IID_ITfContextComposition,
                                (LPVOID*)&pContextComposition) != S_OK)
    return hr;
  if ((pContextComposition->StartComposition(
           ec, pRangeComposition, _pTextService, &pComposition) == S_OK) &&
      (pComposition != NULL)) {
    _pTextService->_SetComposition(pComposition);

    /* WORKAROUND:
     *   CUAS does not provide a correct GetTextExt() position unless the
     * composition is filled with characters. So we insert a zero width space
     * here. The workaround is only needed when inline preedit is not enabled.
     *   See https://github.com/rime/weasel/pull/883#issuecomment-1567625762
     */
    if (!_inlinePreeditEnabled) {
      pRangeComposition->SetText(ec, TF_ST_CORRECTION, L" ", 1);
    }

    /* set selection */
    TF_SELECTION tfSelection;
    if (_inlinePreeditEnabled)
      pRangeComposition->Collapse(ec, TF_ANCHOR_END);
    else
      pRangeComposition->Collapse(ec, TF_ANCHOR_START);
    tfSelection.range = pRangeComposition;
    tfSelection.style.ase = TF_AE_NONE;
    tfSelection.style.fInterimChar = FALSE;
    _pContext->SetSelection(ec, 1, &tfSelection);
  }

  return hr;
}

void WeaselTSF::_StartComposition(com_ptr<ITfContext> pContext,
                                  BOOL fCUASWorkaroundEnabled) {
  com_ptr<CStartCompositionEditSession> pStartCompositionEditSession;
  pStartCompositionEditSession.Attach(new CStartCompositionEditSession(
      this, pContext, fCUASWorkaroundEnabled, _cand->style().inline_preedit));
  _cand->StartUI();
  if (pStartCompositionEditSession != nullptr) {
    HRESULT hr;
    pContext->RequestEditSession(_tfClientId, pStartCompositionEditSession,
                                 TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
  }
}

/* End Composition */
class CEndCompositionEditSession : public CEditSession {
 public:
  CEndCompositionEditSession(com_ptr<WeaselTSF> pTextService,
                             com_ptr<ITfContext> pContext,
                             com_ptr<ITfComposition> pComposition,
                             BOOL clear = TRUE)
      : CEditSession(pTextService, pContext), _clear(clear) {
    _pComposition = pComposition;
  }

  /* ITfEditSession */
  STDMETHODIMP DoEditSession(TfEditCookie ec);

 private:
  com_ptr<ITfComposition> _pComposition;
  BOOL _clear;
};

STDAPI CEndCompositionEditSession::DoEditSession(TfEditCookie ec) {
  /* Clear the dummy text we set before, if any. */
  if (_pComposition == nullptr)
    return S_OK;
  // Avoid null pointer dereference
  if (!_pTextService || !_pContext)
    return S_OK;

  _pTextService->_ClearCompositionDisplayAttributes(ec, _pContext);

  com_ptr<ITfRange> pCompositionRange;
  if (_clear && _pComposition->GetRange(&pCompositionRange) == S_OK)
    pCompositionRange->SetText(ec, 0, L"", 0);

  _pComposition->EndComposition(ec);
  if (_pTextService)  // if _pTextService released, skip _FinalizeComposition
    _pTextService->_FinalizeComposition();
  return S_OK;
}

void WeaselTSF::_EndComposition(com_ptr<ITfContext> pContext, BOOL clear) {
  CEndCompositionEditSession* pEditSession;
  HRESULT hr;

  _cand->EndUI();
  if ((pEditSession = new CEndCompositionEditSession(
           this, pContext, _pComposition, clear)) != NULL) {
    pContext->RequestEditSession(_tfClientId, pEditSession,
                                 TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
    pEditSession->Release();
  }
}

BOOL WeaselTSF::_EndCompositionSynchronously(TfEditCookie ec,
                                             com_ptr<ITfContext> pContext,
                                             BOOL clear) {
  if (!_IsComposing())
    return TRUE;
  if (pContext == nullptr || _pComposition == nullptr)
    return FALSE;

  _cand->EndUI();
  _ClearCompositionDisplayAttributes(ec, pContext);

  com_ptr<ITfRange> pCompositionRange;
  if (clear) {
    if (FAILED(_pComposition->GetRange(&pCompositionRange)))
      return FALSE;
    if (FAILED(pCompositionRange->SetText(ec, 0, L"", 0)))
      return FALSE;
  }

  if (FAILED(_pComposition->EndComposition(ec)))
    return FALSE;

  _FinalizeComposition();
  return TRUE;
}

BOOL WeaselTSF::_SendUnicodeText(const std::wstring& text) {
  if (text.empty())
    return TRUE;

  constexpr ULONG_PTR kUnicodeCommitMarker = static_cast<ULONG_PTR>(0x57484432);
  std::vector<INPUT> inputs;
  inputs.reserve(text.size() * 2);

  for (const wchar_t unit : text) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = 0;
    input.ki.wScan = static_cast<WORD>(unit);
    input.ki.dwFlags = KEYEVENTF_UNICODE;
    input.ki.dwExtraInfo = kUnicodeCommitMarker;
    inputs.push_back(input);

    input.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    inputs.push_back(input);
  }

  SetLastError(ERROR_SUCCESS);
  const UINT inserted = ::SendInput(static_cast<UINT>(inputs.size()),
                                    inputs.data(), sizeof(INPUT));
  return inserted == inputs.size();
}

/* Candidate Position Diagnostics */
namespace {

std::wstring Hd2LogRect(const RECT& rc) {
  wchar_t buf[64] = {};
  swprintf_s(buf, L"(%ld,%ld,%ld,%ld)", rc.left, rc.top, rc.right, rc.bottom);
  return std::wstring(buf);
}

std::wstring Hd2LogWindowInfo(HWND hwnd) {
  if (hwnd == NULL)
    return L"null";
  wchar_t cls[128] = {};
  ::GetClassNameW(hwnd, cls, _countof(cls));
  RECT rc = {};
  ::GetWindowRect(hwnd, &rc);
  wchar_t buf[256] = {};
  swprintf_s(buf, L"0x%p cls=%ls rect=%ls", hwnd, cls, Hd2LogRect(rc).c_str());
  return std::wstring(buf);
}

struct Hd2PositionLogData {
  const wchar_t* branch;
  HRESULT textExtResult;
  RECT textExt;
  BOOL fClipped;
  RECT screenExt;
  bool hasViewRect;
  HWND viewWnd;
  HWND foregroundWnd;
  POINT caret;
  bool hasCaret;
  POINT mouse;
  bool invalidRect;
  bool nearViewOrigin;
  RECT out;
};

void Hd2WritePositionLog(const Hd2PositionLogData& data) {
  if (!Hd2CandidateFixLogEnabled())
    return;

  SYSTEMTIME st = {};
  ::GetLocalTime(&st);
  wchar_t ts[64] = {};
  swprintf_s(ts, L"%04u-%02u-%02u %02u:%02u:%02u.%03u", st.wYear, st.wMonth,
             st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

  wchar_t line[1024] = {};
  swprintf_s(line,
             L"%ls pid=%u branch=%ls textExt=%ls hr=0x%08lx clip=%d "
             L"screenExt=%ls hasView=%d view=%ls fg=%ls caret=(%ld,%ld) "
             L"ok=%d mouse=(%ld,%ld) invalid=%d nearOrigin=%d out=%ls",
             ts, ::GetCurrentProcessId(), data.branch,
             Hd2LogRect(data.textExt).c_str(), data.textExtResult,
             data.fClipped ? 1 : 0, Hd2LogRect(data.screenExt).c_str(),
             data.hasViewRect ? 1 : 0, Hd2LogWindowInfo(data.viewWnd).c_str(),
             Hd2LogWindowInfo(data.foregroundWnd).c_str(), data.caret.x,
             data.caret.y, data.hasCaret ? 1 : 0, data.mouse.x, data.mouse.y,
             data.invalidRect ? 1 : 0, data.nearViewOrigin ? 1 : 0,
             Hd2LogRect(data.out).c_str());

  fs::path path = WeaselLogPath() / L"candidate-position.log";
  HANDLE h = ::CreateFileW(path.c_str(), FILE_APPEND_DATA,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE)
    return;
  std::string utf8 = wstring_to_string(line, CP_UTF8) + "\r\n";
  DWORD written = 0;
  ::WriteFile(h, utf8.data(), static_cast<DWORD>(utf8.size()), &written, NULL);
  ::CloseHandle(h);
}

}  // namespace

/* Get Text Extent */
class CGetTextExtentEditSession : public CEditSession {
 public:
  CGetTextExtentEditSession(com_ptr<WeaselTSF> pTextService,
                            com_ptr<ITfContext> pContext,
                            com_ptr<ITfContextView> pContextView,
                            com_ptr<ITfComposition> pComposition,
                            bool enhancedPosition)
      : CEditSession(pTextService, pContext) {
    _pContextView = pContextView;
    _pComposition = pComposition;
    _enhancedPosition = enhancedPosition;
  }

  /* ITfEditSession */
  STDMETHODIMP DoEditSession(TfEditCookie ec);

 private:
  com_ptr<ITfContextView> _pContextView;
  com_ptr<ITfComposition> _pComposition;
  bool _enhancedPosition;
};

STDAPI CGetTextExtentEditSession::DoEditSession(TfEditCookie ec) {
  com_ptr<ITfInsertAtSelection> pInsertAtSelection;
  com_ptr<ITfRange> pRangeComposition;
  ITfRange* pRange;
  RECT rc = {};
  BOOL fClipped;
  TF_SELECTION selection;
  ULONG nSelection;

  if (FAILED(_pContext->QueryInterface(IID_ITfInsertAtSelection,
                                       (LPVOID*)&pInsertAtSelection)))
    return E_FAIL;
  if (FAILED(_pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &selection,
                                     &nSelection)))
    return E_FAIL;

  if (_pComposition != nullptr && _pComposition->GetRange(&pRange) == S_OK) {
    pRange->Collapse(ec, TF_ANCHOR_START);
  } else {
    // composition end
    // note: selection.range is always an empty range
    pRange = selection.range;
  }

  const HRESULT textExtResult =
      _pContextView->GetTextExt(ec, pRange, &rc, &fClipped);

  HWND hwndView = NULL;
  _pContextView->GetWnd(&hwndView);
  HWND hwndForeground = ::GetForegroundWindow();
  POINT ptCaret = {};
  const bool hasCaret = !!::GetCaretPos(&ptCaret);
  POINT ptMouse = {};
  ::GetCursorPos(&ptMouse);

  if (!Hd2CandidateFixEnabled()) {
    // Original upstream behavior: use the reported text extent directly,
    // with the optional enhanced position correction.
    if (SUCCEEDED(textExtResult) && (rc.left != 0 || rc.top != 0)) {
      if (_enhancedPosition) {
        HWND hwnd = ::GetForegroundWindow();
        RECT rcForegroundWindow = {};
        ::GetWindowRect(hwnd, &rcForegroundWindow);

        if (rc.left < rcForegroundWindow.left ||
            rc.left > rcForegroundWindow.right ||
            rc.top < rcForegroundWindow.top ||
            rc.top > rcForegroundWindow.bottom) {
          POINT pt = {};
          bool hasCaret = ::GetCaretPos(&pt);
          int offsetx =
              rcForegroundWindow.left - rc.left + (hasCaret ? pt.x : 0);
          int offsety = rcForegroundWindow.top - rc.top + (hasCaret ? pt.y : 0);
          rc.left += offsetx;
          rc.right += offsetx;
          rc.top += offsety;
          rc.bottom += offsety;
        }
      }
      _pTextService->_SetCompositionPosition(rc);
    }
    if (Hd2CandidateFixLogEnabled()) {
      RECT rcView = {};
      const bool hasViewRect = SUCCEEDED(_pContextView->GetScreenExt(&rcView));
      Hd2WritePositionLog(
          {L"raw", textExtResult, rc, fClipped, rcView, hasViewRect, hwndView,
           hwndForeground, ptCaret, hasCaret, ptMouse,
           FAILED(textExtResult) || (rc.left == 0 && rc.top == 0), false, rc});
    }
    return S_OK;
  }

  RECT rcView = {};
  const bool hasScreenExt = SUCCEEDED(_pContextView->GetScreenExt(&rcView));
  const bool hasViewWidth = rcView.right > rcView.left;
  const bool hasViewHeight = rcView.bottom > rcView.top;
  bool hasViewRect = hasScreenExt && hasViewWidth && hasViewHeight;
  if (!hasViewRect) {
    HWND hwnd = NULL;
    if (SUCCEEDED(_pContextView->GetWnd(&hwnd)) && hwnd != NULL) {
      hasViewRect = !!::GetWindowRect(hwnd, &rcView);
    }
  }
  if (!hasViewRect) {
    HWND hwnd = ::GetForegroundWindow();
    hasViewRect = hwnd != NULL && !!::GetWindowRect(hwnd, &rcView);
  }

  const bool invertedRect = rc.right < rc.left || rc.bottom <= rc.top;
  const bool invalidRect = FAILED(textExtResult) || invertedRect;
  const bool nearViewLeft = abs(rc.left - rcView.left) <= 2;
  const bool nearViewTop = abs(rc.top - rcView.top) <= 2;
  const bool nearViewOrigin = hasViewRect && nearViewLeft && nearViewTop;

  if ((invalidRect || nearViewOrigin) && hasViewRect) {
    // HD2 exposes a TSF text store but reports an empty or top-left text
    // extent for its custom chat control. Anchor the standalone candidate UI
    // near the lower-right chat area using relative coordinates so the
    // fallback works across resolutions and window modes.
    const LONG width = rcView.right - rcView.left;
    const LONG height = rcView.bottom - rcView.top;
    rc.left = rc.right = rcView.left + width * 3 / 4;
    rc.top = rcView.top + height * 4 / 5;
    rc.bottom = rc.top + 2;
  } else if (SUCCEEDED(textExtResult) && _enhancedPosition && hasViewRect &&
             (rc.left < rcView.left || rc.left > rcView.right ||
              rc.top < rcView.top || rc.top > rcView.bottom)) {
    POINT pt = {};
    const bool hasCaret = !!::GetCaretPos(&pt);
    const int offsetx = rcView.left - rc.left + (hasCaret ? pt.x : 0);
    const int offsety = rcView.top - rc.top + (hasCaret ? pt.y : 0);
    ::OffsetRect(&rc, offsetx, offsety);
  }

  const wchar_t* branch = L"direct";
  if ((invalidRect || nearViewOrigin) && hasViewRect)
    branch = L"fallback";
  else if (SUCCEEDED(textExtResult) && _enhancedPosition && hasViewRect &&
           (rc.left < rcView.left || rc.left > rcView.right ||
            rc.top < rcView.top || rc.top > rcView.bottom))
    branch = L"enhanced";

  if (!invalidRect || hasViewRect) {
    _pTextService->_SetCompositionPosition(rc);
  }
  Hd2WritePositionLog({branch, textExtResult, rc, fClipped, rcView, hasViewRect,
                       hwndView, hwndForeground, ptCaret, hasCaret, ptMouse,
                       invalidRect, nearViewOrigin, rc});
  return S_OK;
}

/* Composition Window Handling */
BOOL WeaselTSF::_UpdateCompositionWindow(com_ptr<ITfContext> pContext) {
  com_ptr<ITfContextView> pContextView;
  if (pContext->GetActiveView(&pContextView) != S_OK)
    return FALSE;
  com_ptr<CGetTextExtentEditSession> pEditSession;
  pEditSession.Attach(
      new CGetTextExtentEditSession(this, pContext, pContextView, _pComposition,
                                    _cand->style().enhanced_position));
  if (pEditSession == NULL) {
    return FALSE;
  }
  HRESULT hr;
  pContext->RequestEditSession(_tfClientId, pEditSession,
                               TF_ES_ASYNCDONTCARE | TF_ES_READ, &hr);
  return SUCCEEDED(hr);
}

void WeaselTSF::_SetCompositionPosition(const RECT& rc) {
  /* Test if rect is valid.
   * If it is invalid during CUAS test, we need to apply CUAS workaround */
  if (!_fCUASWorkaroundTested) {
    _fCUASWorkaroundTested = TRUE;
    if (rc.top == rc.bottom) {
      _fCUASWorkaroundEnabled = TRUE;
      return;
    }
  }
  RECT _rc;
  _rc.left = _rc.right = rc.left;
  _rc.top = _rc.bottom = rc.bottom;
  m_client.UpdateInputPosition(rc);
  _cand->UpdateInputPosition(rc);
}

/* Inline Preedit */
class CInlinePreeditEditSession : public CEditSession {
 public:
  CInlinePreeditEditSession(com_ptr<WeaselTSF> pTextService,
                            com_ptr<ITfContext> pContext,
                            com_ptr<ITfComposition> pComposition,
                            const std::shared_ptr<weasel::Context> context)
      : CEditSession(pTextService, pContext),
        _pComposition(pComposition),
        _context(context) {}

  /* ITfEditSession */
  STDMETHODIMP DoEditSession(TfEditCookie ec);

 private:
  com_ptr<ITfComposition> _pComposition;
  const std::shared_ptr<weasel::Context> _context;
};

STDAPI CInlinePreeditEditSession::DoEditSession(TfEditCookie ec) {
  std::wstring preedit = _context->preedit.str;

  com_ptr<ITfRange> pRangeComposition;
  if (_pComposition == nullptr)
    return E_FAIL;
  if ((_pComposition->GetRange(&pRangeComposition)) != S_OK)
    return E_FAIL;

  if ((pRangeComposition->SetText(ec, 0, preedit.c_str(),
                                  static_cast<LONG>(preedit.length()))) != S_OK)
    return E_FAIL;

  /* TODO: Check the availability and correctness of these values */
  int sel_cursor = -1;
  for (size_t i = 0; i < _context->preedit.attributes.size(); i++) {
    if (_context->preedit.attributes.at(i).type == weasel::HIGHLIGHTED) {
      sel_cursor = _context->preedit.attributes.at(i).range.cursor;
      break;
    }
  }

  _pTextService->_SetCompositionDisplayAttributes(ec, _pContext,
                                                  pRangeComposition);

  /* Set caret */
  LONG cch;
  TF_SELECTION tfSelection;
  if (sel_cursor < 0) {
    pRangeComposition->Collapse(ec, TF_ANCHOR_END);
  } else {
    pRangeComposition->Collapse(ec, TF_ANCHOR_START);
    pRangeComposition->ShiftStart(ec, sel_cursor, &cch, NULL);
  }
  tfSelection.range = pRangeComposition;
  tfSelection.style.ase = TF_AE_NONE;
  tfSelection.style.fInterimChar = FALSE;
  _pContext->SetSelection(ec, 1, &tfSelection);

  return S_OK;
}

BOOL WeaselTSF::_ShowInlinePreedit(
    com_ptr<ITfContext> pContext,
    const std::shared_ptr<weasel::Context> context) {
  com_ptr<CInlinePreeditEditSession> pEditSession;
  pEditSession.Attach(
      new CInlinePreeditEditSession(this, pContext, _pComposition, context));
  if (pEditSession != NULL) {
    HRESULT hr;
    pContext->RequestEditSession(_tfClientId, pEditSession,
                                 TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
  }
  return TRUE;
}

/* Update Composition */
class CInsertTextEditSession : public CEditSession {
 public:
  CInsertTextEditSession(com_ptr<WeaselTSF> pTextService,
                         com_ptr<ITfContext> pContext,
                         com_ptr<ITfComposition> pComposition,
                         const std::wstring& text)
      : CEditSession(pTextService, pContext),
        _text(text),
        _pComposition(pComposition) {}

  /* ITfEditSession */
  STDMETHODIMP DoEditSession(TfEditCookie ec);

 private:
  std::wstring _text;
  com_ptr<ITfComposition> _pComposition;
};

STDMETHODIMP CInsertTextEditSession::DoEditSession(TfEditCookie ec) {
  com_ptr<ITfRange> pRange;
  TF_SELECTION tfSelection;
  HRESULT hRet = S_OK;

  if (_pComposition == nullptr)
    return E_FAIL;
  if (FAILED(_pComposition->GetRange(&pRange)))
    return E_FAIL;

  if (FAILED(pRange->SetText(ec, 0, _text.c_str(),
                             static_cast<LONG>(_text.length()))))
    return E_FAIL;

  /* update the selection to an insertion point just past the inserted text. */
  pRange->Collapse(ec, TF_ANCHOR_END);

  tfSelection.range = pRange;
  tfSelection.style.ase = TF_AE_NONE;
  tfSelection.style.fInterimChar = FALSE;

  _pContext->SetSelection(ec, 1, &tfSelection);

  return hRet;
}

BOOL WeaselTSF::_InsertText(com_ptr<ITfContext> pContext,
                            const std::wstring& text) {
  CInsertTextEditSession* pEditSession;
  HRESULT hr;

  if ((pEditSession = new CInsertTextEditSession(this, pContext, _pComposition,
                                                 text)) != NULL) {
    pContext->RequestEditSession(_tfClientId, pEditSession,
                                 TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
    pEditSession->Release();
  }

  return TRUE;
}

void WeaselTSF::_UpdateComposition(com_ptr<ITfContext> pContext) {
  HRESULT hr;

  _pEditSessionContext = pContext;

  _pEditSessionContext->RequestEditSession(
      _tfClientId, this, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
  _async_edit = !!(hr == TF_S_ASYNC);
  _UpdateCompositionWindow(pContext);
}

/* Composition State */
STDAPI WeaselTSF::OnCompositionTerminated(TfEditCookie ecWrite,
                                          ITfComposition* pComposition) {
  // NOTE:
  // This will be called when an edit session ended up with an empty composition
  // string, Even if it is closed normally. Silly M$.

  _AbortComposition();
  return S_OK;
}

void WeaselTSF::_AbortComposition(bool clear) {
  m_client.ClearComposition();
  if (_IsComposing()) {
    _EndComposition(_pEditSessionContext, clear);
  }
  _committed = TRUE;
  _cand->Destroy();
}

void WeaselTSF::_FinalizeComposition() {
  _pComposition = nullptr;
}

void WeaselTSF::_SetComposition(com_ptr<ITfComposition> pComposition) {
  _pComposition = pComposition;
}

BOOL WeaselTSF::_IsComposing() {
  return _pComposition != NULL;
}
