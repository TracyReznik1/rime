#pragma once

#include <memory>
#include <string>
#include <gdiplus.h>
#include <WeaselIPCData.h>

// Static H1 skins imported by tools/import_sogou_skin.py. This deliberately
// consumes a small manifest, never a ZIP or arbitrary Sogou extension code.
class SogouSkin {
 public:
  ~SogouSkin() { Reset(); }

  void Invalidate() { checked_ = false; stamp_ = {}; }

  void Reset() {
    bitmap_.reset();
    ClearCache();
  }

  void Load() {
    if (active_file_.empty()) {
      wchar_t directory[32768] = {};
      DWORD bytes = sizeof(directory);
      if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Rime\\Weasel",
                       L"RimeUserDir", RRF_RT_REG_SZ, nullptr, directory,
                       &bytes) != ERROR_SUCCESS) {
        if (!GetEnvironmentVariableW(L"APPDATA", directory, 32768)) {
          Reset();
          return;
        }
        wcscat_s(directory, L"\\Rime");
      }
      active_file_ = std::wstring(directory) + L"\\sogou-skin.ini";
    }
    LoadFile(active_file_);
  }

  // Also used by the isolated native renderer tests; no registry changes.
  void LoadFile(const std::wstring& active, bool force = false) {
    DWORD now = ::GetTickCount();
    const bool same_file = active == active_file_;
    if (!force && same_file && checked_ && (now - last_stat_tick_ < 1000)) {
      return;
    }
    checked_ = true;
    last_stat_tick_ = now;
    active_file_ = active;
    WIN32_FILE_ATTRIBUTE_DATA info = {};
    if (!GetFileAttributesExW(active.c_str(), GetFileExInfoStandard, &info)) {
      Reset();
      stamp_ = {};
      return;
    }
    if (!force && same_file && CompareFileTime(&stamp_, &info.ftLastWriteTime) == 0 && bitmap_)
      return;
    stamp_ = info.ftLastWriteTime;
    Reset();
    wchar_t path[32768] = {};
    GetPrivateProfileStringW(L"skin", L"manifest", L"", path, 32768,
                             active.c_str());
    const std::wstring manifest(path);
    auto slash = manifest.find_last_of(L"\\/");
    if (slash == std::wstring::npos)
      return;
    auto number = [&](const wchar_t* key, int fallback = -1) {
      return static_cast<int>(GetPrivateProfileIntW(L"skin", key, fallback,
                                                    manifest.c_str()));
    };
    if (number(L"version") != 1)
      return;
    // The filename is fixed; do not interpret image paths from the manifest.
    const std::wstring image = manifest.substr(0, slash + 1) + L"background.png";
    std::unique_ptr<Gdiplus::Bitmap> bitmap(
        Gdiplus::Bitmap::FromFile(image.c_str()));
    if (!bitmap || bitmap->GetLastStatus() != Gdiplus::Ok)
      return;
    width = bitmap->GetWidth();
    height = bitmap->GetHeight();
    left = number(L"left"); right = number(L"right");
    top = number(L"top"); bottom = number(L"bottom");
    inset_left = number(L"inset_left");
    inset_right = number(L"inset_right");
    inset_top = number(L"inset_top");
    inset_bottom = number(L"inset_bottom");
    spacing_ = number(L"spacing");
    font_ = number(L"font_point");
    for (int value : {left, right, top, bottom, inset_left, inset_right,
                      inset_top, inset_bottom, spacing_}) {
      if (value < 0 || value > 8192)
        return;
    }
    if (width <= 0 || height <= 0 || width > 8192 || height > 8192 ||
        static_cast<long long>(width) * height > 16000000 ||
        left + right >= width || top + bottom >= height ||
        font_ < 6 || font_ > 72)
      return;
    text_ = static_cast<COLORREF>(number(L"text_color", 0xff333333));
    candidate_ = static_cast<COLORREF>(number(L"candidate_text_color", 0xff333333));
    selected_ = static_cast<COLORREF>(number(L"hilited_candidate_text_color", 0xff993366));
    bitmap_ = std::move(bitmap);
    // Build at the actual window dimensions on first paint. A guessed size
    // immediately misses the cache (and can be smaller than the fixed corners).
  }

  bool Available() const { return bitmap_ != nullptr; }

  void ApplyStyle(weasel::UIStyle& style) const {
    style.inline_preedit = false;
    style.font_point = font_;
    style.label_font_point = font_;
    style.comment_font_point = font_;
    style.spacing = spacing_;
    style.shadow_radius = 0;
    style.border = 0;
    style.shadow_color = style.border_color = style.hilited_shadow_color = 0;
    // Inner native margins remain small; image decoration insets are added
    // after layout, so candidate text and mouse hit regions stay aligned.
    style.margin_x = style.margin_y = 0;
    style.hilite_padding_x = style.hilite_padding_y = 0;
    style.text_color = style.hilited_text_color = text_;
    style.candidate_text_color = style.label_text_color = candidate_;
    style.comment_text_color = candidate_;
    style.hilited_candidate_text_color = style.hilited_label_text_color = selected_;
    style.hilited_comment_text_color = selected_;
    style.back_color = style.candidate_back_color = 0;
    style.hilited_back_color = style.hilited_candidate_back_color = 0;
    style.candidate_border_color = style.hilited_candidate_border_color = 0;
    style.candidate_shadow_color = style.hilited_candidate_shadow_color = 0;
    style.hilited_mark_color = 0;
  }

  bool Draw(HDC dc, int dest_width, int dest_height, float scale) const {
    if (!bitmap_)
      return false;
    if (!UpdateCache(dest_width, dest_height, scale))
      return false;
    return ::BitBlt(dc, 0, 0, dest_width, dest_height, cached_mem_dc_, 0, 0, SRCCOPY) != FALSE;
  }

  int width = 0, height = 0;
  int left = 0, right = 0, top = 0, bottom = 0;
  int inset_left = 0, inset_right = 0, inset_top = 0, inset_bottom = 0;

 private:
  void ClearCache() const {
    if (cached_mem_dc_) {
      if (cached_old_bitmap_) {
        ::SelectObject(cached_mem_dc_, cached_old_bitmap_);
        cached_old_bitmap_ = nullptr;
      }
      ::DeleteDC(cached_mem_dc_);
      cached_mem_dc_ = nullptr;
    }
    if (cached_bitmap_) {
      ::DeleteObject(cached_bitmap_);
      cached_bitmap_ = nullptr;
    }
    cached_pixels_ = nullptr;
    cached_dest_width_ = 0;
    cached_dest_height_ = 0;
    cached_scale_ = 0.0f;
  }

  bool UpdateCache(int dest_width, int dest_height, float scale) const {
    if (!bitmap_ || dest_width <= 0 || dest_height <= 0)
      return false;

    if (cached_mem_dc_ && cached_dest_width_ == dest_width &&
        cached_dest_height_ == dest_height && cached_scale_ == scale)
      return true;

    ClearCache();

    HDC screen_dc = ::GetDC(nullptr);
    cached_mem_dc_ = ::CreateCompatibleDC(screen_dc);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = dest_width;
    bmi.bmiHeader.biHeight = -dest_height;  // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    cached_bitmap_ = ::CreateDIBSection(cached_mem_dc_, &bmi, DIB_RGB_COLORS,
                                        &cached_pixels_, nullptr, 0);
    ::ReleaseDC(nullptr, screen_dc);

    if (!cached_bitmap_ || !cached_mem_dc_) {
      ClearCache();
      return false;
    }

    cached_old_bitmap_ = ::SelectObject(cached_mem_dc_, cached_bitmap_);
    memset(cached_pixels_, 0, static_cast<size_t>(dest_width) * dest_height * 4);

    const int xs[] = {0, left, width - right, width};
    const int ys[] = {0, top, height - bottom, height};
    const int xd[] = {0, static_cast<int>(left * scale),
                      dest_width - static_cast<int>(right * scale), dest_width};
    const int yd[] = {0, static_cast<int>(top * scale),
                      dest_height - static_cast<int>(bottom * scale), dest_height};

    Gdiplus::Graphics graphics(cached_mem_dc_);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeBilinear);
    Gdiplus::ImageAttributes attributes;
    attributes.SetWrapMode(Gdiplus::WrapModeTileFlipXY);
    for (int y = 0; y < 3; ++y) {
      for (int x = 0; x < 3; ++x) {
        if (xs[x + 1] == xs[x] || ys[y + 1] == ys[y])
          continue;
        if (graphics.DrawImage(bitmap_.get(),
              Gdiplus::Rect(xd[x], yd[y], xd[x + 1] - xd[x], yd[y + 1] - yd[y]),
              xs[x], ys[y], xs[x + 1] - xs[x], ys[y + 1] - ys[y],
              Gdiplus::UnitPixel, &attributes) != Gdiplus::Ok) {
          // The Graphics still owns this HDC. ClearCache runs on the next
          // retry, after Graphics has been destroyed.
          return false;
        }
      }
    }

    cached_dest_width_ = dest_width;
    cached_dest_height_ = dest_height;
    cached_scale_ = scale;
    return true;
  }

  FILETIME stamp_ = {};
  std::wstring active_file_;
  DWORD last_stat_tick_ = 0;
  bool checked_ = false;
  std::unique_ptr<Gdiplus::Bitmap> bitmap_;
  int spacing_ = 0, font_ = 16;
  COLORREF text_ = 0xff333333, candidate_ = 0xff333333, selected_ = 0xff993366;

  mutable HDC cached_mem_dc_ = nullptr;
  mutable HBITMAP cached_bitmap_ = nullptr;
  mutable HGDIOBJ cached_old_bitmap_ = nullptr;
  mutable void* cached_pixels_ = nullptr;
  mutable int cached_dest_width_ = 0;
  mutable int cached_dest_height_ = 0;
  mutable float cached_scale_ = 0.0f;
};
