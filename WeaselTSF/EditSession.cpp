#include "stdafx.h"
#include "WeaselTSF.h"
#include "CandidateList.h"
#include "ResponseParser.h"

STDAPI WeaselTSF::DoEditSession(TfEditCookie ec) {
  // get commit string from server
  std::wstring commit;
  weasel::Config config;
  auto context = std::make_shared<weasel::Context>();
  weasel::ResponseParser parser(&commit, context.get(), &_status, &config,
                                &_cand->style());

  bool ok = m_client.GetResponseData(std::ref(parser));

  _UpdateLanguageBar(_status);

  if (ok) {
    if (!commit.empty()) {
      // Helldivers 2 Unicode commit proof of concept. The normal TSF text
      // insertion path is intentionally bypassed so the commit cannot be
      // inserted twice.
      const BOOL compositionEnded =
          !_IsComposing() ||
          _EndCompositionSynchronously(ec, _pEditSessionContext, true);
      const BOOL unicodeSent = compositionEnded && _SendUnicodeText(commit);
      if (!unicodeSent) {
        OutputDebugStringW(
            L"Weasel HD2 POC: Unicode commit was not fully inserted.\n");
      }
      _committed = TRUE;
    } else {
      _committed = FALSE;
    }
    if (_status.composing && !_IsComposing()) {
      _StartComposition(_pEditSessionContext,
                        _fCUASWorkaroundEnabled && !config.inline_preedit);
    } else if (!_status.composing && _IsComposing()) {
      _EndComposition(_pEditSessionContext, true);
    }
    if (_IsComposing() && config.inline_preedit) {
      _ShowInlinePreedit(_pEditSessionContext, context);
    }
    _UpdateCompositionWindow(_pEditSessionContext);
  }

  _UpdateUI(*context, _status);

  return TRUE;
}
