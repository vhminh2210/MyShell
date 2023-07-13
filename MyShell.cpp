#include <windows.h>
#include <csignal>
#include <cstdlib>
#include <bits/stdc++.h>
#include <direct.h>
#define el '\n'

using namespace std;

struct TASK
{
    string Task;
    string status = "active";
    PROCESS_INFORMATION PI;
};

struct CMD
{
    string Type;
    vector<string> Arg;
};

map <DWORD, TASK> PROCESS_DICT; // ID - Tasks mapping
set < pair<int, DWORD> > PROCESS_IDS; // ID - Time mapping
set <DWORD> Cur_Ids; // Current processes

int cnt_history;
PROCESS_INFORMATION cur_fgp;
HANDLE Ctrl_handler;
bool fgp_interrupt = false;

string ROOT_PATH = ""; // Root directory of shell, for e.g. D:\\IT3070\\MyShell\\ //
list<string> PATH;

CMD get_cmd(string cmd_str);
int str2int(string x);
void Print_CMD(CMD cmd);
void SIGINT_Handler(int param);
void Get_signal();
void Create_Foreground_Process(LPCSTR task, DWORD MAX_TIME);
void Kill_Background_Process(PROCESS_INFORMATION pi, bool immediate, DWORD MAX_TIME);
PROCESS_INFORMATION Create_Background_Process(LPCSTR task);
bool Check_id_exists(DWORD id);
void Cleanse_Background();
void RaiseSyntaxError();
void RaiseCmdNotFound();
void EXIT();
void LIST();
void RUN_BGP(CMD cmd);
void RUN_FGP(CMD cmd);
void KILL(CMD cmd);
void RESUME(string Arg);
void PAUSE(string Arg);
void STATUS(DWORD id);
void HELP(const std::string& filename);
void BATCH(const std::string& filename);
void CD(const std::string& dir);
std::string getCurrentDirectory();
void DIR();
void DATE_();
void TIME_();
bool MKDIR(const std::string& directoryPath);
void get_subdir(stack<string> &subdir, string path);
int Find_in_path(string Query, string path, list<string> &path_found, bool all);
void FIND(CMD cmd);
bool path_exists(string path);
void load_paths(char*file_path);
void PATH_();
void MyShell();

int main()
{
    MyShell();
    Cleanse_Background();
    return 0;
}

CMD get_cmd(string cmd_str)
{
    CMD res;
    string blk = "";
    int i = 0;
    int len = cmd_str.length();
    while(cmd_str[i] == ' ' && i < len) ++i;
    while(cmd_str[i] != ' ' && i < len)
    {
        if(cmd_str[i] == '~') blk = blk + ' ';
        else blk = blk + cmd_str[i];
        ++i;
    }
    res.Type = blk;
    blk = "";
    ++i;
    while(i < len)
    {
        if(cmd_str[i] == ' ')
        {
            if(blk.length() > 0) res.Arg.push_back(blk);
            blk = "";
        }
        else {
            if(cmd_str[i] == '~') blk = blk + ' ';
            else blk = blk + cmd_str[i];
        }
        ++i;
    }
    if(blk.length() > 0) res.Arg.push_back(blk);
    blk = "";
    return res;
}

int str2int(string x)
{
    int res = 0;
    for(int i=0;i<x.length();++i)
    {
        res *= 10;
        res += (x[i] - '0');
    }
    return res;
}

void Print_CMD(CMD cmd)
{
    cout<<cmd.Type<<el;
    for(int i=0;i<cmd.Arg.size();++i) cout<<cmd.Arg[i]<<el;
    cout<<el;
    return;
}

void SIGINT_Handler(int param)
{
    TerminateProcess(cur_fgp.hProcess, 0);
    CloseHandle(cur_fgp.hProcess);
    CloseHandle(cur_fgp.hThread);
    fgp_interrupt = true;
    cout<<"Foreground process interrupt by Ctrl-C signal."<<el;
    TerminateThread(Ctrl_handler, 0);
    CloseHandle(Ctrl_handler);
    return;
}

void Get_signal()
{
    signal(SIGINT, SIGINT_Handler);
    string sig = "";
    while(true)
    {
        DWORD id_status;
        GetExitCodeProcess(cur_fgp.hProcess, &id_status);
        if(id_status != 259) return;
        getline(cin, sig);
        if(cin.fail() || cin.eof())
        {
            cin.clear();
            raise(SIGINT);
            return;
        }
        else
        {
            GetExitCodeProcess(cur_fgp.hProcess, &id_status);
            if(id_status == 259) cout<<"Unknown command.\n";
            else return;
        }
    }
    return;
}

void Create_Foreground_Process(LPCSTR task, DWORD MAX_TIME= INFINITE)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    CreateProcess(task, NULL, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
    cur_fgp = pi;

    DWORD Id;
    Ctrl_handler = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) Get_signal, NULL, 0, &Id);
    HANDLE Handle[2] = {pi.hProcess, Ctrl_handler};

    WaitForSingleObject(pi.hProcess, MAX_TIME);
    if(!fgp_interrupt) cout<<"Foreground process ended successfully. Press enter to continue ...\n";
    WaitForSingleObject(Ctrl_handler, MAX_TIME);

    if(!fgp_interrupt)
    {
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        CloseHandle(Ctrl_handler);
    }
    fgp_interrupt = false;
    return;
}

void Kill_Background_Process(PROCESS_INFORMATION pi, bool immediate= true, DWORD MAX_TIME= INFINITE)
{
    if(PROCESS_DICT[pi.dwProcessId].Task == "NULL") return;

    if(!immediate) WaitForSingleObject(pi.hProcess, MAX_TIME);

    TerminateProcess(pi.hProcess, 0);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    PROCESS_DICT[pi.dwProcessId].Task = "NULL";
    Cur_Ids.erase(pi.dwProcessId);
	cout<<"Background process "<<pi.dwProcessId<<" ended successfully.\n";
    return;
}

PROCESS_INFORMATION Create_Background_Process(LPCSTR task)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    CreateProcess(task, NULL, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
    TASK cur_task;
    cur_task.Task = task;
    cur_task.PI = pi;
    PROCESS_IDS.insert(make_pair(++cnt_history, pi.dwProcessId));
    Cur_Ids.insert(pi.dwProcessId);
    PROCESS_DICT[pi.dwProcessId] = cur_task;
    if(pi.dwProcessId == 0 && pi.dwThreadId == 0) cout<<"Background process created unsuccessfully.\n";
	else cout<<"Background process "<<pi.dwProcessId<<" created successfully.\n";
    return pi;
}

bool Check_id_exists(DWORD id)
{
    if(Cur_Ids.find(id) == Cur_Ids.end())
    {
        cout<<"Process "<<id<<" does not exist"<<el;
//        cout<<"~ ";
        return false;
    }
    return true;
}

void Cleanse_Background()
{
    vector<DWORD> cls;
    for(auto id: Cur_Ids)
    {
        TASK id_process = PROCESS_DICT[id];
        PROCESS_INFORMATION id_pi = id_process.PI;
        DWORD id_status;
        GetExitCodeProcess(id_pi.hProcess, &id_status);
        if(id_status != 259) cls.push_back(id);
    }
    for(int i=0;i<cls.size();++i)
    {
        DWORD id = cls[i];
        Cur_Ids.erase(id);
        PROCESS_DICT[id].Task = "NULL";
    }
    return;
}

void RaiseSyntaxError()
{
    cout<<"Syntax Error. Please check your command argument list"<<el;
//    cout<<"~ ";
    return;
}

void RaiseCmdNotFound()
{
    cout<<"LMS: Command not found."<<el;
//    cout<<"~ ";
    return;
}

void EXIT()
{
	for(auto P : PROCESS_IDS){
    	DWORD p = P.second;
    	Kill_Background_Process(PROCESS_DICT[p].PI);
	}
    // for(auto id: Cur_Ids)
    // {
    //     TASK id_process = PROCESS_DICT[id];
    //     PROCESS_INFORMATION id_pi = id_process.PI;
    //     DWORD id_status;
    //     GetExitCodeProcess(id_pi.hProcess, &id_status);
    //     if(id_status == 259) Kill_Background_Process(id_pi);
    // }
   return;
}

void LIST()
{
    for(auto P : PROCESS_IDS)
    {
        DWORD p = P.second;
        if(PROCESS_DICT[p].Task != "NULL") cout<<"Process ID: "<<p<<" - Task: "<<PROCESS_DICT[p].Task<<el;
    }
//    cout<<"~ ";
    return;
}

void RUN_BGP(CMD cmd)
{
	if(cmd.Arg.size() > 2)
    {
        RaiseSyntaxError();
//        cout<<"~ ";
        return;
    }
    int task_num = 1;
    if(cmd.Arg.size() == 2){
    	int temp = (int) str2int(cmd.Arg[1]);
    	if(temp>0) task_num = temp;
	}
    LPCSTR task = cmd.Arg[0].c_str();
    for(int i=0;i<task_num;i++) PROCESS_INFORMATION bpi = Create_Background_Process(task);
//    cout<<"~ ";
    return;
}

void RUN_FGP(CMD cmd)
{
    if(cmd.Arg.size() == 0 || cmd.Arg.size() > 2)
    {
        RaiseSyntaxError();
        return;
    }
    LPCSTR task = cmd.Arg[0].c_str();
    if(cmd.Arg.size() > 1)
    {
        DWORD t_wait = (DWORD) str2int(cmd.Arg[1]);
        Create_Foreground_Process(task, t_wait);
    }
    else Create_Foreground_Process(task);
//    cout<<"~ ";
    return;
}

void KILL(CMD cmd)
{
    if(cmd.Arg[0] == "-all"){
    	DWORD t_wait = 0;
        bool apart = false;
    	if(cmd.Arg.size()>=2) t_wait = (DWORD) str2int(cmd.Arg[1]);
        if(cmd.Arg.size()>=3&&cmd.Arg[2]=="-apart") apart = true;
    	for(auto P : PROCESS_IDS){
    		DWORD p = P.second;
            if(PROCESS_DICT[p].Task == "NULL") continue;
    		Kill_Background_Process(PROCESS_DICT[p].PI,false,t_wait);
            if(!apart) t_wait = 0;
		}
	}
	else if(cmd.Arg[0] == "-list"){
		if(cmd.Arg.size()<2) RaiseSyntaxError();
		else {
            DWORD t_wait = 0;
            bool apart = false;
            int off = 0,end = cmd.Arg.size();
            if(cmd.Arg[end-1] == "-apart" && cmd.Arg[end-3] == "-wait"){
                t_wait = (DWORD) str2int(cmd.Arg[end-2]);
                apart = true;
                off = 3;
            }else if(cmd.Arg[end-2] == "-wait"){
                t_wait = (DWORD) str2int(cmd.Arg[end-1]);
                off = 1;
            }
			for(int i=1;i < (end-off);i++){
    			DWORD id = (DWORD) str2int(cmd.Arg[i]);
				if(Check_id_exists(id)){
                    Kill_Background_Process(PROCESS_DICT[id].PI,false,t_wait);
                    if(!apart) t_wait = 0;
                }
			}
		}
	}
	else if(cmd.Arg[0] == "-task"){
		if(cmd.Arg.size()<2) RaiseSyntaxError();
    	else {
    		DWORD t_wait = 0;
            bool apart = false;
    		if(cmd.Arg.size()>=3) t_wait = (DWORD) str2int(cmd.Arg[2]);
            if(cmd.Arg.size()>=4&&cmd.Arg[2]=="-apart") apart = true;
    		for(auto P : PROCESS_IDS){
    			DWORD p = P.second;
    			if(PROCESS_DICT[p].Task==cmd.Arg[1]){
                    Kill_Background_Process(PROCESS_DICT[p].PI,false,t_wait);
                    if(!apart) t_wait = 0;
                }
			}
		}
	}
	else if(cmd.Arg.size() == 0 || cmd.Arg.size() > 2) RaiseSyntaxError();
    else {
    	DWORD id = (DWORD) str2int(cmd.Arg[0]);
    	if(!Check_id_exists(id)) return;
    	if(cmd.Arg.size() > 1)
    	{
    	    DWORD t_wait = (DWORD) str2int(cmd.Arg[1]);
    	    Kill_Background_Process(PROCESS_DICT[id].PI, false, t_wait);
    	}
    	else Kill_Background_Process(PROCESS_DICT[id].PI);
	}
//    cout<<"~ ";
    return;
}

void RESUME(string Arg)
{
    if(Arg == "-all")
    {
        for(auto P:PROCESS_IDS)
        {
            DWORD id = P.second;
            if(!Check_id_exists(id)) return;
            TASK task = PROCESS_DICT[id];
            PROCESS_INFORMATION pi = task.PI;
            if(task.status == "paused")
            {
                ResumeThread(pi.hThread);
            }
            else cout<<"Process "<<id<<" is not paused."<<el;
            PROCESS_DICT[id].status = "active";
        }
        cout<<"All paused processes are resumed.\n";
        cout<<"~ ";
        return;
    }
    DWORD id = (DWORD) str2int(Arg);
    if(!Check_id_exists(id)) return;
    TASK task = PROCESS_DICT[id];
    PROCESS_INFORMATION pi = task.PI;
    if(task.status == "paused")
    {
        ResumeThread(pi.hThread);
        cout<<"Process "<<id<<" resumes."<<el;
    }
    else cout<<"Process "<<id<<" is not paused."<<el;
    PROCESS_DICT[id].status = "active";
    cout<<el;
//    cout<<"\n~ ";
    return;
}

void PAUSE(string Arg)
{
    if(Arg == "-all")
    {
        for(auto P:PROCESS_IDS)
        {
            DWORD id = P.second;
            if(!Check_id_exists(id)) continue;
            TASK task = PROCESS_DICT[id];
            PROCESS_INFORMATION pi = task.PI;
            if(task.status != "paused") SuspendThread(pi.hThread);
            PROCESS_DICT[id].status = "paused";
        }
        cout<<"All processes are paused.\n";
//        cout<<"~ ";
        return;
    }
    DWORD id = (DWORD) str2int(Arg);
    if(!Check_id_exists(id)) return;
    TASK task = PROCESS_DICT[id];
    PROCESS_INFORMATION pi = task.PI;
    if(task.status != "paused") SuspendThread(pi.hThread);
    PROCESS_DICT[id].status = "paused";
    cout<<"Process "<<id<<" is paused."<<el;
//    cout<<"~ ";
    return;
}

void STATUS(DWORD id)
{
    if(!Check_id_exists(id)) return;
    TASK task = PROCESS_DICT[id];
    cout<<"Status: "<<task.status<<el;
    cout<<"Task: "<<task.Task<<el;
//    cout<<"\n~ ";
    return;
}

void HELP(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        std::cout << "Failed to open the file." << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::cout << line << std::endl;
    }

    file.close();
//    cout << "~ ";
    return;
}

void BATCH(const std::string& filename) {
    std::string command = "cmd /c \"" + filename + "\"";

    int result = system(command.c_str());

    // Check the result of the system call
    if (result == 0) {
        std::cout << "\nBatch file executed successfully." << std::endl;
    } else {
        std::cout << "\nFailed to execute the batch file." << std::endl;
    }
//    cout << "~ ";
    return;
}

void CD(const std::string& dir){
	const char* directory = dir.c_str();
	int result = chdir(directory);
    
    // Check the result of the directory change
    if (result == 0) {
        return;
    } else {
        std::cout << "No such directory." << std::endl;
//        cout << "~ ";
        return;
    }
	
}

std::string getCurrentDirectory() {
    char buffer[FILENAME_MAX];
    if (getcwd(buffer, sizeof(buffer)) != nullptr) {
        return std::string(buffer);
    } else {
        return std::string();
    }
}

void DIR(){
	const char* directoryPath = "."; // Specify the directory path to list

    intptr_t handle;
    _finddata_t fileInfo;

    handle = _findfirst((directoryPath + std::string("\\*")).c_str(), &fileInfo);
    if (handle == -1) {
        std::cout << "Failed to open directory." << std::endl;
        return;
    }
	int count = 0;
    do {
    	count++;
        if (count > 2)std::cout << fileInfo.name << std::endl;
    } while (_findnext(handle, &fileInfo) == 0);

    _findclose(handle);
}

void DATE_() {
    std::time_t currentTime = std::time(nullptr);
    std::tm* currentDate = std::localtime(&currentTime);
    int year = currentDate->tm_year + 1900;  // years since 1900
    int month = currentDate->tm_mon + 1;     // months since January [0-11]
    int day = currentDate->tm_mday;          // day of the month [1-31]
    std::cout << "Current Date: " << day << "-" << month << "-" << year << std::endl;
//    cout << "~ ";
}

void TIME_() {
    std::time_t currentTime = std::time(nullptr);
    std::string timeString = std::ctime(&currentTime);
    std::cout << "Current Time: " << timeString;
//    cout << "~ ";
}

bool MKDIR(const std::string& directoryPath) {
    int result = mkdir(directoryPath.c_str());

    // Check the result of the directory creation
    if (result == 0) {
        std::cout << "Directory " << directoryPath.c_str() << " created successfully." << std::endl;
        return true;
    } else {
        std::cout << "Failed to create directory " << directoryPath.c_str() << std::endl;
        return false;
    }
}

void get_subdir(stack<string> &subdir, string path){
    WIN32_FIND_DATA lpResult;
    string path_string = path + "*.*";
    LPCSTR path_LPCSTR = (LPCSTR) path_string.c_str();
    HANDLE hFind = FindFirstFileA(path_LPCSTR, &lpResult);
    do{
        if (lpResult.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
            string dir = (char*) lpResult.cFileName;
            if(dir != "." && dir != ".."){
                subdir.push(path + dir + '\\');
            }
        }
    }while (FindNextFile(hFind, &lpResult) != 0);
}

int Find_in_path(string Query, string path, list<string> &path_found, bool all = false)
{
    bool found = false;
    int flag = 0;
    string path_Query = path + Query;
    LPCSTR lpQuery = (LPCSTR) path_Query.c_str();
    WIN32_FIND_DATA lpResult;
    HANDLE hFind = FindFirstFileA(lpQuery, &lpResult);
    if(hFind != INVALID_HANDLE_VALUE){
        path_found.push_back(path);
        found = true;
    }else flag = GetLastError();

    if(flag == 2||all){// flag == 2: ERROR_FILE_NOT_FOUND
        stack<string> subdir;
        get_subdir(subdir, path);
        while(!subdir.empty()){
            path = subdir.top();
            subdir.pop();
            path_Query = path + Query;
            lpQuery = (LPCSTR) path_Query.c_str();
            hFind = FindFirstFileA(lpQuery, &lpResult);
            if(hFind != INVALID_HANDLE_VALUE){
                path_found.push_back(path);
                found = true;
                if(!all) break;
            }
            get_subdir(subdir, path);
        }
    }

    if(found) flag = 0;
    FindClose(hFind);
    return flag;
}

void FIND(CMD cmd)
{
    if(cmd.Arg.size()<1)
    {
        RaiseSyntaxError();
        cout<<el;
        return;
    }
    list<string> path_found;
    string Query = cmd.Arg[0];
    string mode = "-auto";
    bool check_all = false, check_path = false, found = false;
    int end = cmd.Arg.size(), flag;
    if(cmd.Arg.size() >=3 && cmd.Arg[end-1] == "-all") check_all = true;
    if(cmd.Arg.size() >=3 && cmd.Arg[1] == "-path") check_path = true;
    if(check_path){
        string path = cmd.Arg[2];
        if(path[path.length()-1]!='\\') path = path + '\\';
        flag = Find_in_path(Query, path, path_found, check_all);
        if(!flag){
            cout<<"The queried file/folder is found in "<<path<<"."<<el;
            found = true;
        }else if(flag == 3) cout<<"Path "<<path<<" not found."<<el;
        else cout<<"The queried file/folder is not found in "<<path<<"."<<el;
    }

    if(check_all||!check_path)for(list<string>::iterator P = PATH.begin();P != PATH.end();P++){
        flag = Find_in_path(Query, *P, path_found, check_all);
        if(!flag){
            // cout<<"The queried file/folder is found with path "<<*P + Query<<el;
            found = true;
            if(!check_all) break;
        }
        // else if(flag == 3) cout<<"Path "<<*P<<" not found."<<el;
        // else cout<<"The queried file/folder is not found in "<<*P<<"."<<el;
    }
    if(!found) cout<<"The queried file/folder is not found."<<el;
    else{
        cout<<"The queried file/folder is found in:"<<el;
        for(list<string>::iterator pf = path_found.begin();pf != path_found.end();pf++) cout<<*pf<<el;
    }

    return;
}

bool path_exists(string path)
{
    string path_Query = path + "*.*";
    LPCSTR lpQuery = (LPCSTR) path_Query.c_str();
    WIN32_FIND_DATA lpResult;
    HANDLE hFind = FindFirstFileA(lpQuery, &lpResult);
    return hFind != INVALID_HANDLE_VALUE;
}

void load_paths(char*file_path)
{
    FILE*file = fopen(file_path,"r");
    if(file == NULL){
        cout<<"Load file unsuccessfully"<<el;
        return;
    }
    string path = "";
    while(1){
        char temp[2];
        fread(temp,sizeof(char),1,file);
        if(feof(file)) break;
        if(strcmp(temp,"\n")) path = path + temp;
        else{
            PATH.push_back(path);
            path = "";
        }
    }
    PATH.unique();
    fclose(file);
}

void PATH_(CMD cmd)
{
    if(cmd.Arg.size()>=2 && cmd.Arg[0]=="-insert"){
        string path = cmd.Arg[1];
        if(path[path.length()-1]!='\\') path = path + '\\';
        if(path_exists(path)) PATH.push_back(path);
        else {
            char check = ' ';
            cout<<"Path "<<path<<" is invalid. Insert anyway?\nY/y: Yes, Anything else: No\n";
            cin>>check;
            if(check=='y' || check=='Y') PATH.push_back(path);
        }
        PATH.unique();
    }else if(cmd.Arg.size()>=2 && cmd.Arg[0]=="-remove"){
        string path = cmd.Arg[1];
        if(path[path.length()-1]!='\\') path = path + '\\';
        if(path==ROOT_PATH){
            char check = ' ';
            cout<<"This is the root path. Are you sure you want to remove?\nY/y: Yes, Anything else: No\n";
            cin>>check;
            if(check=='y' || check=='Y') PATH.remove(path);
        }else PATH.remove(path);
    }else if(cmd.Arg.size()>=1 && cmd.Arg[0]=="-reset"){
        PATH.clear();
        PATH.push_back(ROOT_PATH);
    }else if(cmd.Arg.size()>=2 && cmd.Arg[0]=="-load"){
        string file_path = cmd.Arg[1];
        load_paths((char*) file_path.c_str());
    }else{
        for(list<string>::iterator P = PATH.begin();P != PATH.end();P++){
            if(*P==ROOT_PATH) cout<<"(root) ";
            cout<<*P<<el;
        }
        // cout<<"----------END OF PATH ENVIRONMENT LIST----------\n";
    }
    string path = ROOT_PATH + "path.txt";
    FILE*file = fopen(path.c_str(),"w");
    for(list<string>::iterator P = PATH.begin();P != PATH.end();P++){
        char*p = (char*) (*P).c_str(), ln[] = "\n";
        fwrite(p,sizeof(char),strlen(p),file);
        fwrite(ln,sizeof(char),1,file);
    }
    fclose(file);
    return;
}

void MyShell()
{
    cout<<"Welcome to MyShell!\n\nPlease type \"help\" for instructions\n "<<el;
    string cmd_str;
    ROOT_PATH = getCurrentDirectory() + "\\";
    PATH.push_back(ROOT_PATH);
    string temp = ROOT_PATH +"path.txt";
    load_paths((char*) temp.c_str());
    
    while(true)
    {
        Cleanse_Background();
        fflush(stdin);
        std::string currentDir = getCurrentDirectory();
    	std::cout << "LMS " << currentDir << " >> ";
        getline(cin, cmd_str);
        CMD cmd = get_cmd(cmd_str);
        if(cmd.Type == "exit")
        {
            char check = ' ';
			cout<<"Are you sure you want to exit the shell?\nY/y: Yes, Anything else: No\n";
            cin>>check;
            if(check=='y' || check=='Y') EXIT();
            break;
        }
        if(cmd.Type == "bgp")
        {
            RUN_BGP(cmd);
            continue;
        }
        if(cmd.Type == "list")
        {
            LIST();
            continue;
        }
        if(cmd.Type == "kill")
        {
            KILL(cmd);
            continue;
        }
        if(cmd.Type == "fgp")
        {
            RUN_FGP(cmd);
            continue;
        }
        if(cmd.Type == "pause")
        {
            if(cmd.Arg.size() != 1)
            {
                RaiseSyntaxError();
                continue;
            }
            PAUSE(cmd.Arg[0]);
            continue;
        }
        if(cmd.Type == "resume")
        {
            if(cmd.Arg.size() != 1)
            {
                RaiseSyntaxError();
                continue;
            }
            RESUME(cmd.Arg[0]);
            continue;
        }
        if(cmd.Type == "status")
        {
            if(cmd.Arg.size() != 1)
            {
                RaiseSyntaxError();
                continue;
            }
            STATUS((DWORD) str2int(cmd.Arg[0]));
            continue;
        }
        if(cmd.Type == "help")
        {
        HELP(ROOT_PATH + "documentation.txt");
        	continue;
		}
		if(cmd.Type == "bat"){
			if(cmd.Arg.size() != 1)
			{
				RaiseSyntaxError();
				continue;
			}
			BATCH(cmd.Arg[0]);
			continue;
		}
		if(cmd.Type == "dir"){
			if(cmd.Arg.size() != 0)
			{
				RaiseSyntaxError();
				continue;
			}
			DIR();
			continue;
		}
		if (cmd.Type == "date"){
			DATE_();
			continue;
		}
		if (cmd.Type == "time"){
			TIME_();
			continue;
		}
		if (cmd.Type == "cd"){
			if(cmd.Arg.size() != 1)
			{
				RaiseSyntaxError();
				continue;
			}
			CD(cmd.Arg[0]);
			continue;
		}
		if (cmd.Type == "mkdir"){

			for(int l = 0; l < cmd.Arg.size();l++){
				MKDIR(cmd.Arg[l]);
			}
			continue;
		}
        if (cmd.Type == "find")
        {
            if(cmd.Arg.size() > 4)
            {
                RaiseSyntaxError();
                continue;
            }
            FIND(cmd);
            continue;
        }
        if (cmd.Type == "path")
        {
            if(cmd.Arg.size() > 4)
            {
                RaiseSyntaxError();
                continue;
            }
            PATH_(cmd);
            continue;
        }
        if (cmd.Type == "system"){
            char permit = ' ';
            printf("WARNING: This feature may cause random things to happen.\nPermit? ");
            printf("Y/y: Yes, Anything else: No\n");
            scanf("%c",&permit);
            if (permit=='y' || permit=='Y'){
                std::string a = "";
                for(int l = 0; l < cmd.Arg.size();l++){
                    a += " ";
                    a += cmd.Arg[l];
                }
                int result = system(a.c_str());
                if (result==0) continue;
                else RaiseCmdNotFound();
            }
            continue;
        }
		RaiseCmdNotFound();
        //cout<<"~ ";
    }
    return;
}
