#include "../Config.h"
#include "ProcessCore.h"
#include "../Misc/DynImport.h"
#include "../Include/Macro.h"
#include "../Include/Exception.h"
#include <3rd_party/VersionApi.h>

namespace blackbone
{

#ifdef COMPILER_GCC
#define PROCESS_DEP_ENABLE  0x00000001
#endif

ProcessCore::ProcessCore()
    : _native( nullptr )
{
}

ProcessCore::~ProcessCore()
{
    Close();
}

/// <summary>
/// Attach to existing process
/// </summary>
/// <param name="pid">Process ID</param>
/// <param name="access">Access mask</param>
///
void ProcessCore::Open( DWORD pid, DWORD access )
{
    if (pid == GetCurrentProcessId())
    {
        // Some routines in win10 do not support pseudo handle
        if (IsWindows10OrGreater())
            _hProcess = OpenProcess( PROCESS_ALL_ACCESS, FALSE, pid );
        else
            _hProcess = GetCurrentProcess();
    }
    else
        _hProcess = OpenProcess( access, false, pid );

    if (!_hProcess)
        THROW_WITH_STATUS_AND_LOG( LastNtStatus(), "Failed to open process" );

    _pid = pid;
    Init();
}

/// <summary>
/// Attach to existing process
/// </summary>
/// <param name="pid">Process ID</param>
/// <param name="access">Access mask</param>
/// <returns>Status</returns>
void ProcessCore::Open( HANDLE handle )
{
    _hProcess = handle;
    _pid = GetProcessId( _hProcess );
    Init();
}


/// <summary>
/// Initialize some internal data
/// </summary>
void ProcessCore::Init()
{
    // Detect x86 OS
    SYSTEM_INFO info = { { 0 } };
    GetNativeSystemInfo( &info );

    if (info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
    {
        _native = std::make_unique<x86Native>( _hProcess );
    }
    else
    {
        // Detect wow64 barrier
        BOOL wowSrc = FALSE;
        IsWow64Process( GetCurrentProcess(), &wowSrc );

        if (wowSrc == TRUE)
            _native = std::make_unique<NativeWow64>( _hProcess );
        else
            _native = std::make_unique<Native>( _hProcess );
    }

    // Get DEP info
    // For native x64 processes DEP is always enabled
    if (_native->GetWow64Barrier().targetWow64 == false)
    {
        _dep = true;
    }
    else
    {
        DWORD flags = 0;
        BOOL perm = 0;

        if (SAFE_CALL( GetProcessDEPPolicy, _hProcess, &flags, &perm ))
            _dep = (flags & PROCESS_DEP_ENABLE) != 0;
    }
}

/// <summary>
/// Close current process handle
/// </summary>
void ProcessCore::Close()
{
    _hProcess.reset();
    _native.reset();
    _pid = 0;
}

bool ProcessCore::isProtected()
{
    if (!_hProcess)
        THROW_AND_LOG( "No active process" );

    _PROCESS_EXTENDED_BASIC_INFORMATION_T<DWORD64> info = { 0 };
    info.Size = sizeof( info );

    _native->QueryProcessInfoT( ProcessBasicInformation, &info, sizeof( info ) );
    return info.Flags.IsProtectedProcess;
}

}