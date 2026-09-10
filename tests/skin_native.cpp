#include "stdafx.h"
#include "WeaselPanel.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <algorithm>

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

// Exercises the real layout, paint and mouse handlers in an isolated window.
// It does not register an IME, modify the user's configuration or inject keys.
struct WeaselSkinTestAccess {
  static void verify_live_change(WeaselPanel& panel, const std::wstring& source,
                                 const std::wstring& directory) {
    const auto active = directory + L"\\hot-reload.ini";
    require(CopyFileW(source.c_str(), active.c_str(), FALSE), "copy activation failed");
    panel.m_skin.LoadFile(active, true);
    panel.Refresh();
    require(panel.m_skinActive, "live-change initial skin unavailable");
    const auto pending = directory + L"\\hot-reload-next.ini";
    {
      const wchar_t disabled[] = L"\xfeff[skin]\r\nmanifest=\r\n";
      std::ofstream stream(pending, std::ios::binary);
      stream.write(reinterpret_cast<const char*>(disabled), sizeof(disabled)-sizeof(wchar_t));
      require(stream.good(), "write activation failed");
    }
    require(MoveFileExW(pending.c_str(), active.c_str(), MOVEFILE_REPLACE_EXISTING),
            "atomic activation failed");
    panel.SendMessage(WeaselPanel::SkinChangedMessage());
    require(!panel.m_skinActive, "skin-change notification did not disable cached skin");
    require(CopyFileW(source.c_str(), active.c_str(), FALSE), "restore activation failed");
    panel.SendMessage(WeaselPanel::SkinChangedMessage());
    require(panel.m_skinActive, "skin-change notification did not restore skin");
    std::cout << "PASS live skin switch without restarting candidate window\n";
  }
  static void prepare(WeaselPanel& panel, const std::wstring& active, UINT dpi) {
    panel.m_style = panel.m_configStyle;
    panel.m_skin.LoadFile(active);
    require(panel.m_skin.Available(), "skin load failed");
    panel.m_skinActive = true;
    panel.m_skin.ApplyStyle(panel.m_style);
    panel.dpi = dpi;
    panel.dpiScaleLayout = dpi / 96.0f;
    panel.pDWR = std::make_shared<weasel::DirectWriteResources>(panel.m_style, dpi);
    panel.m_candidateCount = static_cast<BYTE>(panel.m_ctx.cinfo.candies.size());
    panel.hide_candidates = false;
    panel.m_istorepos = false;
    panel._CreateLayout();
    CDCHandle dc = panel.GetDC();
    panel.m_layout->DoLayout(dc, panel.pDWR);
    panel.ReleaseDC(dc);
    auto& skin = panel.m_skin;
    auto scale = [&](int value) { return int(value * panel.dpiScaleLayout); };
    static_cast<weasel::StandardLayout*>(panel.m_layout)->AddSkinInsets(
        scale(skin.inset_left), scale(skin.inset_top), scale(skin.inset_right),
        scale(skin.inset_bottom), scale(skin.width), scale(skin.height));
    panel._ResizeWindow();
  }

  static void verify(WeaselPanel& panel) {
    auto size = panel.m_layout->GetContentSize();
    for (int i = 0; i < panel.m_candidateCount; ++i) {
      const auto rect = panel.m_layout->GetCandidateRect(i);
      const auto text = panel.m_layout->GetCandidateTextRect(i);
      require(rect.left >= 0 && rect.top >= 0 && rect.right <= size.cx &&
              rect.bottom <= size.cy, "candidate rectangle out of bounds");
      require(text.left >= rect.left && text.right <= rect.right,
              "text outside candidate hit region");
      size_t selected = SIZE_MAX;
      panel.m_ctx.cinfo.highlighted = i;
      panel._UICallback = [&](size_t* index, size_t*, bool*, bool*) {
        if (index) selected = *index;
      };
      BOOL handled = FALSE;
      const auto center = rect.CenterPoint();
      panel.OnLeftClickedUp(WM_LBUTTONUP, 0, MAKELPARAM(center.x, center.y), handled);
      require(selected == static_cast<size_t>(i), "mouse selected wrong candidate");
    }
    panel.m_ctx.cinfo.highlighted = 0;
    panel._UICallback = {};
  }

  static void verify_disabled(WeaselPanel& panel, const std::wstring& missing) {
    panel.m_skin.LoadFile(missing);
    require(!panel.m_skin.Available(), "missing activation did not disable skin");
    panel.Refresh();
    require(!panel.m_skinActive, "regular renderer did not recover");
    require(panel.m_style.font_point == panel.m_configStyle.font_point,
            "imported font leaked into regular configuration");
  }

  static void benchmark(WeaselPanel& panel, weasel::UI& ui) {
    std::vector<double> samples;
    for (int i = 0; i < 220; ++i) {
      ui.style().client_caps = (i % 2) ? weasel::INLINE_PREEDIT_CAPABLE : 0;
      ui.ctx().preedit.str = (i % 2) ? L"ni hao" : L"ni";
      auto start = std::chrono::steady_clock::now();
      panel.Refresh();
      auto ms = std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - start).count();
      if (i >= 20) samples.push_back(ms);
    }
    std::sort(samples.begin(), samples.end());
    std::cout << "BENCH refresh + paint (200 samples): median_ms="
              << samples[100] << " p95_ms=" << samples[190] << "\n";
  }

  static void verify_resource_reuse(WeaselPanel& panel, weasel::UI& ui) {
    panel.Refresh();
    auto target = panel.pDWR->pRenderTarget;
    auto format = panel.pDWR->pTextFormat;
    require(panel.pDWR->pPreeditTextFormat == format,
            "identical preedit and candidate formats were duplicated");
    require(panel.pDWR->pCommentTextFormat == panel.pDWR->pLabelTextFormat,
            "identical comment and label formats were duplicated");
    const int spacing = ui.style().candidate_spacing;
    ui.style().candidate_spacing += 1;
    panel.Refresh();
    require(panel.pDWR->pRenderTarget == target && panel.pDWR->pTextFormat == format,
            "layout-only changes rebuilt text resources");
    ui.style().candidate_spacing = spacing;
    const auto font = ui.style().font_face;
    ui.style().font_face = L"Segoe UI";
    panel.Refresh();
    require(panel.pDWR->pRenderTarget == target && panel.pDWR->pTextFormat != format,
            "font change did not reuse target and rebuild text formats");
    ui.style().font_face = font;
    panel.Refresh();
    panel.RedrawWindow();
    auto dc = panel.m_hCachedMemDC;
    auto bitmap = panel.m_hCachedMemBitmap;
    panel.DestroyWindow();
    require(panel.m_hCachedMemDC == dc && panel.m_hCachedMemBitmap == bitmap,
            "composition teardown discarded reusable backbuffer");
    require(panel.Create(nullptr) != nullptr, "window recreation failed");
    std::cout << "PASS text resources and backbuffer reuse across compositions\n";
  }
};

void save_bitmap(HBITMAP handle, const std::wstring& path) {
  DIBSECTION section = {};
  require(GetObject(handle, sizeof(section), &section) != 0, "GetObject failed");
  const auto* pixels = static_cast<unsigned char*>(section.dsBm.bmBits);
  require(pixels != nullptr, "not a DIB");
  size_t opaque = 0, transparent = 0;
  for (int y = 0; y < section.dsBm.bmHeight; ++y)
    for (int x = 0; x < section.dsBm.bmWidth; ++x) {
      auto alpha = pixels[y * section.dsBm.bmWidthBytes + x * 4 + 3];
      opaque += alpha > 0;
      transparent += alpha == 0;
    }
  require(opaque > 100 && transparent > 100, "alpha composition failed");
  BITMAPFILEHEADER header = {};
  header.bfType = 0x4d42;
  header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
  auto info = section.dsBmih;
  info.biHeight = -section.dsBm.bmHeight;
  info.biSizeImage = section.dsBm.bmWidthBytes * section.dsBm.bmHeight;
  header.bfSize = header.bfOffBits + info.biSizeImage;
  std::ofstream stream(path, std::ios::binary);
  stream.write(reinterpret_cast<const char*>(&header), sizeof(header));
  stream.write(reinterpret_cast<const char*>(&info), sizeof(info));
  stream.write(reinterpret_cast<const char*>(pixels), info.biSizeImage);
  require(stream.good(), "snapshot write failed");
}

int wmain(int argc, wchar_t** argv) {
  try {
    require(argc == 3, "usage: skin_native.exe ACTIVE_INI OUTPUT_DIRECTORY");
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    weasel::UI ui;
    auto& style = ui.style();
    style.layout_type = weasel::UIStyle::LAYOUT_HORIZONTAL;
    style.font_face = style.label_font_face = style.comment_font_face = L"Microsoft YaHei";
    style.font_point = style.label_font_point = style.comment_font_point = 16;
    style.candidate_spacing = 22;
    style.hilite_spacing = 6;
    style.text_color = style.candidate_text_color = 0xff333333;
    style.back_color = 0xffffffff;
    ui.status().schema_id = L"rime_ice";
    ui.status().composing = true;
    ui.ctx().preedit.str = L"ni hao";
    for (auto text : {L"你好", L"拟好", L"您好", L"你", L"好"}) {
      ui.ctx().cinfo.candies.emplace_back(text);
      ui.ctx().cinfo.comments.emplace_back(L"");
      ui.ctx().cinfo.labels.emplace_back(std::to_wstring(ui.ctx().cinfo.labels.size() + 1));
    }
    {
      WeaselPanel panel(ui);
      require(panel.Create(nullptr) != nullptr, "window creation failed");
      for (UINT dpi : {96, 144, 192}) {
        WeaselSkinTestAccess::prepare(panel, argv[1], dpi);
        WeaselSkinTestAccess::verify(panel);
        HBITMAP bitmap = nullptr;
        panel.DoPaint(nullptr, &bitmap);
        require(bitmap != nullptr, "paint failed");
        save_bitmap(bitmap, std::wstring(argv[2]) + L"\\native-" + std::to_wstring(dpi) + L".bmp");
        DeleteObject(bitmap);
        std::cout << "PASS native paint, alpha and candidate clicks at DPI " << dpi << "\n";
      }
      ui.ctx().cinfo.candies[0].str = L"这是用于验证长候选词布局的测试文本";
      WeaselSkinTestAccess::prepare(panel, argv[1], 96);
      WeaselSkinTestAccess::verify(panel);
      HBITMAP long_bitmap = nullptr;
      panel.DoPaint(nullptr, &long_bitmap);
      require(long_bitmap != nullptr, "long candidate paint failed");
      save_bitmap(long_bitmap, std::wstring(argv[2]) + L"\\native-long.bmp");
      DeleteObject(long_bitmap);
      WeaselSkinTestAccess::benchmark(panel, ui);
      WeaselSkinTestAccess::verify_resource_reuse(panel, ui);
      WeaselSkinTestAccess::verify_live_change(panel, argv[1], argv[2]);
      // Exercise the production fallback after an imported skin is disabled.
      WeaselSkinTestAccess::verify_disabled(panel, std::wstring(argv[2]) + L"\\nonexistent-active.ini");
      std::cout << "PASS long candidates and disabled-skin fallback\n";
      panel.DestroyWindow();
    }
    CoUninitialize();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << "\n";
    return 1;
  }
}
