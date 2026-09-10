#include "stdafx.h"
#include <WeaselUI.h>
#include <psapi.h>
#include <chrono>
#include <iostream>
#include <stdexcept>
#pragma comment(lib, "psapi.lib")

static double elapsed(std::chrono::steady_clock::time_point start) {
  return std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - start).count();
}
static size_t private_bytes() {
  PROCESS_MEMORY_COUNTERS_EX info = {};
  GetProcessMemoryInfo(GetCurrentProcess(),
      reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&info), sizeof(info));
  return info.PrivateUsage;
}
int main(int argc, char**) {
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  const auto before = private_bytes();
  {
    weasel::UI ui;
    auto& style = ui.style();
    style.layout_type = weasel::UIStyle::LAYOUT_HORIZONTAL;
    style.font_face = L"Segoe UI Emoji:23:23, Segoe UI Emoji:2a:2a, Segoe UI Emoji:30:39, Segoe UI Emoji:fe0f:fe0f, Segoe UI Emoji:20e3:20e3, Segoe UI, Microsoft YaHei, DengXian, Segoe UI Emoji, Noto Color Emoji, Segoe UI Symbol";
    style.label_font_face = style.comment_font_face = L"Microsoft YaHei";
    style.font_point = style.label_font_point = 14;
    style.comment_font_point = 13;
    style.candidate_spacing = 22;
    style.inline_preedit = true;
    weasel::Context context;
    context.preedit.str = L"n";
    for (auto word : {L"你", L"能", L"捏", L"哪", L"那"}) {
      context.cinfo.candies.emplace_back(word);
      context.cinfo.comments.emplace_back(L"");
      context.cinfo.labels.emplace_back(std::to_wstring(context.cinfo.labels.size()+1));
    }
    weasel::Status status;
    status.schema_id = L"rime_ice";
    status.composing = true;
    auto start = std::chrono::steady_clock::now();
    if (!ui.Create(nullptr)) return 2;
    const double create = elapsed(start);
    ui.Update(context, status);
    std::cout << "cold_create_ms=" << create << " cold_first_ui_ms=" << elapsed(start)
              << " private_delta_kib=" << (private_bytes()-before)/1024 << "\n";
    const DWORD gdi = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    const auto warm_bytes = private_bytes();
    const int cycles = argc > 1 ? 1000 : 100;
    for (int i=0; i<cycles; ++i) {
      ui.Destroy();
      ui.ctx() = weasel::Context();
      ui.octx() = weasel::Context();
      ui.status().composing = false;
      if (i==cycles-1) Sleep(1200);
      start=std::chrono::steady_clock::now();
      ui.Create(nullptr);
      ui.Update(context,status);
      if(i==cycles-1) std::cout << "idle_first_ui_ms=" << elapsed(start) << "\n";
    }
    std::cout << "repeat_private_growth_kib="
              << (static_cast<long long>(private_bytes())-warm_bytes)/1024
              << " gdi_growth=" << static_cast<long>(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS))-gdi << "\n";
    if (GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) > gdi + 2)
      throw std::runtime_error("GDI objects leaked across compositions");
    std::cout << "PASS composition cycles=" << cycles << "\n";
    ui.Destroy(true);
  }
  CoUninitialize();
}
