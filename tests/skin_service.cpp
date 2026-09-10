#include "stdafx.h"
#include <WeaselIPC.h>
#include <ResponseParser.h>
#include <resource.h>
#include <iostream>

int main() {
  weasel::Client client;
  if (!client.Connect()) return 2;
  client.StartSession();
  if (!client.Echo()) return 3;
  client.TrayCommand(ID_WEASELTRAY_DISABLE_ASCII);
  weasel::Context context;
  weasel::Status status;
  weasel::Config config;
  weasel::UIStyle style;
  std::wstring commit;
  weasel::ResponseParser parser(&commit, &context, &status, &config, &style);
  bool passed = true;
  for (char key : std::string("nihao")) {
    passed &= client.ProcessKeyEvent(weasel::KeyEvent(key, 0));
    passed &= client.GetResponseData(std::ref(parser));
  }
  passed &= !context.cinfo.empty() && status.composing &&
            status.schema_id == L"rime_ice" && commit.empty();
  std::cout << "schema=" << wtou8(status.schema_id)
            << " candidates=" << context.cinfo.candies.size()
            << " preedit=" << wtou8(context.preedit.str) << "\n";
  // Do not commit a synthetic phrase to the user dictionary or any application.
  client.ClearComposition();
  client.EndSession();
  client.Disconnect();
  std::cout << (passed ? "PASS" : "FAIL") << " real server IPC composition\n";
  return passed ? 0 : 1;
}
