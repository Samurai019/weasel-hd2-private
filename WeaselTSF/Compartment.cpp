#include "stdafx.h"
#include "WeaselTSF.h"
#include "Compartment.h"
#include <resource.h>
#include <functional>
#include "ResponseParser.h"
#include "CandidateList.h"
#include "LanguageBar.h"

STDAPI CCompartmentEventSink::QueryInterface(REFIID riid,
                                             _Outptr_ void** ppvObj) {
  if (ppvObj == nullptr)
    return E_INVALIDARG;

  *ppvObj = nullptr;

  if (IsEqualIID(riid, IID_IUnknown) ||
      IsEqualIID(riid, IID_ITfCompartmentEventSink)) {
    *ppvObj = (CCompartmentEventSink*)this;
  }

  if (*ppvObj) {
    AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

STDAPI_(ULONG) CCompartmentEventSink::AddRef() {
  return ++_refCount;
}

STDAPI_(ULONG) CCompartmentEventSink::Release() {
  LONG cr = --_refCount;

  assert(_refCount >= 0);

  if (_refCount == 0) {
    delete this;
  }

  return cr;
}

STDAPI CCompartmentEventSink::OnChange(_In_ REFGUID guidCompartment) {
  return _callback(guidCompartment);
}

HRESULT CCompartmentEventSink::_Advise(_In_ com_ptr<IUnknown> punk,
                                       _In_ REFGUID guidCompartment) {
  HRESULT hr = S_OK;
  ITfCompartmentMgr* pCompartmentMgr = nullptr;
  ITfSource* pSource = nullptr;

  hr = punk->QueryInterface(IID_ITfCompartmentMgr, (void**)&pCompartmentMgr);
  if (FAILED(hr)) {
    return hr;
  }

  hr = pCompartmentMgr->GetCompartment(guidCompartment, &_compartment);
  if (SUCCEEDED(hr)) {
    hr = _compartment->QueryInterface(IID_ITfSource, (void**)&pSource);
    if (SUCCEEDED(hr)) {
      hr = pSource->AdviseSink(IID_ITfCompartmentEventSink, this, &_cookie);
      pSource->Release();
    }
  }

  pCompartmentMgr->Release();

  return hr;
}
HRESULT CCompartmentEventSink::_Unadvise() {
  HRESULT hr = S_OK;
  ITfSource* pSource = nullptr;

  hr = _compartment->QueryInterface(IID_ITfSource, (void**)&pSource);
  if (SUCCEEDED(hr)) {
    hr = pSource->UnadviseSink(_cookie);
    pSource->Release();
  }

  _compartment = nullptr;
  _cookie = 0;

  return hr;
}

// State-only diagnostics: never record key codes, preedit, or committed text.
void WeaselTSF::_Hd2LogInputState(const wchar_t* event, ITfContext* context) {
  if (!Hd2SwitchEnabled(WEASEL_HD2_REG_VALUE_INPUT_STATE_LOG, false) ||
      !Hd2IsGameProcess()) {
    _hd2LastInputState.clear();
    return;
  }

  com_ptr<ITfDocumentMgr> document;
  com_ptr<ITfContext> focused;
  const HRESULT focusHr = _pThreadMgr->GetFocus(&document);
  HRESULT topHr = E_FAIL;
  if (document)
    topHr = document->GetTop(&focused);
  ITfContext* inspected = context ? context : focused.p;
  TF_STATUS status = {};
  const HRESULT statusHr = inspected ? inspected->GetStatus(&status) : E_FAIL;
  auto readFlag = [](IUnknown* owner, REFGUID guid) -> LONG {
    com_ptr<ITfCompartmentMgr> manager;
    com_ptr<ITfCompartment> compartment;
    CComVariant value;
    if (!owner || FAILED(owner->QueryInterface(&manager)) ||
        FAILED(manager->GetCompartment(guid, &compartment)) ||
        compartment->GetValue(&value) != S_OK || value.vt != VT_I4)
      return -1;  // Unavailable is distinct from a reported zero.
    return value.lVal;
  };
  com_ptr<ITfContextView> view;
  HWND viewWindow = nullptr;
  if (inspected && SUCCEEDED(inspected->GetActiveView(&view)) && view)
    view->GetWnd(&viewWindow);
  GUITHREADINFO gui = {sizeof(GUITHREADINFO)};
  const BOOL guiOk = GetGUIThreadInfo(GetCurrentThreadId(), &gui);
  wchar_t state[1536] = {};
  swprintf_s(
      state,
      L"event=%ls doc=%p focusCtx=%p eventCtx=%p focusHr=%08lx topHr=%08lx "
      L"disabled=%ld empty=%ld open=%ld statusHr=%08lx dynamic=%08lx "
      L"static=%08lx view=%p fg=%p guiOk=%d guiFocus=%p caret=%p "
      L"composing=%d ascii=%d closable=%d blocked=%d",
      event, document.p, focused.p, context, focusHr, topHr,
      readFlag(inspected, GUID_COMPARTMENT_KEYBOARD_DISABLED),
      readFlag(inspected, GUID_COMPARTMENT_EMPTYCONTEXT),
      readFlag(_pThreadMgr, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE), statusHr,
      status.dwDynamicFlags, status.dwStaticFlags, viewWindow,
      GetForegroundWindow(), guiOk, gui.hwndFocus, gui.hwndCaret,
      _IsComposing(), _status.ascii_mode ? 1 : 0, _isToOpenClose,
      _IsKeyboardDisabled());
  if (_hd2LastInputState == state)
    return;

  SYSTEMTIME now = {};
  GetLocalTime(&now);
  wchar_t line[1792] = {};
  swprintf_s(line, L"%04u-%02u-%02u %02u:%02u:%02u.%03u pid=%lu tid=%lu %ls",
             now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute,
             now.wSecond, now.wMilliseconds, GetCurrentProcessId(),
             GetCurrentThreadId(), state);
  const auto path = WeaselLogPath() / L"input-state.log";
  HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return;
  const std::string utf8 = wstring_to_string(line, CP_UTF8) + "\r\n";
  DWORD written = 0;
  if (WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()), &written,
                nullptr) &&
      written == utf8.size())
    _hd2LastInputState = state;
  CloseHandle(file);
}

BOOL WeaselTSF::_IsKeyboardDisabled() {
  ITfCompartmentMgr* pCompMgr = NULL;
  ITfDocumentMgr* pDocMgrFocus = NULL;
  ITfContext* pContext = NULL;
  BOOL fDisabled = FALSE;

  if ((_pThreadMgr->GetFocus(&pDocMgrFocus) != S_OK) ||
      (pDocMgrFocus == NULL)) {
    fDisabled = TRUE;
    goto Exit;
  }

  if ((pDocMgrFocus->GetTop(&pContext) != S_OK) || (pContext == NULL)) {
    fDisabled = TRUE;
    goto Exit;
  }

  if (pContext->QueryInterface(IID_ITfCompartmentMgr, (void**)&pCompMgr) ==
      S_OK) {
    ITfCompartment* pCompartmentDisabled;
    ITfCompartment* pCompartmentEmptyContext;

    /* Check GUID_COMPARTMENT_KEYBOARD_DISABLED */
    if (pCompMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_DISABLED,
                                 &pCompartmentDisabled) == S_OK) {
      CComVariant var;
      if (pCompartmentDisabled->GetValue(&var) == S_OK) {
        if (var.vt == VT_I4)  // Even VT_EMPTY, GetValue() can succeed
          fDisabled = fDisabled || (var.lVal != 0);
      }
      pCompartmentDisabled->Release();
    }

    /* Check GUID_COMPARTMENT_EMPTYCONTEXT */
    if (pCompMgr->GetCompartment(GUID_COMPARTMENT_EMPTYCONTEXT,
                                 &pCompartmentEmptyContext) == S_OK) {
      CComVariant var;
      if (pCompartmentEmptyContext->GetValue(&var) == S_OK) {
        if (var.vt == VT_I4)  // Even VT_EMPTY, GetValue() can succeed
          fDisabled = fDisabled || (var.lVal != 0);
      }
      pCompartmentEmptyContext->Release();
    }
    pCompMgr->Release();
  }

Exit:
  if (pContext)
    pContext->Release();
  if (pDocMgrFocus)
    pDocMgrFocus->Release();
  return fDisabled;
}

BOOL WeaselTSF::_IsKeyboardOpen() {
  com_ptr<ITfCompartmentMgr> pCompMgr;
  BOOL fOpen = FALSE;

  if (_pThreadMgr->QueryInterface(&pCompMgr) == S_OK) {
    com_ptr<ITfCompartment> pCompartment;
    if (pCompMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,
                                 &pCompartment) == S_OK) {
      VARIANT var;
      if (pCompartment->GetValue(&var) == S_OK) {
        if (var.vt == VT_I4)  // Even VT_EMPTY, GetValue() can succeed
          fOpen = (BOOL)var.lVal;
      }
    }
  }
  return fOpen;
}

HRESULT WeaselTSF::_SetKeyboardOpen(BOOL fOpen) {
  HRESULT hr = E_FAIL;
  com_ptr<ITfCompartmentMgr> pCompMgr;

  if (_pThreadMgr->QueryInterface(&pCompMgr) == S_OK) {
    ITfCompartment* pCompartment;
    if (pCompMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,
                                 &pCompartment) == S_OK) {
      VARIANT var;
      var.vt = VT_I4;
      var.lVal = fOpen;
      hr = pCompartment->SetValue(_tfClientId, &var);
    }
  }

  return hr;
}

HRESULT WeaselTSF::_GetCompartmentDWORD(DWORD& value, const GUID guid) {
  HRESULT hr = E_FAIL;
  com_ptr<ITfCompartmentMgr> pComMgr;
  if (_pThreadMgr->QueryInterface(&pComMgr) == S_OK) {
    ITfCompartment* pCompartment;
    if (pComMgr->GetCompartment(guid, &pCompartment) == S_OK) {
      VARIANT var;
      if (pCompartment->GetValue(&var) == S_OK) {
        if (var.vt == VT_I4)
          value = var.lVal;
        else
          hr = S_FALSE;
      }
    }
    pCompartment->Release();
  }
  return hr;
}

HRESULT WeaselTSF::_SetCompartmentDWORD(const DWORD& value, const GUID guid) {
  HRESULT hr = S_OK;
  com_ptr<ITfCompartmentMgr> pComMgr;
  if (_pThreadMgr->QueryInterface(&pComMgr) == S_OK) {
    ITfCompartment* pCompartment;
    if (pComMgr->GetCompartment(guid, &pCompartment) == S_OK) {
      VARIANT var;
      var.vt = VT_I4;
      var.lVal = value;
      hr = pCompartment->SetValue(_tfClientId, &var);
    }
    pCompartment->Release();
  }
  return hr;
}

BOOL WeaselTSF::_InitCompartment() {
  using namespace std::placeholders;

  auto callback = std::bind(&WeaselTSF::_HandleCompartment, this, _1);
  _pKeyboardCompartmentSink = new CCompartmentEventSink(callback);
  if (!_pKeyboardCompartmentSink)
    return FALSE;
  DWORD hr = _pKeyboardCompartmentSink->_Advise(
      (IUnknown*)_pThreadMgr, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);

  _pConvertionCompartmentSink = new CCompartmentEventSink(callback);
  if (!_pConvertionCompartmentSink)
    return FALSE;
  hr = _pConvertionCompartmentSink->_Advise(
      (IUnknown*)_pThreadMgr, GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION);
  return SUCCEEDED(hr);
}

void WeaselTSF::_UninitCompartment() {
  if (_pKeyboardCompartmentSink) {
    _pKeyboardCompartmentSink->_Unadvise();
    _pKeyboardCompartmentSink = NULL;
  }
  if (_pConvertionCompartmentSink) {
    _pConvertionCompartmentSink->_Unadvise();
    _pConvertionCompartmentSink = NULL;
  }
}

HRESULT WeaselTSF::_HandleCompartment(REFGUID guidCompartment) {
  _Hd2LogInputState(L"compartment-change");
  if (IsEqualGUID(guidCompartment, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE)) {
    if (_isToOpenClose) {
      BOOL isOpen = _IsKeyboardOpen();
      // clear composition when close keyboard
      if (!isOpen && _pEditSessionContext) {
        m_client.ClearComposition();
        _EndComposition(_pEditSessionContext, true);
      }
      _EnableLanguageBar(isOpen);
      _UpdateLanguageBar(_status);
    } else {
      _status.ascii_mode = !_status.ascii_mode;
      _SetKeyboardOpen(true);
      if (_pLangBarButton && _pLangBarButton->IsLangBarDisabled())
        _EnableLanguageBar(true);
      _HandleLangBarMenuSelect(_status.ascii_mode
                                   ? ID_WEASELTRAY_ENABLE_ASCII
                                   : ID_WEASELTRAY_DISABLE_ASCII);
      if (_pEditSessionContext)
        m_client.ClearComposition();
      _UpdateLanguageBar(_status);
    }
  } else if (IsEqualGUID(guidCompartment,
                         GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION)) {
    BOOL isOpen = _IsKeyboardOpen();
    if (isOpen) {
      weasel::ResponseParser parser(NULL, NULL, &_status, NULL,
                                    &_cand->style());
      bool ok = m_client.GetResponseData(std::ref(parser));
      _UpdateLanguageBar(_status);
    }
  }
  return S_OK;
}
