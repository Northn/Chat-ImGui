#include "pch.h"
#include "ChatImGui.h"
#include "snippets.hpp"

void ChatImGui::initialize()
{
	mChatConstrHook = new rtdhook(sampGetChatConstr(), &CChat__CChat);
	mChatOnLostDeviceHook = new rtdhook(sampGetChatOnLostDevice(), &CChat__OnLostDevice, 9);
	mChatAddEntryHook = new rtdhook(sampGetAddEntryFuncPtr(), &CChat__AddEntry);
	mChatRenderHook = new rtdhook_call(sampGetChatRender(), &CChat__Render);

	mChatDXUTScrollHook = new rtdhook_call(sampGetChatScrollDXUTScrollCallPtr(), &CDXUTScrollBar__Scroll);
	mChatScrollToBottomHook = new rtdhook(sampGetChatScrollToBottomFuncPtr(), &CChat__ScrollToBottom, 7);
	mChatPageUpHook = new rtdhook(sampGetChatPageUpFuncPtr(), &CChat__PageUp, 6);
	mChatPageDownHook = new rtdhook(sampGetChatPageDownFuncPtr(), &CChat__PageDown, 6);

	{ // Nops
		DWORD old_protection;
		const uintptr_t* nops = sampGetChatNopsData();
		const uintptr_t address = sampGetBase() + nops[1];
		VirtualProtect((LPVOID)address, nops[2], PAGE_EXECUTE_READWRITE, &old_protection);

		memset((void*)address, 0x90, nops[2]);

		VirtualProtect((LPVOID)address, nops[2], old_protection, nullptr);
	} // Nops

	mChatConstrHook->install();
	mChatOnLostDeviceHook->install();
	mChatRenderHook->install();
	mChatAddEntryHook->install();

	mChatDXUTScrollHook->install();
	mChatScrollToBottomHook->install();
	mChatPageUpHook->install();
	mChatPageDownHook->install();
}

void ChatImGui::rebuildFonts()
{
	std::string fontPath(256, '\0');

	if (SHGetSpecialFolderPathA(GetActiveWindow(), (LPSTR)(fontPath.c_str()), CSIDL_FONTS, false))
	{
		fontPath.resize(fontPath.find('\0'));
		auto fontName = std::string(sampGetChatFontName());
		if (fontName == "Arial")
			fontName += "Bd";
		fontPath += "\\" + fontName + ".ttf";
		auto io = ImGui::GetIO();

		ImVector<ImWchar> ranges;
		ImFontGlyphRangesBuilder builder;
		builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
		builder.AddText(u8"‚„…†‡ˆ‰‹‘’“”•–—™›¹");
		builder.BuildRanges(&ranges);

		mCurrentFontSize = sampGetFontsize();
		io.Fonts->AddFontFromFileTTF(fontPath.c_str(), mCurrentFontSize - 2, nullptr, ranges.Data);
		io.Fonts->Build();
	}
}

void* __fastcall CChat__CChat(void* ptr, void*, IDirect3DDevice9* pDevice, void* pFontRenderer, const char* pChatLogPath)
{
	void* CChat = reinterpret_cast<void* (__thiscall*)(void*, IDirect3DDevice9*, void*, const char*)>(gChat.getConstrHook()->getTrampoline())
		(ptr, pDevice, pFontRenderer, pChatLogPath);

	ImGui::CreateContext();
	ImGui_ImplWin32_Init(GetActiveWindow());
	ImGui_ImplDX9_Init(pDevice);

	gChat.rebuildFonts();

	return CChat;
}

int __fastcall CChat__OnLostDevice(void* ptr, void*)
{
	ImGui_ImplDX9_InvalidateDeviceObjects();

	return reinterpret_cast<int(__thiscall*)(void*)>(gChat.getOnLostDeviceHook()->getTrampoline())(ptr);
}

void ChatImGui::renderOutline(const char* text__)
{
	bool shadows = *reinterpret_cast<int*>(sampGetChatInfoPtr() + 0x8) == 2;

	if (shadows)
	{
		auto pos = ImGui::GetCursorPos();
		ImGui::SetCursorPos(ImVec2(pos.x + 1, pos.y));
		ImGui::TextColored(ImVec4(0, 0, 0, 1), text__);
		ImGui::SetCursorPos(ImVec2(pos.x - 1, pos.y));
		ImGui::TextColored(ImVec4(0, 0, 0, 1), text__);
		ImGui::SetCursorPos(ImVec2(pos.x, pos.y + 1));
		ImGui::TextColored(ImVec4(0, 0, 0, 1), text__);
		ImGui::SetCursorPos(ImVec2(pos.x, pos.y - 1));
		ImGui::TextColored(ImVec4(0, 0, 0, 1), text__);
		ImGui::SetCursorPos(pos);
	}
}

void ChatImGui::renderLine(ChatImGui::chat_line_t& data)
{
	ImVec4 color(1, 1, 1, 1);
	for (auto& line : data)
	{
		if (line.color != nullptr)
			color = *(line.color);
		else if (line.timestamp != nullptr)
		{
			if (*reinterpret_cast<bool*>(sampGetChatInfoPtr() + 12))
			{
				ChatImGui::renderOutline(line.timestamp);
				ImGui::TextColored(color, line.timestamp);
				ImGui::SameLine(0.0f, 5.0f);
			}
		}
		else
		{
			ChatImGui::renderOutline(line.text);
			ImGui::TextColored(color, line.text);
			ImGui::SameLine(0.0f, 0.0f);
		}
	}
	ImGui::NewLine();
}

void __fastcall CChat__AddEntry(void* ptr, void*, int nType, const char* szText, const char* szPrefix, uint32_t textColor, uint32_t prefixColor)
{
	std::string text(szText);
	ChatImGui::chat_line_t output;

	uint8_t r_ = ((textColor >> 16) & 0xff), g_ = ((textColor >> 8) & 0xff), b_ = (textColor & 0xff);
	ImVec4 color((float)r_ / 255.f, (float)g_ / 255, (float)b_ / 255, 1);
	ChatImGui::pushColorToBuffer(output, color);

	{
		char time_[64];
		std::time_t t = std::time(nullptr);

		if (reinterpret_cast<size_t(*)(char*, size_t, char const*, struct tm const*)>(sampGetStrftimeFuncPtr())
			(time_, sizeof(time_), "[%H:%M:%S]", std::localtime(&t))
		) {
			std::string time__(time_);
			ChatImGui::pushTimestampToBuffer(output, time__);
		}
	}

	if (nType == 2)
	{
		uint8_t r_ = ((prefixColor >> 16) & 0xff), g_ = ((prefixColor >> 8) & 0xff), b_ = (prefixColor & 0xff);
		ImVec4 colorPrefix((float)r_ / 255.f, (float)g_ / 255, (float)b_ / 255, 1);
		ChatImGui::pushColorToBuffer(output, colorPrefix);
		std::string prefix(szPrefix);
		prefix += ": ";
		ChatImGui::pushTextToBuffer(output, prefix);
		ChatImGui::pushColorToBuffer(output, color);
	}

	std::regex regex("\\{([a-fA-F0-9]{6})\\}");
	std::smatch color_match;

	bool found = std::regex_search(text, color_match, regex);

	auto a = color_match.position();
	auto b = a + 7;

	while (found)
	{
		auto t = text.substr(0, a);
		if (t.length() > 0)
		{
			auto t_ = cp1251_to_utf8(t.c_str());
			ChatImGui::pushTextToBuffer(output, t_);
		}

		const auto& clr = color_match[1];
		{
			auto clr_ = stoi(clr.str(), nullptr, 16);
			uint8_t r = ((clr_ >> 16) & 0xff), g = ((clr_ >> 8) & 0xff), b = (clr_ & 0xff);
			color = ImVec4((float)r / 255.f, (float)g / 255, (float)b / 255, 1);
			ChatImGui::pushColorToBuffer(output, color);
		}

		text = text.substr(b + 1);

		found = std::regex_search(text, color_match, regex);

		a = color_match.position();
		b = a + 7;
	}
	if (text.length() > 0)
	{
		auto t_ = cp1251_to_utf8(text.c_str());
		ChatImGui::pushTextToBuffer(output, t_);
	}
	gChat.mChatLines.push_back(output);

	reinterpret_cast<void(__thiscall*)(void*, uint32_t, const char*, const char*, uint32_t, uint32_t)>(gChat.getAddEntryHook()->getTrampoline())
		(ptr, nType, szText, szPrefix, textColor, prefixColor);
}

void __fastcall CChat__Render(void* ptr, void*)
{
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::SetNextWindowPos(ImVec2(36, 5));
	ImGui::SetNextWindowSize(ImVec2(4096, (ImGui::GetTextLineHeightWithSpacing() * *reinterpret_cast<uint32_t*>(ptr) /* pagesize */) - 9));
	if (ImGui::Begin("Chat ImGui", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar))
	{
		static ImGuiListClipper clipper;
		clipper.Begin(gChat.mChatLines.size());
		while (clipper.Step())
		{
			for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
			{
				ChatImGui::renderLine(gChat.mChatLines[row]);
			}
		}
		if (gChat.shouldWeScrollToBottom())
		{
			float current_scroll = ImGui::GetScrollY(), max_scroll = ImGui::GetScrollMaxY();
			if (max_scroll - current_scroll > 150)
				ImGui::SetScrollY(current_scroll + 10);
			else if (max_scroll - current_scroll > 0)
				ImGui::SetScrollY(current_scroll + 5);
			else if (max_scroll - current_scroll < 0)
				ImGui::SetScrollY(max_scroll);
		}
		else
			ImGui::SetScrollY(ImGui::GetScrollMaxY() + (ImGui::GetTextLineHeightWithSpacing() * gChat.whereToScroll()));

		ImGui::End();
	}
	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}

void ChatImGui::pushColorToBuffer(ChatImGui::chat_line_t& line, ImVec4& color)
{
	line.push_back({ new ImVec4(color) });
}

void ChatImGui::pushTextToBuffer(ChatImGui::chat_line_t& line, std::string& text)
{
	char* out = new char[text.length() + 1];
	text.copy(out, text.length());
	out[text.length()] = '\0';
	line.push_back({ nullptr, out });
}

void ChatImGui::pushTimestampToBuffer(ChatImGui::chat_line_t& line, std::string& timestamp)
{
	char* out = new char[timestamp.length() + 1];
	timestamp.copy(out, timestamp.length());
	out[timestamp.length()] = '\0';
	line.push_back({ nullptr, nullptr, out });
}

int __fastcall CDXUTScrollBar__Scroll(uintptr_t ptr, void*, int nDelta)
{
	int ret = reinterpret_cast<int(__thiscall*)(uintptr_t, int)>(gChat.getDXUTScrollHook()->getHookedFunctionAddress())(ptr, nDelta);
	gChat.scrollTo(nDelta);

	return ret;
}

int __fastcall CChat__ScrollToBottom(void* ptr, void*)
{
	gChat.scrollToBottom();

	return reinterpret_cast<int(__thiscall*)(void*)>(gChat.getScrollToBottomHook()->getTrampoline())(ptr);
}

int __fastcall CChat__PageUp(void* ptr, void*)
{
	gChat.scrollTo(-15);

	return reinterpret_cast<int(__thiscall*)(void*)>(gChat.getPageUpHook()->getTrampoline())(ptr);
}

int __fastcall CChat__PageDown(void* ptr, void*)
{
	gChat.scrollTo(15);

	return reinterpret_cast<int(__thiscall*)(void*)>(gChat.getPageDownHook()->getTrampoline())(ptr);
}
