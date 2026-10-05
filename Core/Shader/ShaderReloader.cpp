//=============================================================================
//
// ShaderReloader処理 [ShaderReloader.cpp]
// Author : 
//
//=============================================================================
#include "ShaderReloader.h"

void ShaderReloader::WatchFile(const char* path)
{
    WIN32_FILE_ATTRIBUTE_DATA attr;
    if (GetFileAttributesExA(path, GetFileExInfoStandard, &attr))
    {
        uint64_t hash = CalculateFileHash(path);
        m_fileStateMap[path] = { attr.ftLastWriteTime, hash };
    }
}

bool ShaderReloader::HasFileChanged(const char* path)
{
    WIN32_FILE_ATTRIBUTE_DATA attr;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &attr)) return false;

    FILETIME newTime = attr.ftLastWriteTime;
    uint64_t newHash = CalculateFileHash(path);

    auto& state = m_fileStateMap[path];
    if (CompareFileTime(&newTime, &state.lastWriteTime) != 0 || newHash != state.contentHash)
    {
        state.lastWriteTime = newTime;
        state.contentHash = newHash;
        return true;
    }
    return false;
}

//bool ShaderReloader::HasFileChanged(const char* path)
//{
//    FILETIME newTime;
//    uint64_t newHash;
//
//    // ファイル属性を取得できなければ変更なし
//    if (!GetFileHashAndTime(path, newTime, newHash))
//    {
//        return false;
//    }
//
//    auto it = m_fileStateMap.find(path);
//
//    // 初回アクセスの場合は登録だけ行い、変更なしと判断
//    if (it == m_fileStateMap.end())
//    {
//        m_fileStateMap[path] = { newTime, newHash };
//        return false;
//    }
//
//    // 前回の記録と比較して違っていれば変更とみなす
//    if (CompareFileTime(&newTime, &it->value.lastWriteTime) != 0)
//    {
//        // 内容も変わっていた場合のみ true を返す
//        if (newHash != it->value.contentHash)
//        {
//            it->value.lastWriteTime = newTime;
//            it->value.contentHash = newHash;
//            return true; // 実際に中身が変わった！
//        }
//
//        // 時刻だけ変わったが内容は同じ → 更新しない
//        it->value.lastWriteTime = newTime; // 保持する
//
//        return false;
//    }
//
//    // N フレームごとに fallback ハッシュチェック
//    if (m_frameCount == 0)
//    {
//        if (newHash != it->value.contentHash)
//        {
//            it->value.contentHash = newHash;
//            return true; // 時刻が同じでも中身が変わった！
//        }
//    }
//
//    return false;
//}

//void ShaderReloader::AdvanceFrame(void)
//{
//    m_frameCount++;
//
//    // 指定のフレーム数に達したらリセット（次のフレームがチェック対象になる）
//    if (m_frameCount >= kHashFallbackInterval)
//    {
//        m_frameCount = 0;
//    }
//}

bool ShaderReloader::HasShaderContentChanged(const char* path, const char* entry, const void* blob, size_t size)
{
    ShaderSourceKey key = { path, entry };
    uint64_t newHash = CalculateBlobHash(blob, size);
    uint64_t& oldHash = m_blobHashMap[key];

    if (oldHash != newHash)
    {
        oldHash = newHash;
        return true; // 本当に変化した
    }
    return false; // blob は同じ
}

//bool ShaderReloader::GetFileHashAndTime(const char* path, FILETIME& outTime, uint64_t& outHash)
//{
//    WIN32_FILE_ATTRIBUTE_DATA attr;
//
//    // ファイル属性を取得
//    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &attr))
//        return false;
//
//    // 最終更新時刻を記録
//    outTime = attr.ftLastWriteTime;
//    outHash = CalculateFileHash(path);
//    return true;
//}

uint64_t ShaderReloader::CalculateBlobHash(const void* data, size_t size)
{
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

uint64_t ShaderReloader::CalculateFileHash(const char* path)
{
    FILE* file = nullptr;
    fopen_s(&file, path, "rb");
    if (!file) return 0;

    const size_t bufferSize = 4096;
    uint8_t buffer[bufferSize];
    uint64_t hash = 0xcbf29ce484222325ULL; // FNV-1a offset basis

    while (!feof(file))
    {
        size_t read = fread(buffer, 1, bufferSize, file);
        for (size_t i = 0; i < read; ++i)
        {
            hash ^= buffer[i];
            hash *= 1099511628211ULL; // FNV-1a prime
        }
    }

    fclose(file);
    return hash;
}
