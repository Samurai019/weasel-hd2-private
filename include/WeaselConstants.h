#pragma once

#define WEASEL_CODE_NAME "WeaselHD2"
#define WEASEL_REG_KEY L"Software\\Rime\\WeaselHD2"
#define WEASEL_USER_REG_KEY L"Software\\Rime\\WeaselHD2"
#define WEASEL_UPDATE_REG_KEY L"Software\\Rime\\WeaselHD2\\Updates"
#define WEASEL_TSF_BASENAME L"weaselhd2"
#define WEASEL_SERVER_MUTEX L"(WEASEL-HD2)Furandoru-Sukaretto-"
#define WEASEL_DEPLOYER_MUTEX L"WeaselHD2DeployerMutex"
#define WEASEL_DEPLOYER_EXCLUSIVE_MUTEX L"WeaselHD2DeployerExclusiveMutex"

#define STRINGIZE(x) #x
#define VERSION_STR(x) STRINGIZE(x)
#define WEASEL_VERSION VERSION_STR(VERSION_MAJOR.VERSION_MINOR.VERSION_PATCH)
