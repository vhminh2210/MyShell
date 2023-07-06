#include <windows.h>
#include <csignal>
#include <cstdlib>
#include <bits/stdc++.h>

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

CMD get_cmd(string cmd_str)
{
    CMD res;
    string blk = "";
    int i = 0;
    int len = cmd_str.length();
    while(cmd_str[i] == ' ' && i < len) ++i;
    while(cmd_str[i] != ' ' && i < len)
    {
        blk = blk + cmd_str[i];
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
        else blk = blk + cmd_str[i];
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

    return pi;
}

bool Check_id_exists(DWORD id)
{
    if(Cur_Ids.find(id) == Cur_Ids.end())
    {
        cout<<"Process "<<id<<" does not exist"<<el;
        cout<<"~ ";
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
    cout<<"~ ";
    return;
}

void RaiseCmdNotFound()
{
    cout<<"Command not found."<<el;
    cout<<"~ ";
    return;
}

void EXIT()
{
    for(auto id: Cur_Ids)
    {
        TASK id_process = PROCESS_DICT[id];
        PROCESS_INFORMATION id_pi = id_process.PI;
        DWORD id_status;
        GetExitCodeProcess(id_pi.hProcess, &id_status);
        if(id_status == 259) Kill_Background_Process(id_pi);
    }
    return;
}

void LIST()
{
    for(auto P : PROCESS_IDS)
    {
        DWORD p = P.second;
        if(PROCESS_DICT[p].Task != "NULL") cout<<"Process ID: "<<p<<" - Task: "<<PROCESS_DICT[p].Task<<el;
    }
    cout<<"~ ";
    return;
}

void RUN_BGP(CMD cmd)
{
    if(cmd.Arg.size() != 1)
    {
        RaiseSyntaxError();
        return;
    }
    LPCSTR task = cmd.Arg[0].c_str();
    PROCESS_INFORMATION bpi = Create_Background_Process(task);
    cout<<"Process created with id: "<<bpi.dwProcessId<<el;
    cout<<"\n~ ";
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
    cout<<"~ ";
    return;
}

void KILL(CMD cmd)
{
    if(cmd.Arg.size() == 0 || cmd.Arg.size() > 2)
    {
        RaiseSyntaxError();
        return;
    }
    DWORD id = (DWORD) str2int(cmd.Arg[0]);
    if(!Check_id_exists(id)) return;
    if(cmd.Arg.size() > 1)
    {
        DWORD t_wait = (DWORD) str2int(cmd.Arg[1]);
        Kill_Background_Process(PROCESS_DICT[id].PI, false, t_wait);
    }
    else Kill_Background_Process(PROCESS_DICT[id].PI);
    cout<<"~ ";
    return;
}

void RESUME(DWORD id)
{
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
    cout<<"\n~ ";
    return;
}

void PAUSE(DWORD id)
{
    if(!Check_id_exists(id)) return;
    TASK task = PROCESS_DICT[id];
    PROCESS_INFORMATION pi = task.PI;
    if(task.status != "paused") SuspendThread(pi.hThread);
    PROCESS_DICT[id].status = "paused";
    cout<<"Process "<<id<<" is paused."<<el;
    cout<<"~ ";
    return;
}

void STATUS(DWORD id)
{
    if(!Check_id_exists(id)) return;
    TASK task = PROCESS_DICT[id];
    cout<<"Status: "<<task.status<<el;
    cout<<"Task: "<<task.Task<<el;
    cout<<"\n~ ";
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
    cout << "\n~ ";
    return;
}

void BATCH(const std::string& filename) {
    std::string command = "cmd /c " + filename;

    int result = system(command.c_str());

    // Check the result of the system call
    if (result == 0) {
        std::cout << "\nBatch file executed successfully." << std::endl;
    } else {
        std::cout << "\nFailed to execute the batch file." << std::endl;
    }
    cout << "~ ";
    return;
}

void MyShell()
{
    cout<<"Welcome to MyShell!"<<el;
    cout<<"~ ";
    string cmd_str;
    while(true)
    {
        Cleanse_Background();
        getline(cin, cmd_str);
        CMD cmd = get_cmd(cmd_str);
        if(cmd.Type == "exit")
        {
            EXIT();
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
            PAUSE((DWORD) str2int(cmd.Arg[0]));
            continue;
        }
        if(cmd.Type == "resume")
        {
            if(cmd.Arg.size() != 1)
            {
                RaiseSyntaxError();
                continue;
            }
            RESUME((DWORD) str2int(cmd.Arg[0]));
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
        	HELP("documentation.txt");
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
        RaiseCmdNotFound();
        //cout<<"~ ";
    }
    return;
}

int main()
{
    MyShell();
    Cleanse_Background();
    return 0;
}
