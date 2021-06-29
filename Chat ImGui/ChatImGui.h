#pragma once

class ChatImGui
{
private:
	rtdhook*			mChatConstrHook = nullptr;
	rtdhook*			mChatOnLostDeviceHook = nullptr;
	rtdhook*			mChatAddEntryHook = nullptr;
	rtdhook_call*		mChatRenderHook = nullptr;

	rtdhook*			mChatScrollToBottomHook = nullptr;
	rtdhook_call*		mChatDXUTScrollHook = nullptr;
	rtdhook*			mChatPageUpHook = nullptr;
	rtdhook*			mChatPageDownHook = nullptr;

	float				mCurrentFontSize = 18; // WIP, not sure if it's such a needy functionality
	float				mWhereToScroll = 0.f;

public:
	struct chat_line_data_t { ImVec4* color = nullptr; char* text = nullptr; char* timestamp = nullptr; };
	typedef std::vector<chat_line_data_t> chat_line_t;
	std::vector<chat_line_t>	mChatLines;

	void initialize();

	inline rtdhook*			getConstrHook() { return mChatConstrHook; }
	inline rtdhook*			getOnLostDeviceHook() { return mChatOnLostDeviceHook; }
	inline rtdhook*			getAddEntryHook() { return mChatAddEntryHook; }
	inline rtdhook_call*	getDXUTScrollHook() { return mChatDXUTScrollHook; }
	inline rtdhook*			getScrollToBottomHook() { return mChatScrollToBottomHook; }
	inline rtdhook*			getPageUpHook() { return mChatPageUpHook; }
	inline rtdhook*			getPageDownHook() { return mChatPageDownHook; }

	static inline void		pushColorToBuffer(ChatImGui::chat_line_t& line, ImVec4& color);
	static inline void		pushTextToBuffer(ChatImGui::chat_line_t& line, std::string& text);
	static inline void		pushTimestampToBuffer(ChatImGui::chat_line_t& line, std::string& timestamp);

	static void				renderShadow(const char* text__);
	static void				renderLine(ChatImGui::chat_line_t& data);

	void					rebuildFonts();

	inline void				scrollToBottom() { mWhereToScroll = 0.f; }
	inline void				scrollTo(int to) {
		mWhereToScroll += (float)to;
		if (mWhereToScroll > 0.f)
			mWhereToScroll = 0;
		else if (mWhereToScroll < -100.f)
			mWhereToScroll = -100.f;
	}
	inline bool				shouldWeScrollToBottom() { return mWhereToScroll == 0.f; }
	inline float			whereToScroll() { return mWhereToScroll; }
};

inline ChatImGui gChat;

void* __fastcall CChat__CChat(void* ptr, void*, IDirect3DDevice9* pDevice, void* pFontRenderer, const char* pChatLogPath);
int __fastcall CChat__OnLostDevice(void* ptr, void*);
void __fastcall CChat__Render(void* ptr, void*);
void __fastcall CChat__AddEntry(void* ptr, void*, int nType, const char* szText, const char* szPrefix, uint32_t textColor, uint32_t prefixColor);

int __fastcall CDXUTScrollBar__Scroll(uintptr_t ptr, void*, int nDelta);
int __fastcall CChat__ScrollToBottom(void* ptr, void*);
int __fastcall CChat__PageUp(void* ptr, void*);
int __fastcall CChat__PageDown(void* ptr, void*);