#include "stdafx.h"
#include "Globals.h"

HINSTANCE g_hInst;

LONG g_cRefDll = -1;

CRITICAL_SECTION g_cs;

// {40D0E539-EF5E-45A2-93FF-55FA9A4EC5AE}
const GUID c_clsidTextService = {
    0x40d0e539,
    0xef5e,
    0x45a2,
    {0x93, 0xff, 0x55, 0xfa, 0x9a, 0x4e, 0xc5, 0xae}};

// {3BF30DF8-FF9D-4F69-BD72-1B6B29FAA58E}
const GUID c_guidProfile = {0x3bf30df8,
                            0xff9d,
                            0x4f69,
                            {0xbd, 0x72, 0x1b, 0x6b, 0x29, 0xfa, 0xa5, 0x8e}};

// {A7BA63EC-9889-4E56-ACDC-68DFBF349CC7}
const GUID c_guidLangBarItemButton = {
    0xa7ba63ec,
    0x9889,
    0x4e56,
    {0xac, 0xdc, 0x68, 0xdf, 0xbf, 0x34, 0x9c, 0xc7}};

// {40BAA905-711B-48C1-A90A-9F7BE6FC905E}
const GUID c_guidDisplayAttributeInput = {
    0x40baa905,
    0x711b,
    0x48c1,
    {0xa9, 0x0a, 0x9f, 0x7b, 0xe6, 0xfc, 0x90, 0x5e}};

#ifdef WEASEL_USING_OLDER_TSF_SDK

/* For Windows 8 */
const GUID GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT = {
    0x13A016DF,
    0x560B,
    0x46CD,
    {0x94, 0x7A, 0x4C, 0x3A, 0xF1, 0xE0, 0xE3, 0x5D}};

const GUID GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT = {
    0x25504FB4,
    0x7BAB,
    0x4BC1,
    {0x9C, 0x69, 0xCF, 0x81, 0x89, 0x0F, 0x0E, 0xF5}};

#endif

// {2C77A81E-41CC-4178-A3A7-5F8A987568E6}
// This is the standard TSF input-mode language-bar item GUID. It is a
// Windows language-bar contract, not a product identity, so it must remain
// shared by the standalone and official Weasel profiles.
const GUID GUID_LBI_INPUTMODE = {
    0x2c77a81e,
    0x41cc,
    0x4178,
    {0xa3, 0xa7, 0x5f, 0x8a, 0x98, 0x75, 0x68, 0xe6}};

// {F44F4EED-EB02-4AA3-951F-F5B8B8060B9E}
const GUID GUID_IME_MODE_PRESERVED_KEY = {
    0xf44f4eed,
    0xeb02,
    0x4aa3,
    {0x95, 0x1f, 0xf5, 0xb8, 0xb8, 0x06, 0x0b, 0x9e}};
