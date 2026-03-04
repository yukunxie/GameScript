#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
    #if defined(GS_BUILDING_DLL)
        #define GS_API __declspec(dllexport)
    #elif defined(GS_USING_DLL)
        #define GS_API __declspec(dllimport)
    #else
        #define GS_API
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    #define GS_API __attribute__((visibility("default")))
#else
    #define GS_API
#endif
