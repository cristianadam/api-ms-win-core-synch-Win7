#include <Windows.h>

HRESULT CreateDXGIFactory2(
        UINT   Flags,
        REFIID riid,
        void   **ppFactory)
{
    return E_FAIL;
}

HRESULT CreateDXGIFactory1(
        REFIID riid,
        void   **ppFactory)
{
    return E_FAIL;
}

HRESULT CreateDXGIFactory(
        REFIID riid,
        void   **ppFactory)
{
    return E_FAIL;
}

#pragma comment(linker, "/export:CreateDXGIFactory")
#pragma comment(linker, "/export:CreateDXGIFactory1")
#pragma comment(linker, "/export:CreateDXGIFactory2")
