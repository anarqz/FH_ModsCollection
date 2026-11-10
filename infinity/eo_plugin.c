/* Infinity build (INFINITY_EO): Spotify Radio as an ExtraOptions plugin instead of an Azimuth Mod Loader
   mod. ExtraOptions' loader (plugin ABI 1, extra-options/fh2/fh2_plugin.h) loads
   <Azimuth>\mods\<game folder>\<id>\plugins\FH_SpotifyRadio.dll and calls FH2PluginInitialize on its worker
   thread once the game runs. This hands the mod's start-up (spotify_mod.c) the AzimuthModHost it was
   written for: data_dir is the mod's ExtraOptions folder (the parent of plugins\), mods_dir <Azimuth>\mods\. */
#include "extra_options.h"
#include "azimuth_mod.h"
#include "fh2/fh2_plugin.h"
#include "infinity_eo.h"
#include <tlhelp32.h>

int spotify_radio_load(const AzimuthModHost *host);

static wchar_t data_dir[PATH_CAPACITY], mods_dir[PATH_CAPACITY];
static const wchar_t no_game_exe[] = L""; // only the logo copy read it, and this build has none

static int is_separator(wchar_t c) {
    return c == L'\\' || c == L'/';
}

static int component_is(const wchar_t *name, size_t length, const wchar_t *lower) {
    size_t i = 0;
    for (; i < length && lower[i]; ++i) {
        wchar_t c = name[i] >= L'A' && name[i] <= L'Z' ? (wchar_t)(name[i] + 32) : name[i];
        if (c != lower[i]) {
            return 0;
        }
    }
    return i == length && !lower[i];
}

// Drops trailing separators from path[0..*length) and returns where its last component starts.
static size_t last_component(const wchar_t *path, size_t *length) {
    while (*length && is_separator(path[*length - 1])) {
        --*length;
    }
    size_t start = *length;
    while (start && !is_separator(path[start - 1])) {
        --start;
    }
    return start;
}

/* data_dir: plugins_directory without its last component "plugins" (trailing separator kept).
   mods_dir: the nearest "mods" folder above the mod's own (mods\Forza Horizon 2\<id>\ or mods\<id>\). */
static int find_folders(const wchar_t *plugins_directory) {
    size_t length = 0;
    while (plugins_directory[length]) {
        if (++length >= PATH_CAPACITY - 1) {
            return 0;
        }
    }
    for (size_t i = 0; i < length; ++i) {
        data_dir[i] = plugins_directory[i];
    }
    size_t start = last_component(data_dir, &length);
    if (!start || !component_is(data_dir + start, length - start, L"plugins")) {
        return 0;
    }
    data_dir[start] = 0;
    for (size_t i = 0; i <= start; ++i) {
        mods_dir[i] = data_dir[i];
    }
    length = start;
    length = last_component(mods_dir, &length); // above the mod's own folder
    while (length) {
        size_t parent = last_component(mods_dir, &length);
        if (component_is(mods_dir + parent, length - parent, L"mods")) {
            mods_dir[length] = L'\\';
            mods_dir[length + 1] = 0;
            return 1;
        }
        length = parent;
    }
    return 0;
}

__declspec(dllexport) BOOL WINAPI FH2PluginInitialize(const FH2PluginContext *context) {
    if (!context || context->size < sizeof(*context) || context->api_version != FH2_PLUGIN_API_VERSION) {
        return FALSE;
    }
    if (context->adapter != FH2_PLUGIN_AZIMUTH || !context->game_image || !context->plugins_directory ||
        !find_folders(context->plugins_directory)) {
        if (context->log_utf8) {
            context->log_utf8("Spotify Radio: not started (not Azimuth, or not in <mods>\\...\\<id>\\plugins\\).\r\n");
        }
        return FALSE;
    }
    static AzimuthModHost host;
    host.size = sizeof(host);
    host.api_version = AZIMUTH_MOD_API_VERSION;
    host.game_image = (unsigned char *)context->game_image;
    host.game_exe = no_game_exe;
    host.mods_dir = mods_dir;
    host.data_dir = data_dir;
    host.game = AZIMUTH_GAME_FORZA_HORIZON_2; // ExtraOptions.Infinity runs Forza Horizon 2 only
    return spotify_radio_load(&host) ? TRUE : FALSE;
}

#define FROZEN_CAPACITY 1024
static HANDLE frozen[FROZEN_CAPACITY];
static unsigned frozen_count;
static CONTEXT frozen_context; // CONTEXT is 16-byte aligned by its declaration

void infinity_freeze_threads(const unsigned char *target, size_t length, const unsigned char *trampoline) {
    frozen_count = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD process = GetCurrentProcessId(), self = GetCurrentThreadId();
    THREADENTRY32 entry;
    entry.dwSize = sizeof(entry);
    for (BOOL more = Thread32First(snapshot, &entry); more && frozen_count < FROZEN_CAPACITY;
         entry.dwSize = sizeof(entry), more = Thread32Next(snapshot, &entry)) {
        if (entry.th32OwnerProcessID != process || entry.th32ThreadID == self) {
            continue;
        }
        HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, entry.th32ThreadID);
        if (!thread) {
            continue;
        }
        if (SuspendThread(thread) == (DWORD)-1) {
            CloseHandle(thread);
            continue;
        }
        frozen[frozen_count++] = thread;
        frozen_context.ContextFlags = CONTEXT_CONTROL; // GetThreadContext also waits until the thread is stopped
        uintptr_t start = (uintptr_t)target;
        if (GetThreadContext(thread, &frozen_context) && frozen_context.Rip >= start && frozen_context.Rip < start + length) {
            frozen_context.Rip = (uintptr_t)trampoline + (frozen_context.Rip - start);
            SetThreadContext(thread, &frozen_context);
        }
    }
    CloseHandle(snapshot);
}

void infinity_thaw_threads(void) {
    while (frozen_count) {
        HANDLE thread = frozen[--frozen_count];
        ResumeThread(thread);
        CloseHandle(thread);
    }
}
