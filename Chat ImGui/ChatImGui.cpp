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
		void* address = reinterpret_cast<void*>(sampGetBase() + nops[0]);
		VirtualProtect(address, nops[1], PAGE_EXECUTE_READWRITE, &old_protection);

		memset(address, 0x90, nops[1]);

		VirtualProtect(address, nops[1], old_protection, nullptr);
	} // Nops

	mChatConstrHook->install();
	mChatOnLostDeviceHook->install();
	mChatRenderHook->install();
	mChatAddEntryHook->install();

	mChatDXUTScrollHook->install();
	mChatScrollToBottomHook->install();
	mChatPageUpHook->install();
	mChatPageDownHook->install();

	std::thread([&]
		{
			uintptr_t sampChatInput = sampGetInputInfoPtr();
			while (!sampChatInput)
				sampChatInput = sampGetInputInfoPtr();

			sampRegisterChatCommand("alphachat", CMDPROC__AlphaChat);

			mChatAlphaEnabled = sampReadVariableFromConfig("alphachat");
		}
	).detach();

}

void ChatImGui::rebuildFonts()
{
	std::string fontPath(256, '\0');

	if (SHGetSpecialFolderPathA(GetActiveWindow(), fontPath.data(), CSIDL_FONTS, false))
	{
		fontPath.resize(fontPath.find('\0'));
		std::string fontName{ sampGetChatFontName() };
		if (fontName == "Arial")
			fontName += "Bd";
		fontPath += "\\" + fontName + ".ttf";

		auto& io = ImGui::GetIO();
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

void ChatImGui::renderOutline(const char* text__)
{

	if (sampGetChatDisplayMode() == 2) // If outlines are enabled
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

void ChatImGui::renderText(ImVec4& color, void* data, bool isTimestamp)
{
	auto text = reinterpret_cast<char*>(data);
	ChatImGui::renderOutline(text);
	ImGui::TextColored(color, text);
	ImGui::SameLine(0.0f, (isTimestamp ? 5.0f : 0.0f));
}

void ChatImGui::renderLine(ChatImGui::chat_line_t& data)
{
	ImVec4 color(1, 1, 1, 1);
	for (auto& line : data)
	{
		switch (line.type)
		{
		case eLineMetadataType::COLOR:
			color = *reinterpret_cast<ImVec4*>(line.data);
			break;
		case eLineMetadataType::TIMESTAMP:
			if (sampIsTimestampEnabled())
				renderText(color, line.data, true);
			break;
		case eLineMetadataType::TEXT:
			renderText(color, line.data);
		}
	}
	ImGui::NewLine();
}

void ChatImGui::pushColorToBuffer(ChatImGui::chat_line_t& line, ImVec4& color)
{
	line.push_back({ eLineMetadataType::COLOR, new ImVec4(color) });
}

void ChatImGui::pushTextToBuffer(ChatImGui::chat_line_t& line, std::string_view text, bool isTimestamp)
{
	char* out = new char[text.length() + 1];
	auto len = text.copy(out, text.length());
	out[len] = '\0';
	line.push_back({ (isTimestamp ? eLineMetadataType::TIMESTAMP : eLineMetadataType::TEXT), out });
}

void ChatImGui::pushTimestampToBuffer(ChatImGui::chat_line_t& line, std::string_view timestamp)
{
	pushTextToBuffer(line, timestamp, true);
}

void* __fastcall CChat__CChat(void* ptr, void*, IDirect3DDevice9* pDevice, void* pFontRenderer, const char* pChatLogPath)
{
	ImGui::CreateContext();
	ImGui_ImplWin32_Init(GetActiveWindow());
	ImGui_ImplDX9_Init(pDevice);

	gChat.rebuildFonts();

	return reinterpret_cast<void* (__thiscall*)(void*, IDirect3DDevice9*, void*, const char*)>(gChat.getConstrHook()->getTrampoline())
		(ptr, pDevice, pFontRenderer, pChatLogPath);
}

void __fastcall CChat__AddEntry(void* ptr, void*, int nType, const char* szText, const char* szPrefix, uint32_t textColor, uint32_t prefixColor)
{
	std::string text(szText);
	ChatImGui::chat_line_t output;

	uint8_t r_ = ((textColor >> 16) & 0xff), g_ = ((textColor >> 8) & 0xff), b_ = (textColor & 0xff);
	ImVec4 color(r_ / 255.f, g_ / 255.f, b_ / 255.f, 1);
	ChatImGui::pushColorToBuffer(output, color);

	{
		std::string time_(64, '\0');
		std::time_t t = std::time(nullptr);

		if (reinterpret_cast<decltype(std::strftime)*>(sampGetStrftimeFuncPtr())
			(time_.data(), time_.size() + 1, "[%H:%M:%S]", std::localtime(&t))
		) {
			time_.resize(time_.find('\0'));
			ChatImGui::pushTimestampToBuffer(output, time_);
		}
	}

	if (nType == 2)
	{
		uint8_t r_ = ((prefixColor >> 16) & 0xff), g_ = ((prefixColor >> 8) & 0xff), b_ = (prefixColor & 0xff);
		ImVec4 colorPrefix(r_ / 255.f, g_ / 255.f, b_ / 255.f, 1);
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
			auto t_ = cp1251_to_utf8(t);
			ChatImGui::pushTextToBuffer(output, t_);
		}

		const auto& clr = color_match[1];
		{
			auto clr_ = stoi(clr.str(), nullptr, 16);
			uint8_t r = ((clr_ >> 16) & 0xff), g = ((clr_ >> 8) & 0xff), b = (clr_ & 0xff);
			color = ImVec4(r / 255.f, g / 255.f, b / 255.f, 1);
			ChatImGui::pushColorToBuffer(output, color);
		}

		text = text.substr(b + 1);

		found = std::regex_search(text, color_match, regex);

		a = color_match.position();
		b = a + 7;
	}
	if (text.length() > 0)
	{
		auto t_ = cp1251_to_utf8(text);
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
	ImGui::SetNextWindowSize(ImVec2(4096, (ImGui::GetTextLineHeightWithSpacing() * sampGetPagesize()) - 9));
	if (gChat.isChatAlphaEnabled())
	{
		auto& alpha = ImGui::GetStyle().Alpha;
		if (!sampIsChatInputEnabled())
		{
			if (alpha >= 0.8f)
				alpha -= 0.005f;
		}
		else
		{
			if (alpha < 1.f)
				alpha += 0.005f;
		}
	}
	if (ImGui::Begin("Chat ImGui", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar))
	{
		static ImGuiListClipper clipper;
		clipper.Begin(gChat.mChatLines.size());
		while (clipper.Step())
			for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
				ChatImGui::renderLine(gChat.mChatLines[row]);

		if (gChat.shouldWeScrollToBottom())
		{
			float current_scroll = ImGui::GetScrollY(), max_scroll = ImGui::GetScrollMaxY();
			if (max_scroll - current_scroll > 300.f)
				ImGui::SetScrollY(current_scroll + 30.f);
			else if (max_scroll - current_scroll > 150.f)
				ImGui::SetScrollY(current_scroll + 10.f);
			else if (max_scroll - current_scroll > 0.f)
				ImGui::SetScrollY(current_scroll + 5.f);
			else if (max_scroll - current_scroll < 0.f)
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

int __fastcall CChat__OnLostDevice(void* ptr, void*)
{
	ImGui_ImplDX9_InvalidateDeviceObjects();

	return reinterpret_cast<int(__thiscall*)(void*)>(gChat.getOnLostDeviceHook()->getTrampoline())(ptr);
}

int __fastcall CDXUTScrollBar__Scroll(void* ptr, void*, int nDelta)
{
	gChat.scrollTo(nDelta);

	return reinterpret_cast<int(__thiscall*)(void*, int)>(gChat.getDXUTScrollHook()->getHookedFunctionAddress())(ptr, nDelta);
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

void CMDPROC__AlphaChat(const char* text)
{
	gChat.switchChatAlphaState();
	bool enabled = gChat.isChatAlphaEnabled();
	std::string text_("-> Chat alpha ");
	text_ += enabled ? "enabled" : "disabled";
	CChat__AddEntry(reinterpret_cast<void*>(sampGetChatInfoPtr()), nullptr, (1 << 2), text_.c_str(), nullptr, 0xFF88AA62, 0);

	sampSaveVariableToConfig("alphachat", enabled);

	if (!enabled)
		ImGui::GetStyle().Alpha = 1.f;
}
