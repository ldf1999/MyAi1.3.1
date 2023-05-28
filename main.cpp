#include "Main.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "imstb_rectpack.h"
#include "imstb_textedit.h"
#include "imstb_truetype.h"

#include "Font.h"
#include "config-info.h"
#include <dxgi-capture.h>
#include <win-capture.h>
#include <process-info.h>
#include <create-resource.h>
#include <move-info.h>
#include "aim-info.h"




//全局结构体
struct Config_Info cfg_info;		//config参数
struct global_info global_data;		//全局信息
struct Porocess_info Process_data;	//后处理信息,目标信息

//当前全局结构体
static struct Time_info Time_i;	//后处理信息,目标信息

//互斥体
static HANDLE repeat_run = nullptr;		//互斥体,重复检测

//事件
HANDLE Aim_Event = nullptr;		//控制自瞄，全局事件

//线程句柄
static HANDLE aim_move_thread = nullptr;		//自瞄线程
static HANDLE Dynamic_Read_thread = nullptr;		//动态线程


//当前全局
static MSG msg;
static float Version = 1.3;					//版本标识
static bool Init_Ready = FALSE;				//初始化完成
static bool Set_Console_Ready = FALSE;		//设置控制台回调函数
static bool While_Should_Stop = FALSE;      //线程停止标识

//开关标识
static int key_state = 0;					//按键状态
static int Aim_state = FALSE;				//开关状态

//函数声明
static void Free();
static void Exit_Coda();
static inline void close_handle(HANDLE* handle);

static ID3D11Device* g_pd3dDevice = NULL;
static ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
static IDXGISwapChain* g_pSwapChain = NULL;
static ID3D11RenderTargetView* g_mainRenderTargetView = NULL;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);



//程序重复检测
static BOOL CheckRepeat() {
    SetLastError(0);
    HANDLE repeat_handle = CreateMutexA(NULL, FALSE, "Repeat");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxA(NULL, "已经存在一个程序运行", MESSAGEBOX_TIELE, MB_OK);
        close_handle(&repeat_handle);
        return FALSE;
    }
    return TRUE;
}

//获取管理员权限
static inline bool Load_UAC() {

    BOOL retn;
    HANDLE hToken;
    LUID Luid;

    //获取打开进程的令牌
    retn = OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken);
    if (retn != TRUE) {
        MessageBoxA(NULL, "请使用管理员身份运行", MESSAGEBOX_TIELE, MB_OK);
        return false;
    }

    //查找特权值
    retn = LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &Luid);
    if (retn != TRUE) {
        std::cout << "获取Luid失败" << std::endl;
        return false;
    }

    //给TP和TP里的LUID结构体赋值
    TOKEN_PRIVILEGES tp{}; //新特权结构体
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    tp.Privileges[0].Luid = Luid;

    //调整权限
    AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
    if (GetLastError() != ERROR_SUCCESS) {
        MessageBoxA(NULL, "请使用管理员身份运行", MESSAGEBOX_TIELE, MB_OK);
        system("pause");
        return false;
    }
    return true;
}

//加载重复检测
static BOOL CheckHead() {
    if (CheckRepeat())return TRUE;
    return FALSE;
}







static inline bool Check_Config_Version() {
    //检查ini文件和exe版本
    if (cfg_info.Check.checking_Version != Version) {
        std::cout << "版本出现问题与系统版本不一致" << cfg_info.Check.checking_Version << std::endl;
        system("pause");
        return false;
    }
    return true;
}

static inline bool Init_capture() {
    //DXGI捕获方式
    if (cfg_info.Windows.capture_method == 0) {
        if (!Init_dxgi()) {
            Free_dxgi();
            return false;
        }
        std::cout << "Dxgi screenshot initialization PASS..." << std::endl;
    }
    //GDI捕获方式
    if (cfg_info.Windows.capture_method == 1) {
        if (!Init_BitBlt()) {
            Free_BitBlt();
            return false;
        }
        std::cout << "BitBlt screenshot initialization PASS..." << std::endl;
    }
    return true;
}

void Write_Parameter_to_ini_file_key_method() {
    std::ifstream fin(cfg_info.Ini_Path.c_str());
    if (!fin.is_open()) {
        std::cerr << "无法打开文件！" << std::endl;
        return;
    }
    std::ostringstream oss;
    oss << fin.rdbuf();  // 将整个文件读入一个字符串流中
    fin.close();
    std::string content = oss.str();
    std::string new_value = std::to_string(cfg_info.Key.key_method); // 将截图方式设置为字符串类型
    // 在新值字符串的末尾添加一个引号字符
    new_value += "";
    // 转义双引号
    size_t quote_pos = new_value.find("\"", 0);
    while (quote_pos != std::string::npos) {
        new_value.insert(quote_pos, "\\");
        quote_pos = new_value.find("\"", quote_pos + 2);
    }

    // 更改需要更新的参数的值
    size_t start_pos = content.find("[ KEY ]");
    if (start_pos == std::string::npos) {
        std::cout << "找不到 [ KEY ] 节点" << std::endl;
        return;
    }
    size_t end_pos = content.find_first_of("\n\r", start_pos + 1);
    if (end_pos == std::string::npos) {
        std::cout << "无效的节点" << std::endl;
        return;
    }
    size_t capture_method_pos = content.find("key_method", start_pos + 1, end_pos - start_pos - 1);  // 查找参数位置
    if (capture_method_pos == std::string::npos) {
        // 如果参数不存在，就添加到末尾
        content.insert(end_pos, "\key_method = " + new_value);
    }
    else {
        size_t value_start_pos = content.find_first_not_of(" \t=", capture_method_pos + strlen("key_method"));  // 查找当前值起始位置
        if (value_start_pos == std::string::npos) {
            std::cout << "无效的参数值" << std::endl;
            return;
        }
        size_t value_end_pos = content.find_first_of("\r\n", value_start_pos + 1);  // 查找当前值结束位置
        if (value_end_pos == std::string::npos) {
            value_end_pos = end_pos - 1;
        }

        content.replace(value_start_pos, value_end_pos - value_start_pos, new_value);  // 替换为新值
    }

    // 将更新后的配置文件内容写回文件
    std::ofstream fout(cfg_info.Ini_Path);
    if (!fout.is_open()) {
        std::cerr << "无法写入文件！" << std::endl;
        return;
    }
    fout << content;
    fout.close();
}

void Write_Parameter_to_ini_file_capture_method() {
    std::ifstream fin(cfg_info.Ini_Path.c_str());
    if (!fin.is_open()) {
        std::cerr << "无法打开文件！" << std::endl;
        return;
    }
    std::ostringstream oss;
    oss << fin.rdbuf();  // 将整个文件读入一个字符串流中
    fin.close();
    std::string content = oss.str();
    std::string new_value = std::to_string(cfg_info.Windows.capture_method); // 将截图方式设置为字符串类型
    // 在新值字符串的末尾添加一个引号字符
    new_value += "";
    // 转义双引号
    size_t quote_pos = new_value.find("\"", 0);
    while (quote_pos != std::string::npos) {
        new_value.insert(quote_pos, "\\");
        quote_pos = new_value.find("\"", quote_pos + 2);
    }

    // 更改需要更新的参数的值
    size_t start_pos = content.find("[ WINDOW ]");
    if (start_pos == std::string::npos) {
        std::cout << "找不到 [ WINDOW ] 节点" << std::endl;
        return;
    }
    size_t end_pos = content.find_first_of("\n\r", start_pos + 1);
    if (end_pos == std::string::npos) {
        std::cout << "无效的节点" << std::endl;
        return;
    }
    size_t capture_method_pos = content.find("capture_method", start_pos + 1, end_pos - start_pos - 1);  // 查找参数位置
    if (capture_method_pos == std::string::npos) {
        // 如果参数不存在，就添加到末尾
        content.insert(end_pos, "\ncapture_method = " + new_value);
    }
    else {
        size_t value_start_pos = content.find_first_not_of(" \t=", capture_method_pos + strlen("capture_method"));  // 查找当前值起始位置
        if (value_start_pos == std::string::npos) {
            std::cout << "无效的参数值" << std::endl;
            return;
        }
        size_t value_end_pos = content.find_first_of("\r\n", value_start_pos + 1);  // 查找当前值结束位置
        if (value_end_pos == std::string::npos) {
            value_end_pos = end_pos - 1;
        }

        content.replace(value_start_pos, value_end_pos - value_start_pos, new_value);  // 替换为新值
    }

    // 将更新后的配置文件内容写回文件
    std::ofstream fout(cfg_info.Ini_Path);
    if (!fout.is_open()) {
        std::cerr << "无法写入文件！" << std::endl;
        return;
    }
    fout << content;
    fout.close();
}

void Write_Parameter_to_ini_file_move_manner() {
    std::ifstream fin(cfg_info.Ini_Path.c_str());
    if (!fin.is_open()) {
        std::cerr << "无法打开文件！" << std::endl;
        return;
    }
    std::ostringstream oss;
    oss << fin.rdbuf();  // 将整个文件读入一个字符串流中
    fin.close();
    std::string content = oss.str();
    std::string new_value = std::to_string(cfg_info.Move.move_manner); // 将截图方式设置为字符串类型
    // 在新值字符串的末尾添加一个引号字符
    new_value += "";
    // 转义双引号
    size_t quote_pos = new_value.find("\"", 0);
    while (quote_pos != std::string::npos) {
        new_value.insert(quote_pos, "\\");
        quote_pos = new_value.find("\"", quote_pos + 2);
    }
    // 更改需要更新的参数的值
    size_t start_pos = content.find("[ MOVE ]");
    if (start_pos == std::string::npos) {
        std::cout << "找不到 [ MOVE ] 节点" << std::endl;
        return;
    }
    size_t end_pos = content.find_first_of("\n\r", start_pos + 1);
    if (end_pos == std::string::npos) {
        std::cout << "无效的节点" << std::endl;
        return;
    }
    size_t capture_method_pos = content.find("move_manner", start_pos + 1, end_pos - start_pos - 1);  // 查找参数位置
    if (capture_method_pos == std::string::npos) {
        // 如果参数不存在，就添加到末尾
        content.insert(end_pos, "\move_manner = " + new_value);
    }
    else {
        size_t value_start_pos = content.find_first_not_of(" \t=", capture_method_pos + strlen("move_manner"));  // 查找当前值起始位置
        if (value_start_pos == std::string::npos) {
            std::cout << "无效的参数值" << std::endl;
            return;
        }
        size_t value_end_pos = content.find_first_of("\r\n", value_start_pos + 1);  // 查找当前值结束位置
        if (value_end_pos == std::string::npos) {
            value_end_pos = end_pos - 1;
        }

        content.replace(value_start_pos, value_end_pos - value_start_pos, new_value);  // 替换为新值
    }
    // 将更新后的配置文件内容写回文件
    std::ofstream fout(cfg_info.Ini_Path);
    if (!fout.is_open()) {
        std::cerr << "无法写入文件！" << std::endl;
        return;
    }
    fout << content;
    fout.close();
}


// -------------------------------  Tool  ------------------------------- //

//全局函数，向外提供 线程停止信号
bool Thread_Should_Stop() {
    return !!While_Should_Stop;
}

//所有初始化就绪，开始信号
static inline bool Start_Ready() {
    return !!Set_Console_Ready && !!Init_Ready;
}

//获取开关状态
static inline bool get_switch_state() {
    return !!Aim_state;
}

//画框函数
static inline bool Draw_box() {


    if (cfg_info.Aim.cla_off) {


        for (int i = 0; i < Process_data.indices.size(); i++) {
            if (Process_data.Classes[Process_data.indices[i]] == cfg_info.Aim.label_chose) {



                BBOX box;
                cv::rectangle(global_data.img,
                    cv::Rect(
                        Process_data.boxes[Process_data.indices[i]].x - Process_data.boxes[Process_data.indices[i]].width * 0.5f,
                        Process_data.boxes[Process_data.indices[i]].y - Process_data.boxes[Process_data.indices[i]].height * 0.5f,
                        Process_data.boxes[Process_data.indices[i]].width,
                        Process_data.boxes[Process_data.indices[i]].height),
                    cv::Scalar(0, 255, 0), 2, 8, 0);
            }
        }
    }
    else {
        for (int i = 0; i < Process_data.indices.size(); i++) {


            BBOX box;
            cv::rectangle(global_data.img,
                cv::Rect(
                    Process_data.boxes[Process_data.indices[i]].x - Process_data.boxes[Process_data.indices[i]].width * 0.5f,
                    Process_data.boxes[Process_data.indices[i]].y - Process_data.boxes[Process_data.indices[i]].height * 0.5f,
                    Process_data.boxes[Process_data.indices[i]].width,
                    Process_data.boxes[Process_data.indices[i]].height),
                cv::Scalar(0, 255, 0), 2, 8, 0);

        }

    }







    cv::imshow(WINDOWS_SHOW_NAME, global_data.img);
    cv::waitKey(1);
    return true;
}

static inline bool Win_Show_Switch() {

    if (!cfg_info.Windows.show) {
        //如果已经有一个窗口存在，则注销
        if (cv::getWindowProperty(WINDOWS_SHOW_NAME, cv::WND_PROP_VISIBLE)) {
            cv::destroyWindow(WINDOWS_SHOW_NAME);
        }
        return false;
    }
    //显示窗口
    Draw_box();
    return true;
}

//打印线程计时器
std::chrono::system_clock::time_point Print_Sleep_Start = std::chrono::system_clock::now();
static inline bool print_sleep() {
    std::chrono::system_clock::time_point Print_Sleep_End = std::chrono::system_clock::now();
    return std::chrono::duration_cast <std::chrono::milliseconds> (Print_Sleep_End - Print_Sleep_Start).count() >= cfg_info.Other.console_refresh;
}

static inline void Print_info() {
    //打印输出
    if (print_sleep() && cfg_info.Other.console_refresh) {
        std::cout
            << "循环: " << std::chrono::duration_cast<std::chrono::milliseconds>(Time_i.end - Time_i.start).count() << "ms\t"
            << "截图: " << std::chrono::duration_cast<std::chrono::milliseconds>(Time_i.capture_end - Time_i.capture_start).count() << "ms\t"
            << "推理: " << std::chrono::duration_cast<std::chrono::milliseconds>(Time_i.Inference_end - Time_i.Inference_start).count() << "ms\t"
            << "主动睡眠: " << cfg_info.Pred.sleep << "ms\t"
            << "FPS:" << 1000 / (std::chrono::duration_cast<std::chrono::milliseconds>(Time_i.end - Time_i.start).count())
            << std::endl;

        //更新计时器
        Print_Sleep_Start = std::chrono::system_clock::now();
    }
}

//初始化推理需要的资源
static inline bool My_Ai_TRT() {
    //初始化cuda
    if (!Init_CUDA()) {
        return false;
    }
    //初始化截图
    if (!Init_capture()) {
        return false;
    }
    //初始化推理框架
    if (!Init_frame()) {
        return false;
    }
    else {
        std::cout << "Frame initialization PASS..." << std::endl;
    }
    //初始化移动
    if (!Init_Move()) {
        return false;
    }

    return true;
}

// -------------------------------  Init_Event  ------------------------------- //
//创建事件，返回句柄
static inline HANDLE create_event(const wchar_t* name, DWORD pid) {
    HANDLE handle = nullptr;
    wchar_t new_name[64]{};

    //确保内核不会出现相同的事件
    _snwprintf(new_name, 64, L"%s%lu", name, pid);
    handle = CreateEventW(NULL, false, false, name);

    if (!handle) {
        return false;
    }
    return handle;
}

//初始化事件
static inline bool Init_Event() {
    DWORD pid = GetCurrentProcessId();

    //创事件
    Aim_Event = create_event(EVENT_CAPTURE_RESTART1, pid);
    if (!Aim_Event) {
        std::cout << "创建Aim_Event事件失败,错误码: " << GetLastError() << std::endl;
        return false;
    }
    return true;
}
// -------------------------------  Init_thread  ------------------------------- //
//初始化线程
static inline bool Init_thread() {
    //创建自瞄线程，挂起
    aim_move_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)aim_func, 0, CREATE_SUSPENDED, 0);
    if (aim_move_thread == NULL) {
        std::cout << "Init_thread; 创建自瞄线程失败,错误码: " << GetLastError() << std::endl;
        return false;
    }
    //创建动态更新线程，挂起
    Dynamic_Read_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)dynamic_thread_loop, 0, CREATE_SUSPENDED, 0);
    if (Dynamic_Read_thread == NULL) {
        std::cout << "Init_thread; 创建动态读取线程失败,错误码: " << GetLastError() << std::endl;
        return false;
    }

    return true;
}


// -------------------------------  Hook 键盘 ------------------------------- //
HHOOK switch_hook;
LRESULT CALLBACK KeyboardProc(int code, WPARAM wparam, LPARAM iparam) {	//iparam：VK键值
    if (code == HC_ACTION) {
        //退出按键
        if (GetAsyncKeyState(cfg_info.Key.end_key)) {
            While_Should_Stop = TRUE;
            Exit_Coda();
        }
        //开关
        if (GetAsyncKeyState(cfg_info.Key.aim_off) && key_state == 0) {
            key_state = 1;
            Aim_state = TRUE;
            std::cout << "Aim: 开" << std::endl;
        }
        if (!GetAsyncKeyState(cfg_info.Key.aim_off) && key_state == 1) {
            key_state = 2;
        }
        if (GetAsyncKeyState(cfg_info.Key.aim_off) && key_state == 2) {
            key_state = 3;
            Aim_state = FALSE;
            std::cout << "Aim: 关" << std::endl;
        }
        if (!GetAsyncKeyState(cfg_info.Key.aim_off) && key_state == 3) {
            key_state = 0;
        }
    }
    return CallNextHookEx(switch_hook, code, wparam, iparam);
}

static inline bool Init_keyboard_hook() {

    //hook全局键盘消息
    switch_hook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);

    if (!switch_hook) {
        std::cout << "hook按键失败,错误码:" << GetLastError() << std::endl;
        return false;
    }
    return true;
}

// -------------------------------  Init  ------------------------------- //
//所有初始化的开始函数
bool Init() {
    // 初始化ini参数	
    // Init_config()必须在Check_Config_Version()前运行
    if (!LoadConfig()) {
        return false;
    }
    //检查config版本是否对应
    if (!Check_Config_Version()) {
        return false;
    }
    //初始化应用资源
    if (!My_Ai_TRT()) {
        return false;
    }
    //初始化事件
    if (!Init_Event()) {
        return false;
    }
    //初始化线程
    if (!Init_thread()) {
        return false;
    }
    //hook键盘
    if (!Init_keyboard_hook()) {
        return false;
    }
    return true;
}

// -------------------------------  start_main  ------------------------------- //

static inline bool start_thread() {
    //启动自瞄线程
    DWORD hr = ResumeThread(aim_move_thread);
    if (hr == (DWORD)-1) {
        std::cout << "start_thread: 启动自瞄线程失败,错误码: " << GetLastError() << std::endl;
        return false;
    }
    //启动动态线程
    hr = ResumeThread(Dynamic_Read_thread);
    if (hr == (DWORD)-1) {
        std::cout << "start_thread: 启动动态读取线程失败,错误码: " << GetLastError() << std::endl;
        return false;
    }

    return true;
}

//推理
static inline bool Inference() {
    CUDA_CHECK(cudaMemcpyAsync(pred_data.buffers_ptr[0], pred_data.intput, pred_data.buffer_size[0], cudaMemcpyHostToDevice, trt.stream));
    trt.context->executeV2(pred_data.buffers_ptr);		//同步
    CUDA_CHECK(cudaMemcpyAsync(pred_data.output, pred_data.buffers_ptr[trt.engine->getNbBindings() - 1], pred_data.buffer_size[trt.engine->getNbBindings() - 1], cudaMemcpyDeviceToHost, trt.stream));
    return true;
}

static inline bool hook_msg() {
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return true;
}

//主函数循环
static bool start_main() {
    //线程启动
    if (!start_thread()) {
        return false;
    }

    std::cout << "Run..." << std::endl;
    //start
    while (true) {
        //派发键盘消息
        hook_msg();

        //是否退出程序
        //必须在派发消息之后停止循环
        if (Thread_Should_Stop()) {
            break;
        }


        Time_i.start = std::chrono::system_clock::now();					//循环开始时间

        //截图1-2-3ms
        Time_i.capture_start = std::chrono::system_clock::now();    		//捕获开始时间
        if (!global_data.capture_map()) {
            std::cout << "start_main: 截图发生错误，跳过本次循环" << std::endl;
            continue;
        }
        Time_i.capture_end = std::chrono::system_clock::now();		//捕获结束时间

        //预处理 2ms
        if (!global_data.Pre_process(global_data.img)) {
            std::cout << "start_main: 预处理发生错误，跳过本次循环" << std::endl;
            continue;
        }


        //推理 1050 yolov5 : 15ms yolox: 6ms
        Time_i.Inference_start = std::chrono::system_clock::now();     //推理开始时间
        if (!Inference()) {
            std::cout << "start_main: 推理发生错误，跳过本次循环" << std::endl;
            continue;
        }
        Time_i.Inference_end = std::chrono::system_clock::now();     //推理结束时间

        //后处理  0ms
        if (!global_data.Post_poress()) {
            std::cout << "start_main: 后处理发生错误，跳过本次循环" << std::endl;
            continue;
        }

        //自瞄打开 && 存在目标
        if (get_switch_state() && Process_data.indices.size())
            SetEvent(Aim_Event);

        //主动睡眠
        Sleep(cfg_info.Pred.sleep);
        Time_i.end = std::chrono::system_clock::now();//循环结束时间

        //画框
        Win_Show_Switch();
        //打印信息
        Print_info();
    }
    std::cout << "主线程结束" << std::endl;
    return true;
}

// -------------------------------  Main  ------------------------------- //
//控制台消息回调函数
BOOL WINAPI ConsoleHandler(DWORD CEvent) {
    if (CEvent == CTRL_CLOSE_EVENT) {	//消息 = 点击控制台的×
        //标识停止循环
        While_Should_Stop = TRUE;
        //退出代码
        Exit_Coda();
    }
    return true;
}

//设置控制台关闭消息
static inline bool Set_Console() {
    //设置控制台消息回调函数
    if (SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler, TRUE) == 0) {
        std::cout << "设置控制台回调函数错误" << std::endl;
        return false;
    }
    return true;
}

bool is_ai_running = false; // 记录ai线程是否在运行

static bool Open_Ai() {
    //设置控制台回调消息
    if (Set_Console())
        Set_Console_Ready = TRUE;	//设置控制台就绪标识

    //初始化
    if (Init())
        Init_Ready = TRUE;		//标志所有初始化就绪
    else
        system("pause");

    //运行函数
    if (Start_Ready())
        start_main();

    return 0;
}
void ai_thread() {
    Open_Ai();
    is_ai_running = false;
}

// -------------------------------  Free  ------------------------------- //
static inline void close_handle(HANDLE* handle)
{
    if (*handle) {
        CloseHandle(*handle);
        *handle = nullptr;
    }
}
//释放所有资源
static inline void Free() {
    //释放截图资源
    global_data.capture_free();

    //CUDA资源
    Free_CUDA();

    //移动句柄释放
    global_data.Move_free();

    //释放线程句柄
    close_handle(&aim_move_thread);
    close_handle(&Dynamic_Read_thread);


    //释放可等候句柄
    close_handle(&Aim_Event);

    //释放互斥体句柄
    close_handle(&repeat_run);

    //卸载hook
    UnhookWindowsHookEx(switch_hook);
}

static void Exit_Coda() {

    SetEvent(Aim_Event);

    WaitForSingleObject(aim_move_thread, INFINITE);	//等待自瞄线程退出

    WaitForSingleObject(Dynamic_Read_thread, INFINITE);	//等待自瞄线程退出

    //PostQuitMessage(0);
    //释放函数
    Free();

}




















int main() {
    ////隐藏控制台窗口
    //ShowWindow(GetConsoleWindow(), SW_HIDE);
    if (!CheckHead()) {
        return 1;
    }
    //if (!Load_UAC()) {
    //return 1;
    //}
    if (!LoadConfig()) {
        return 1;
    }


    if (!Check_Config_Version()) {
        return 1;
    }
    if (!hook_msg()) {
        return 1;
    }
    
    bool is_ai_running = false;

    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("My_Ai"), NULL };
    ::RegisterClassEx(&wc);

    HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("My_Ai"), WS_OVERLAPPEDWINDOW, 100, 100, 1, 1, NULL, NULL, wc.hInstance, NULL);

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_HIDE);
    ::UpdateWindow(hwnd);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;


    ImGui::StyleColorsDark();


    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    io.IniFilename = nullptr;
    ImFontConfig Font_cfg;
    Font_cfg.FontDataOwnedByAtlas = false;

    //ImFont* Font = io.Fonts->AddFontFromFileTTF("..\\ImGui Tool\\Font.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());
    ImFont* Font = io.Fonts->AddFontFromMemoryTTF((void*)Font_data, Font_size, 18.0f, &Font_cfg, io.Fonts->GetGlyphRangesChineseFull());
    ImFont* Font_Big = io.Fonts->AddFontFromMemoryTTF((void*)Font_data, Font_size, 24.0f, &Font_cfg, io.Fonts->GetGlyphRangesChineseFull());
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    bool done = false;
    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        {
            ImGuiStyle& style = ImGui::GetStyle();
            ImVec4* colors = style.Colors;
            // 修改各个颜色的值，将其设定为白色
            colors[ImGuiCol_Text] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
            colors[ImGuiCol_TextDisabled] = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
            colors[ImGuiCol_WindowBg] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            colors[ImGuiCol_ChildBg] = ImVec4(0.92f, 0.92f, 0.92f, 1.0f);
            colors[ImGuiCol_PopupBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.98f);
            colors[ImGuiCol_Border] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
            colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, .0f);
            colors[ImGuiCol_FrameBg] = ImVec4(0.85f, 0.85f, 0.85f, 0.30f);
            colors[ImGuiCol_FrameBgHovered] = ImVec4(0.85f, 0.85f, 0.85f, 0.40f);
            colors[ImGuiCol_FrameBgActive] = ImVec4(0.85f, 0.85f, 0.85f, 0.45f);
            colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            colors[ImGuiCol_TitleBgActive] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
            colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.8f, 0.8f, 0.8f, 0.6f);
            colors[ImGuiCol_MenuBarBg] = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
            colors[ImGuiCol_ScrollbarBg] = ImVec4(0.98f, 0.98f, 0.98f, 0.53f);
            colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.69f, 0.69f, 0.69f, 1.0f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.69f, 0.69f, 0.69f, 0.80f);
            colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.69f, 0.69f, 0.69f, 1.0f);
            colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
            colors[ImGuiCol_SliderGrab] = ImVec4(0.98f, 0.63f, 0.27f, 0.78f);
            colors[ImGuiCol_SliderGrabActive] = ImVec4(0.98f, 0.63f, 0.27f, 1.00f);

            static bool WinPos = true;//用于初始化窗口位置
            int Screen_Width{ GetSystemMetrics(SM_CXSCREEN) };//获取显示器的宽
            int Screen_Heigth{ GetSystemMetrics(SM_CYSCREEN) };//获取显示器的高

            if (WinPos)//初始化窗口
            {
                ImGui::SetNextWindowPos({ float(Screen_Width - 400) / 2,float(Screen_Heigth - 500) / 2 });
                WinPos = false;//初始化完毕
            }
            bool show_loading_window = false;

            bool show_window = true;
            int capture_method = cfg_info.Windows.capture_method;  // 获取当前截图方式
            int key_method = cfg_info.Key.key_method;  // 获取当前截图方式
            int move_manner = cfg_info.Move.move_manner;  // 获取当前截图方式
            bool show = cfg_info.Windows.show;
            bool show_debug_info = false;
            bool PID = cfg_info.Pid.pid_off;
            bool FOV = cfg_info.Fov.fov_off;


            ImGui::Begin(u8"My_Ai", &show_window, 0 | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
            if (!show_window)
            {
                exit(0);
            }
            ImGui::SetWindowSize({ 400.0f,500.0f });//设置窗口大小

            ImGui::Text(u8"当前版本号: %.2f",cfg_info.Check.checking_Version);
            ImGui::Separator();
            ImGui::Separator();
            if (ImGui::Combo(u8"截图方式", &capture_method, u8"dxgi顶层窗口\0Win32指定窗口")) {
                cfg_info.Windows.capture_method = capture_method;  // 根据选择更新截图方式
                Write_Parameter_to_ini_file_capture_method();  // 保存配置到文件
                std::cout << "checking_Version = " << cfg_info.Windows.win32_name << std::endl;
            }
            if (ImGui::Combo(u8"按键方式", &key_method, u8"右键\0自动")) {
                cfg_info.Key.key_method = key_method;  // 根据选择更新按键方式
                Write_Parameter_to_ini_file_key_method();  // 保存配置到文件
            }
            if (ImGui::Combo(u8"移动方式", &move_manner, u8"罗G\0KMBOX\0飞易来/易键鼠\0SendInput移动")) {
                cfg_info.Move.move_manner = move_manner;  // 根据选择更新移动方式
                Write_Parameter_to_ini_file_move_manner();  // 保存配置到文件
            }

            if (ImGui::Checkbox(u8"检测窗口",&show)) {
                cfg_info.Windows.show = show;  // 根据选择更新检测窗口方式
            }
            if (ImGui::Checkbox(u8"PID", &PID)) {
                cfg_info.Pid.pid_off = PID;  // 根据选择更新PID方式
            }
            if (ImGui::Checkbox(u8"FOV", &FOV)) {
                cfg_info.Fov.fov_off = FOV;  // 根据选择更新FOV方式
            }

            if (!is_ai_running && ImGui::Button(u8"开启AI")) {
                is_ai_running = true;
                std::thread t(ai_thread);
                t.detach();

            }
         


            ImGui::End();

            // 如果AI线程正在运行，则显示等待消息
            if (is_ai_running) {
                show_loading_window = true;
                /*imgui::setnextwindowsize(imvec2(200, 100), imguicond_appearing);
                imgui::begin(u8"等待");
                imgui::text(u8"ai 正在运行中...");
                ImGui::End();*/
            }
            else {
                if (show_loading_window) {
                    show_loading_window = false;
                } 
            }
        }

        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());


        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        g_pSwapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    Exit_Coda();
    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;




}


bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0 // From Windows SDK 8.1+ headers
#endif


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    case WM_DPICHANGED:
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports)
        {
            const RECT* suggested_rect = (RECT*)lParam;
            ::SetWindowPos(hWnd, NULL, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        break;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}


