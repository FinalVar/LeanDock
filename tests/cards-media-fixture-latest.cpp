#include <windows.h>
#include <atomic>
#include <cstdio>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.h>
#include <winrt/Windows.Media.Playback.h>
namespace m=winrt::Windows::Media;
int main(){try{
 winrt::init_apartment(winrt::apartment_type::multi_threaded);
 WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"LeanDock.MediaFixture.v1";RegisterClassW(&wc);
 HWND window=CreateWindowW(wc.lpszClassName,L"LeanDock media integration fixture",0,0,0,1,1,nullptr,nullptr,wc.hInstance,nullptr);
 m::Playback::MediaPlayer player;player.CommandManager().IsEnabled(false);
 auto smtc=player.SystemMediaTransportControls();smtc.IsEnabled(true);smtc.IsPlayEnabled(true);smtc.IsPauseEnabled(true);smtc.IsNextEnabled(true);smtc.IsPreviousEnabled(true);
 auto display=smtc.DisplayUpdater();display.Type(m::MediaPlaybackType::Music);display.MusicProperties().Title(L"LeanDock 控制验证");display.MusicProperties().Artist(L"临时测试会话");display.Update();
 smtc.PlaybackStatus(m::MediaPlaybackStatus::Playing);
 std::atomic<unsigned> commands=0;
 auto revoke=smtc.ButtonPressed(winrt::auto_revoke,[&](auto&&,m::SystemMediaTransportControlsButtonPressedEventArgs const&e){
  const auto n=++commands;SetPropW(window,L"Commands",reinterpret_cast<HANDLE>(uintptr_t(n+1)));
  switch(e.Button()){
   case m::SystemMediaTransportControlsButton::Pause:smtc.PlaybackStatus(m::MediaPlaybackStatus::Paused);break;
   case m::SystemMediaTransportControlsButton::Play:smtc.PlaybackStatus(m::MediaPlaybackStatus::Playing);break;
   case m::SystemMediaTransportControlsButton::Next:display.MusicProperties().Title(L"下一首已收到");display.Update();break;
   case m::SystemMediaTransportControlsButton::Previous:display.MusicProperties().Title(L"上一首已收到");display.Update();break;
   default:break;
  }
 });
 SetPropW(window,L"Commands",reinterpret_cast<HANDLE>(1));
 // The fixture is silent and bounded. WM_CLOSE or timeout ends it without
 // modifying a user's media session or installing a background component.
 auto start=GetTickCount64();MSG msg{};
 while(IsWindow(window)&&GetTickCount64()-start<180000){
  MsgWaitForMultipleObjects(0,nullptr,FALSE,500,QS_ALLINPUT);
  while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
 }
 smtc.IsEnabled(false);player.Close();if(IsWindow(window))DestroyWindow(window);
 printf("commands received=%u\n",commands.load());return 0;
}catch(winrt::hresult_error const&e){printf("fixture error=%08lx\n",(unsigned long)e.code().value);return 1;}}
