// In-game page with the KOF XI training options (port of the recomp's CHEATS menu page).
#include "gui.h"
#include "imgui.h"
#include "gui_util.h"
#include "IconsFontAwesome6.h"
#include "kofxi/kofxi_cheats.h"
#include <cstdio>
#include <string>

static bool combo(const char* label, unsigned& value, const char* const* items, int count)
{
	int v = (int)value;
	bool changed = ImGui::Combo(label, &v, items, count);
	if (changed)
		value = (unsigned)v;
	return changed;
}

void gui_kofxi_cheats()
{
	fullScreenWindow(false);
	ImguiStyleVar _(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::Begin("##kofxi", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar
			| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);
	{
		ImguiStyleVar _(ImGuiStyleVar_FramePadding, ScaledVec2(20, 8));
		ImGui::AlignTextToFramePadding();
		ImGui::Indent(uiScaled(10));
		ImGui::Text("%s", (std::string(ICON_FA_BOLT "  ") + "KOF XI").c_str());
		const char* closeLbl = "Close";
		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(closeLbl).x
				- ImGui::GetStyle().FramePadding.x * 2.f);
		if (ImGui::Button(closeLbl))
			gui_setState(GuiState::Commands);
		ImGui::Unindent(uiScaled(10));
	}
	ImGui::BeginChild(ImGui::GetID("kofxi"), ImVec2(0, 0), ImGuiChildFlags_Borders,
			ImGuiWindowFlags_DragScrolling | ImGuiChildFlags_NavFlattened);
	KofxiCheats& o = kofxiCheats();
	ImGui::TextWrapped("Training options. Not part of the original game; they write the game's RAM every frame.");
	ImGui::Separator();
	for (int side = 0; side < 2; ++side)
	{
		ImguiID id(side == 0 ? "p1" : "p2");
		ImGui::Text("Player %d", side + 1);
		ImGui::Checkbox("Infinite health", &o.health[side]);
		ImGui::Checkbox("Full super meter", &o.super[side]);
		ImGui::Checkbox("Full tag meter", &o.tag[side]);
		ImGui::Checkbox("One-hit KO (1 HP)", side == 0 ? &o.p1_ko : &o.p2_ko);
		ImGui::Separator();
	}
	static const char* const damage[] = {"Off", "x2", "x4", "x8", "x16"};
	static const unsigned damageValue[] = {0, 2, 4, 8, 16};
	int di = 0;
	for (int i = 0; i < 5; ++i)
		if (o.damage1 == damageValue[i])
			di = i;
	if (ImGui::Combo("P2 damage multiplier", &di, damage, 5))
		o.damage1 = damageValue[di];
	ImGui::Checkbox("Unlock all fighters and bosses", &o.unlock);
	ImGui::Checkbox("Same character in a team", &o.same);
	ImGui::Checkbox("Time stop", &o.timestop);
	ImGui::Checkbox("Leader command free (LDM any)", &o.ldm);
	ImGui::Checkbox("CPU stop", &o.cpustop);
	static const char* const judge[] = {"Game decides", "P1 wins on time", "Draw", "P2 wins on time"};
	combo("Time-over result", o.judge, judge, 4);
	int t = (int)o.round_time;
	if (ImGui::SliderInt("Round clock cap (0 = off)", &t, 0, 98))
		o.round_time = (unsigned)t;
	static const char* const stage[] = {"Automatic", "Cambodia", "Stage 1", "Stage 2", "Stage 3", "Stage 4",
			"Stage 5", "Stage 6", "Stage 7", "Boss 2"};
	combo("Stage", o.stage, stage, 10);
	static const char* const vs[] = {"Game settings", "No limit", "10 wins", "20 wins", "30 wins", "Event mode"};
	combo("Versus limit", o.vs_limit, vs, 6);
	ImGui::Separator();
	std::string text = kofxiCheatsToText();
	ImGui::TextWrapped("Active: %s", text.empty() ? "(none)" : text.c_str());
	ImGui::TextWrapped("Tip: save this text in kofxi_cheats.txt to start with these options.");
	if (ImGui::Button("Reset all"))
		o = KofxiCheats{};
	scrollWhenDraggingOnVoid();
	windowDragScroll();
	ImGui::EndChild();
	ImGui::End();
}
