// Configuration tests only: never initialize input handling or execute actions.
#include "../src/hotcorners-latest.cpp"
#include <string>
static unsigned checks, testFailures;
static void Check(bool ok,const char* name){++checks;if(!ok){++testFailures;printf("FAIL: %s\n",name);}}
int main(){
    using namespace NativeCorners;
    wchar_t path[MAX_PATH];GetModuleFileNameW(nullptr,path,MAX_PATH);*wcsrchr(path,L'\\')=0;
    std::wstring root=std::wstring(path)+L"\\corner-config-fixture-"+std::to_wstring(GetCurrentProcessId());
    Check(CreateDirectoryW(root.c_str(),nullptr)!=FALSE,"isolated fixture directory");
    std::wstring folderPath=root+L"\\hotcorners";
    Check(CreateDirectoryW(folderPath.c_str(),nullptr)!=FALSE,"isolated configuration directory");
    std::wstring file=folderPath+L"\\latest.ini";
    const wchar_t data[]=L"TopLeft=lock-screen\0TopRight=none\0BottomLeft=show-desktop\0BottomRight=task-view\0DelayMs=0\0TopLeftDelayMs=500\0CooldownMs=0\0CornerPixels=3\0RearmPixels=32\0Monitors=all\0PauseOnFullscreen=1\0\0";
    Check(WritePrivateProfileSectionW(L"HotCorners",data,file.c_str())!=FALSE,"write fixture configuration");
    Check(Validate(root.c_str()),"load per-corner delay");
    Check(config.Delay(0)==500&&config.Delay(1)==0&&config.Delay(2)==0&&config.Delay(3)==0,"only top left has dwell");
    Check(config.actions[0]==nhc::LockScreen&&config.actions[2]==nhc::ShowDesktop&&config.actions[3]==nhc::TaskView,"actions preserved");
    Check(config.pixels==3&&config.rearm==32&&config.cooldown==0&&config.fullscreen&&!config.primaryOnly,"suppression and rearm preserved");
    for(const wchar_t* value:{L"-1",L"1501",L"bad",L"500ms"}){
        WritePrivateProfileStringW(L"HotCorners",L"TopLeftDelayMs",value,file.c_str());
        Check(!LoadConfig(),"invalid override rejected");
        Check(config.Delay(0)==500&&config.Delay(2)==0,"invalid configuration rejected atomically");
    }
    WritePrivateProfileStringW(L"HotCorners",L"TopLeftDelayMs",nullptr,file.c_str());
    WritePrivateProfileStringW(L"HotCorners",L"DelayMs",L"300",file.c_str());
    Check(LoadConfig()&&config.Delay(0)==300&&config.Delay(2)==300,"missing override inherits global setting");
    WritePrivateProfileStringW(L"HotCorners",L"TopLeftDelayMs",L"0",file.c_str());
    Check(LoadConfig()&&config.Delay(0)==0&&config.Delay(3)==300,"explicit zero overrides global delay");
    WritePrivateProfileStringW(L"HotCorners",L"TopLeftDelayMs",L"1500",file.c_str());
    Check(LoadConfig()&&config.Delay(0)==1500,"maximum supported override accepted");
    WritePrivateProfileSectionW(L"HotCorners",data,file.c_str());
    Check(LoadConfig()&&config.Delay(0)==500&&config.Delay(2)==0,"requested configuration restored");
    RecordBlock(BlockReason::InputHeld,2);
    RecordBlock(BlockReason::Fullscreen,3);
    maximizedClientEntries=4;
    Check(WriteStatus(false),"per-corner status written");
    FILE* output=_wfopen(statusPath,L"rb");char json[4096]{};
    if(output){fread(json,1,sizeof(json)-1,output);fclose(output);}
    Check(strstr(json,"\"cornerDelayMs\":{\"topLeft\":500,\"topRight\":0,\"bottomLeft\":0,\"bottomRight\":0}")!=nullptr,"status exposes effective delays");
    Check(strstr(json,"\"blockedEntries\":2,")!=nullptr,"diagnostic entries retain total count");
    Check(strstr(json,"\"inputHeld\":1,\"inputDesktop\":0,\"fullscreen\":1")!=nullptr,"input and fullscreen suppression remain distinguishable");
    Check(strstr(json,"\"maximizedClientEntries\":4,")!=nullptr,"applied maximized-client exception is observable");
    Check(strstr(json,"\"lastBlock\":{\"reason\":\"fullscreen\"")!=nullptr&&strstr(json,"\"corner\":3,")!=nullptr,"last blocked corner and reason survive status serialization");
    Check(!initialized&&!rawRegistered&&!timerArmed&&!activations[0]&&!activations[2]&&!activations[3],"no production controls or input actions used");
    DeleteFileW(statusPath);DeleteFileW(file.c_str());RemoveDirectoryW(folderPath.c_str());RemoveDirectoryW(root.c_str());
    printf("%u configuration checks, %u failures (no input registered or sent)\n",checks,testFailures);
    return testFailures?1:0;
}
