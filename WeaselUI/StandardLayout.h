#pragma once

#include "Layout.h"
#include <d2d1.h>
#include <dwrite.h>
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

namespace weasel {
const int MAX_CANDIDATES_COUNT = 100;
const int STATUS_ICON_SIZE = GetSystemMetrics(SM_CXICON);

class StandardLayout : public Layout {
 public:
  StandardLayout(const UIStyle& style,
                 const Context& context,
                 const Status& status,
                 PDWR pDWR)
      : Layout(style, context, status, pDWR) {}

  /* Layout */

  virtual void DoLayout(CDCHandle dc, PDWR pDWR = NULL) = 0;
  virtual CSize GetContentSize() const { return _contentSize; }
  // Translate both paint and hit-test rectangles as one operation.
  void AddSkinInsets(int left, int top, int right, int bottom,
                     int min_width, int min_height) {
    _preeditRect.OffsetRect(left, top);
    _auxiliaryRect.OffsetRect(left, top);
    _highlightRect.OffsetRect(left, top);
    _statusIconRect.OffsetRect(left, top);
    _prePageRect.OffsetRect(left, top);
    _nextPageRect.OffsetRect(left, top);
    for (int i = 0; i < candidates_count && i < MAX_CANDIDATES_COUNT; ++i) {
      _candidateRects[i].OffsetRect(left, top);
      _candidateLabelRects[i].OffsetRect(left, top);
      _candidateTextRects[i].OffsetRect(left, top);
      _candidateCommentRects[i].OffsetRect(left, top);
    }
    _contentSize.cx = max(min_width, _contentSize.cx + left + right);
    _contentSize.cy = max(min_height, _contentSize.cy + top + bottom);
    _contentRect.SetRect(0, 0, _contentSize.cx, _contentSize.cy);
  }
  virtual CRect GetPreeditRect() const { return _preeditRect; }
  virtual CRect GetAuxiliaryRect() const { return _auxiliaryRect; }
  virtual CRect GetHighlightRect() const { return _highlightRect; }
  virtual CRect GetCandidateLabelRect(int id) const {
    return _candidateLabelRects[id];
  }
  virtual CRect GetCandidateTextRect(int id) const {
    return _candidateTextRects[id];
  }
  virtual CRect GetCandidateCommentRect(int id) const {
    return _candidateCommentRects[id];
  }
  virtual CRect GetCandidateRect(int id) const { return _candidateRects[id]; }
  virtual CRect GetStatusIconRect() const { return _statusIconRect; }
  virtual std::wstring GetLabelText(const std::vector<Text>& labels,
                                    int id,
                                    const wchar_t* format) const;
  virtual bool IsInlinePreedit() const;
  virtual bool ShouldDisplayStatusIcon() const;
  virtual IsToRoundStruct GetRoundInfo(int id) { return _roundInfo[id]; }
  virtual IsToRoundStruct GetTextRoundInfo() { return _textRoundInfo; }
  virtual CRect GetContentRect() { return _contentRect; }
  virtual CRect GetPrepageRect() { return _prePageRect; }
  virtual CRect GetNextpageRect() { return _nextPageRect; }
  virtual CSize GetBeforeSize() { return _beforesz; }
  virtual CSize GetHilitedSize() { return _hilitedsz; }
  virtual CSize GetAfterSize() { return _aftersz; }
  virtual weasel::TextRange GetPreeditRange() { return _range; }

  void GetTextSizeDW(const std::wstring& text,
                     size_t nCount,
                     ComPtr<IDWriteTextFormat1>& pTextFormat,
                     PDWR pDWR,
                     LPSIZE lpSize) const;

 protected:
  /* Utility functions */
  CSize GetPreeditSize(CDCHandle dc,
                       const weasel::Text& text,
                       ComPtr<IDWriteTextFormat1> pTextFormat = NULL,
                       PDWR pDWR = NULL);
  bool _IsHighlightOverCandidateWindow(CRect& rc, CDCHandle& dc);
  void _PrepareRoundInfo(CDCHandle& dc);

  void UpdateStatusIconLayout(int* width, int* height);

  CSize _beforesz, _hilitedsz, _aftersz;
  weasel::TextRange _range;
  CSize _contentSize;
  CRect _preeditRect, _auxiliaryRect, _highlightRect;
  CRect _candidateRects[MAX_CANDIDATES_COUNT];
  CRect _candidateLabelRects[MAX_CANDIDATES_COUNT];
  CRect _candidateTextRects[MAX_CANDIDATES_COUNT];
  CRect _candidateCommentRects[MAX_CANDIDATES_COUNT];
  CRect _statusIconRect;
  CRect _bgRect;
  CRect _contentRect;
  IsToRoundStruct _roundInfo[MAX_CANDIDATES_COUNT];
  IsToRoundStruct _textRoundInfo;
  CRect _prePageRect;
  CRect _nextPageRect;
  const std::wstring pre = L"<";
  const std::wstring next = L">";
};
};  // namespace weasel
