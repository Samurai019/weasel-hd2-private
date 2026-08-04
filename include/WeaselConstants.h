#pragma once

#define WEASEL_CODE_NAME "WeaselHD2"
#define WEASEL_REG_KEY L"Software\\Rime\\WeaselHD2"
#define WEASEL_USER_REG_KEY L"Software\\Rime\\WeaselHD2"
#define WEASEL_UPDATE_REG_KEY L"Software\\Rime\\WeaselHD2\\Updates"

// HD2 behavior switches (DWORD, 1 = enabled). Both default to enabled;
// the in-process TSF DLL reads them on every use so changes take effect
// immediately, no redeploy required.
#define WEASEL_HD2_REG_VALUE_UNICODE_COMMIT L"Hd2UnicodeCommit"
#define WEASEL_HD2_REG_VALUE_CANDIDATE_FIX L"Hd2CandidateFix"
// Diagnostics only, default disabled: logs caret/candidate positions on
// every candidate window reposition to
// %TEMP%\rime.weasel.hd2\candidate-position.log
#define WEASEL_HD2_REG_VALUE_CANDIDATE_FIX_LOG L"Hd2CandidateFixLog"
#define WEASEL_TSF_BASENAME L"weaselhd2"
#define WEASEL_SERVER_MUTEX L"(WEASEL-HD2)Furandoru-Sukaretto-"
#define WEASEL_DEPLOYER_MUTEX L"WeaselHD2DeployerMutex"
#define WEASEL_DEPLOYER_EXCLUSIVE_MUTEX L"WeaselHD2DeployerExclusiveMutex"

#define STRINGIZE(x) #x
#define VERSION_STR(x) STRINGIZE(x)
#define WEASEL_VERSION VERSION_STR(VERSION_MAJOR.VERSION_MINOR.VERSION_PATCH)
