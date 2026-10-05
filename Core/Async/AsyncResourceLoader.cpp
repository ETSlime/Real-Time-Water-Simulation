#include "AsyncResourceLoader.h"
//=============================================================================
//
// [AsyncResourceLoader.cpp]
// Author : 
//
//
//=============================================================================

AsyncResourceLoader::AsyncResourceLoader() : m_thread(NULL), m_done(false), m_progress(0),
m_fileList(nullptr), m_fileCount(0), m_loadedData(nullptr) {}

AsyncResourceLoader::~AsyncResourceLoader()
{
    if (m_thread) 
        CloseHandle(m_thread);

    if (m_loadedData) 
    {
        for (int i = 0; i < m_fileCount; ++i)
            free(m_loadedData[i]);
        SAFE_DELETE_ARRAY(m_loadedData);
    }
}

void AsyncResourceLoader::SetFileList(const char** files, int count)
{
    m_fileList = files;
    m_fileCount = count;
    m_loadedData = new void* [count];
    ZeroMemory(m_loadedData, sizeof(void*) * count);
}

void AsyncResourceLoader::Start()
{
    m_done = false;
    m_progress = 0;
    m_thread = CreateThread(nullptr, 0, ThreadProc, this, 0, nullptr);
}

DWORD WINAPI AsyncResourceLoader::ThreadProc(LPVOID param)
{
    static_cast<AsyncResourceLoader*>(param)->LoadResources();
    return 0;
}

void AsyncResourceLoader::LoadResources()
{
    for (int i = 0; i < m_fileCount; ++i) {
        FILE* file = fopen(m_fileList[i], "rb");
        if (!file) continue;

        fseek(file, 0, SEEK_END);
        size_t size = ftell(file);
        fseek(file, 0, SEEK_SET);

        m_loadedData[i] = malloc(size);
        fread(m_loadedData[i], 1, size, file);
        fclose(file);

        m_progress = ((i + 1) * 100) / m_fileCount;
    }

    m_done = true;
}