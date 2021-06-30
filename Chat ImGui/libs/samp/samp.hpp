#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

enum SAMPVER {
	SAMP_NOT_LOADED,
	SAMP_UNKNOWN,
	SAMP_037_R1,
	SAMP_037_R3_1
};

typedef void(__cdecl* CMDPROC)(const char*);

const uintptr_t samp_addressess[][18]
{
    // CChat, ::CChat, ::OnLostDevice, ::OnResetDevice, ::Draw->call ::Render, getChatFontName, ::AddEntry, ::Scroll->call DXUT__Scroll, ::ScrollToBottom, ::PageUp, ::PageDown, CInput, CInput::AddCommand, CConfig?, CConfig?::SaveVariable, , CConfig?::ReadVariable
    {0x21A0E4, 0x647B0, 0x635D0, 0x64600, 0x642E7, 0xB3D40, 0x64010, 0xB87E7, 0xB3C60, 0x63828, 0x637C0, 0x63700, 0x63760, 0x21A0E8, 0x65AD0, 0x21A0E0, 0x624C0, 0x62250},       // SAMP_037_R1
    {0x26E8C8, 0x67C00, 0x66A20, 0x67A50, 0x67737, 0xC5C00, 0x67460, 0xCA970, 0xC5B20, 0x66C78, 0x66C10, 0x66B50, 0x66BB0, 0x26E8CC, 0x69000, 0x26E8C4, 0x65910, 0x656A0}        // SAMP_037_R3_1
};

const uintptr_t chat_nops[][2]
{
    // nop begin, size
    {0x6428C, 0x4B},
    {0x676DC, 0x4B}
};

inline uintptr_t sampGetBase()
{
    static uintptr_t sampBase = SAMPVER::SAMP_NOT_LOADED;
    if (sampBase == SAMPVER::SAMP_NOT_LOADED)
        sampBase = reinterpret_cast<uintptr_t>(GetModuleHandleA("samp.dll"));
    return sampBase;
}

// https://github.com/imring/TimeFormat/blob/master/samp.hpp#L19

inline SAMPVER sampGetVersion()
{
    static SAMPVER sampVersion = SAMPVER::SAMP_NOT_LOADED;
    if (sampVersion <= SAMPVER::SAMP_UNKNOWN)
	{
        uintptr_t base = sampGetBase();
        if (base == SAMPVER::SAMP_NOT_LOADED) return SAMPVER::SAMP_NOT_LOADED;

        IMAGE_NT_HEADERS* ntHeader = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);

        DWORD ep = ntHeader->OptionalHeader.AddressOfEntryPoint;
        switch (ep)
        {
        case 0x31DF13:
            sampVersion = SAMPVER::SAMP_037_R1;
            break;
        case 0xCC4D0:
            sampVersion = SAMPVER::SAMP_037_R3_1;
            break;
        default:
            sampVersion = SAMPVER::SAMP_UNKNOWN;
        }
    }

    return sampVersion;
}

#define SAMP_OFFSET samp_addressess[sampGetVersion() - 2]

inline uintptr_t sampGetChatInfoPtr()
{
	return *reinterpret_cast<uintptr_t*>(sampGetBase() + SAMP_OFFSET[0]);
}

inline uintptr_t sampGetChatConstr()
{
    return sampGetBase() + SAMP_OFFSET[1];
}

inline uintptr_t sampGetChatOnLostDevice()
{
    return sampGetBase() + SAMP_OFFSET[2];
}

inline uintptr_t sampGetChatOnResetDevice()
{
    return sampGetBase() + SAMP_OFFSET[3];
}

inline uintptr_t sampGetChatRender()
{
    return sampGetBase() + SAMP_OFFSET[4];
}

inline uintptr_t sampGetAddEntryFuncPtr()
{
    return sampGetBase() + SAMP_OFFSET[6];
}

inline const uintptr_t* sampGetChatNopsData()
{
    return chat_nops[sampGetVersion() - 2];
}

inline char* sampGetChatFontName()
{
    return reinterpret_cast<char* (*)()>(sampGetBase() + SAMP_OFFSET[5])();
}

inline float sampGetFontsize()
{
    return static_cast<float>(reinterpret_cast<int(*)()>(sampGetBase() + SAMP_OFFSET[8])());
}

inline uintptr_t sampGetStrftimeFuncPtr()
{
    return sampGetBase() + SAMP_OFFSET[7];
}

inline uintptr_t sampGetChatScrollToBottomFuncPtr()
{
    return sampGetBase() + SAMP_OFFSET[10];
}

inline uintptr_t sampGetChatScrollDXUTScrollCallPtr()
{
    return sampGetBase() + SAMP_OFFSET[9];
}

inline uintptr_t sampGetChatPageUpFuncPtr()
{
    return sampGetBase() + SAMP_OFFSET[11];
}

inline uintptr_t sampGetChatPageDownFuncPtr()
{
    return sampGetBase() + SAMP_OFFSET[12];
}

inline bool sampIsTimestampEnabled()
{
    return *reinterpret_cast<bool*>(sampGetChatInfoPtr() + 0xC);
}

inline int sampGetChatDisplayMode()
{
    return *reinterpret_cast<int*>(sampGetChatInfoPtr() + 0x8);
}

inline uint32_t sampGetPagesize()
{
    return *reinterpret_cast<uint32_t*>(sampGetChatInfoPtr());
}

inline uintptr_t sampGetInputInfoPtr()
{
    return *reinterpret_cast<uintptr_t*>(sampGetBase() + SAMP_OFFSET[13]);
}

inline int sampIsChatInputEnabled()
{
    return *reinterpret_cast<int*>(sampGetInputInfoPtr() + 0x14E0);
}

inline void sampRegisterChatCommand(const char* szName, CMDPROC handler)
{
    reinterpret_cast<void(__thiscall*)(uintptr_t, const char*, CMDPROC)>(sampGetBase() + SAMP_OFFSET[14])
        (sampGetInputInfoPtr(), szName, handler);
}

inline uintptr_t sampGetConfigPtr()
{
    return *reinterpret_cast<uintptr_t*>(sampGetBase() + SAMP_OFFSET[15]);
}

inline void sampSaveVariableToConfig(const char* name, int value)
{
    reinterpret_cast<int(__thiscall*)(uintptr_t, const char*, int, int)>(sampGetBase() + SAMP_OFFSET[16])
        (sampGetConfigPtr(), name, value, 0);
}

inline int sampReadVariableFromConfig(const char* name)
{
    return reinterpret_cast<int(__thiscall*)(uintptr_t, const char*)>(sampGetBase() + SAMP_OFFSET[17])
        (sampGetConfigPtr(), name);
}

#undef SAMP_OFFSET

/*enum EntryType {
    ENTRY_TYPE_NONE = 0,
    ENTRY_TYPE_CHAT = 1 << 1,
    ENTRY_TYPE_INFO = 1 << 2,
    ENTRY_TYPE_DEBUG = 1 << 3
};
enum DisplayMode {
    DISPLAY_MODE_OFF,
    DISPLAY_MODE_NOSHADOW,
    DISPLAY_MODE_NORMAL
};
enum { MAX_MESSAGES = 100 };

#pragma pack(push, 1)
struct CChat_R1 {
    unsigned int          m_nPageSize;
    char* m_szLastMessage;
    int                   m_nMode;
    bool                  m_bTimestamps;
    BOOL                  m_bDoesLogExist;
    char                  m_szLogPath[261]; // MAX_PATH(+1)
    void* m_pGameUi;
    void* m_pEditbox;
    void* m_pScrollbar;
    uint32_t              m_textColor;  // 0xFFFFFFFF
    uint32_t              m_infoColor;  // 0xFF88AA62
    uint32_t              m_debugColor; // 0xFFA9C4E4
    long                  m_nWindowBottom;
    struct ChatEntry {
        __int32  m_timestamp;
        char     m_szPrefix[28];
        char     m_szText[144];
        char     unused[64];
        int      m_nType;
        uint32_t m_textColor;
        uint32_t m_prefixColor;
    };
    ChatEntry             m_entry[MAX_MESSAGES];
    void* m_pFontRenderer;
    void* m_pTextSprite;
    void* m_pSprite;
    void* m_pDevice;
    BOOL                  m_bRenderToSurface;
    void* m_pRenderToSurface;
    void* m_pTexture;
    void* m_pSurface;
#ifdef _d3d9TYPES_H_
    D3DDISPLAYMODE m_displayMode;
#else
    unsigned int m_displayMode[4];
#endif
    int  pad_[2];
    BOOL m_bRedraw;
    long m_nScrollbarPos;
    long m_nCharHeight; // this is the height of the "Y"
    long m_nTimestampWidth;
};

#pragma pack(push, 1)
struct CChat_R3 {
    unsigned int    m_nPageSize;
    char* m_szLastMessage;
    int             m_nMode;
    bool            m_bTimestamps;
    BOOL            m_bDoesLogExist;
    char            m_szLogPath[261]; // MAX_PATH(+1)
    void* m_pGameUi;
    void* m_pEditbox;
    void* m_pScrollbar;
    uint32_t        m_textColor;  // 0xFFFFFFFF
    uint32_t        m_infoColor;  // 0xFF88AA62
    uint32_t        m_debugColor; // 0xFFA9C4E4
    long            m_nWindowBottom;

    struct ChatEntry {
        __int32  m_timestamp;
        char     m_szPrefix[28];
        char     m_szText[144];
        char     unused[64];
        int      m_nType;
        uint32_t m_textColor;
        uint32_t m_prefixColor;
    };
    ChatEntry m_entry[MAX_MESSAGES];

    void* m_pFontRenderer;
    void* m_pTextSprite;
    void* m_pSprite;
    void* m_pDevice;
    BOOL         m_bRenderToSurface;
    void* m_pRenderToSurface;
    void* m_pTexture;
    void* m_pSurface;
#ifdef _d3d9TYPES_H_
    D3DDISPLAYMODE m_displayMode;
#else
    unsigned int m_displayMode[4];
#endif
    int  pad_[2];
    BOOL m_bRedraw;
    long m_nScrollbarPos;
    long m_nCharHeight; // this is the height of the "Y"
    long m_nTimestampWidth;
};

#pragma pack(pop)*/
