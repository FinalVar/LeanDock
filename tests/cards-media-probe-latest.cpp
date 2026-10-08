#include <windows.h>
#include <cstdio>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>
int main(int argc,char**){try{winrt::init_apartment(winrt::apartment_type::multi_threaded);auto m=winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();auto s=m.GetCurrentSession();printf("SMTC available, sessions=%u, current=%s\n",m.GetSessions().Size(),s?"yes":"no");
 if(argc>1){
  if(!s || s.TryGetMediaPropertiesAsync().get().Title()!=L"LeanDock 控制验证"){puts("Current session is not the fixture; no command sent");return 2;}
  const bool toggle=s.TryTogglePlayPauseAsync().get();Sleep(300);
  const bool next=s.TrySkipNextAsync().get();Sleep(300);
  const bool previous=s.TrySkipPreviousAsync().get();Sleep(300);
  printf("fixture commands: toggle=%d next=%d previous=%d\n",toggle,next,previous);
  return toggle&&next&&previous?0:3;
 }
 return 0;}catch(winrt::hresult_error const&e){printf("SMTC error=%08lx\n",(unsigned long)e.code().value);return 1;}}
