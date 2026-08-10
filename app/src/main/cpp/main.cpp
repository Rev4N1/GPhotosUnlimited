#include <android/log.h>
#include <errno.h>
#include <stdint.h>
#include <sys/system_properties.h>
#include <unistd.h>

#include "zygisk.hpp"
#include "json/single_include/nlohmann/json.hpp"
#include "dobby.h"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "FGP/Native", __VA_ARGS__)

#define DEX_FILE_PATH "/data/adb/modules/unlimitedphotos/classes.dex"

#define PROP_FILE_PATH "/data/adb/modules/unlimitedphotos/fgp.prop"
#define CUSTOM_PROP_FILE_PATH "/data/adb/modules/unlimitedphotos/custom.fgp.prop"

#define JSON_FILE_PATH "/data/adb/modules/unlimitedphotos/fgp.json"
#define CUSTOM_JSON_FILE_PATH "/data/adb/modules/unlimitedphotos/custom.fgp.json"

#define PHOTOS_PACKAGE "com.google.android.apps.photos"

static int verboseLogs = 0;
static int spoofBuild = 1;
static int spoofProps = 0;
static int spoofProvider = 0;
static int spoofSignature = 0;
static int spoofFeatures = 1;

static std::map<std::string, std::string> jsonProps;

// std::stoi() calls std::abort() instead of throwing here (built with -fno-exceptions), which
// would take Google Photos down over a typo in the config, so parse the value by hand.
static bool parseInt(const std::string &str, int &out) {
    size_t pos = 0;
    bool negative = false;
    if (!str.empty() && (str[0] == '-' || str[0] == '+')) {
        negative = str[0] == '-';
        pos = 1;
    }
    if (pos >= str.size()) return false;
    int value = 0;
    for (; pos < str.size(); pos++) {
        if (str[pos] < '0' || str[pos] > '9') return false;
        value = value * 10 + (str[pos] - '0');
    }
    out = negative ? -value : value;
    return true;
}

// Trailing whitespace in a config value ends up spoofed verbatim into a Build field otherwise.
static void trim(std::string &str) {
    size_t last = str.find_last_not_of(" \t");
    if (last == std::string::npos) {
        str.clear();
        return;
    }
    str.resize(last + 1);
    str.erase(0, str.find_first_not_of(" \t"));
}

// Reads one integer "Advanced Settings" entry, if present, then drops it from the parsed config
// so only Build field names are left over to hand to the Java side.
static void readSetting(nlohmann::json &json, const char *name, int &target, const char *description) {
    if (!json.contains(name)) return;
    auto value = json[name];
    if (!value.is_null() && value.is_string() && value != "" && parseInt(value.get<std::string>(), target)) {
        if (verboseLogs > 0 && description != nullptr) {
            LOGD("%s %s!", description, (target > 0) ? "enabled" : "disabled");
        }
    } else {
        LOGD("Error parsing %s!", name);
    }
    json.erase(name);
}

// A single read()/write() can come up short on a socket, which would leave a truncated dex or
// config behind, so keep going until the whole payload has moved.
static bool transferFull(int fd, void *buffer, size_t size, bool writing) {
    auto *ptr = static_cast<uint8_t *>(buffer);
    while (size > 0) {
        ssize_t moved = writing ? write(fd, ptr, size) : read(fd, ptr, size);
        if (moved <= 0) {
            if (moved < 0 && errno == EINTR) continue;
            return false;
        }
        ptr += moved;
        size -= static_cast<size_t>(moved);
    }
    return true;
}

static bool readFull(int fd, void *buffer, size_t size) {
    return transferFull(fd, buffer, size, false);
}

static bool writeFull(int fd, void *buffer, size_t size) {
    return transferFull(fd, buffer, size, true);
}

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static std::map<void *, T_Callback> callbacks;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {
    if (cookie == nullptr || name == nullptr || value == nullptr || !callbacks.contains(cookie)) return;

    const char *oldValue = value;

    std::string prop(name);

    if (jsonProps.count(prop)) {
        // Exact property match
        value = jsonProps[prop].c_str();
    } else {
        // Leading * wildcard property match
        for (const auto &p: jsonProps) {
            if (p.first.starts_with("*") && prop.ends_with(p.first.substr(1))) {
                value = p.second.c_str();
                break;
            }
        }
    }

    if (oldValue == value) {
        if (verboseLogs > 99) LOGD("[%s]: %s (unchanged)", name, oldValue);
    } else {
        LOGD("[%s]: %s -> %s", name, oldValue, value);
    }

    return callbacks[cookie](cookie, name, value, serial);
}

static void (*o_system_property_read_callback)(const prop_info *, T_Callback, void *);

static void my_system_property_read_callback(const prop_info *pi, T_Callback callback, void *cookie) {
    if (pi == nullptr || callback == nullptr || cookie == nullptr) {
        return o_system_property_read_callback(pi, callback, cookie);
    }
    callbacks[cookie] = callback;
    return o_system_property_read_callback(pi, modify_callback, cookie);
}

static void doHook() {
    void *handle = DobbySymbolResolver(nullptr, "__system_property_read_callback");
    if (handle == nullptr) {
        LOGD("Couldn't find '__system_property_read_callback' handle");
        return;
    }
    LOGD("Found '__system_property_read_callback' handle at %p", handle);
    DobbyHook(handle, reinterpret_cast<dobby_dummy_func_t>(my_system_property_read_callback),
        reinterpret_cast<dobby_dummy_func_t *>(&o_system_property_read_callback));
}

static void setFieldNative(JNIEnv *env, jclass /* clazz_EntryPoint */, jclass targetClass, jobject fieldObj, jstring typeObj, jobject valueObj) {
    if (!targetClass || !fieldObj || !typeObj) return;

    jfieldID fieldID = env->FromReflectedField(fieldObj);
    if (!fieldID) return;

    const char *typeName = env->GetStringUTFChars(typeObj, nullptr);

    if (strcmp(typeName, "java.lang.String") == 0) {
        env->SetStaticObjectField(targetClass, fieldID, valueObj);
    } else if (strcmp(typeName, "int") == 0) {
        jclass intClass = env->FindClass("java/lang/Integer");
        jmethodID intValue = env->GetMethodID(intClass, "intValue", "()I");
        jint val = env->CallIntMethod(valueObj, intValue);
        env->SetStaticIntField(targetClass, fieldID, val);
    } else if (strcmp(typeName, "long") == 0) {
        jclass longClass = env->FindClass("java/lang/Long");
        jmethodID longValue = env->GetMethodID(longClass, "longValue", "()J");
        jlong val = env->CallLongMethod(valueObj, longValue);
        env->SetStaticLongField(targetClass, fieldID, val);
    } else if (strcmp(typeName, "boolean") == 0) {
        jclass boolClass = env->FindClass("java/lang/Boolean");
        jmethodID booleanValue = env->GetMethodID(boolClass, "booleanValue", "()Z");
        jboolean val = env->CallBooleanMethod(valueObj, booleanValue);
        env->SetStaticBooleanField(targetClass, fieldID, val);
    }

    env->ReleaseStringUTFChars(typeObj, typeName);
}

class GPhotosUnlimited : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        bool isPhotos = false, isPhotosProc = false;

        // Prevent crash on apps with no nice name/data dir: GetStringUTFChars() aborts on a null
        // jstring, so these have to be checked before the conversion rather than after it.
        if (args == nullptr || args->nice_name == nullptr || args->app_data_dir == nullptr) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        auto rawProcess = env->GetStringUTFChars(args->nice_name, nullptr);
        auto rawDir = env->GetStringUTFChars(args->app_data_dir, nullptr);

        if (rawProcess == nullptr || rawDir == nullptr) {
            if (rawProcess != nullptr) env->ReleaseStringUTFChars(args->nice_name, rawProcess);
            if (rawDir != nullptr) env->ReleaseStringUTFChars(args->app_data_dir, rawDir);
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        std::string_view process(rawProcess);
        std::string_view dir(rawDir);

		isPhotos = dir.ends_with("/" PHOTOS_PACKAGE);
		isPhotosProc = process == PHOTOS_PACKAGE;

        env->ReleaseStringUTFChars(args->nice_name, rawProcess);
        env->ReleaseStringUTFChars(args->app_data_dir, rawDir);

        if (!isPhotos) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        // We are in Google Photos now, force unmount
        api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

        if (!isPhotosProc) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        std::vector<char> configVector;
        long dexSize = 0, configSize = 0;

        int fd = api->connectCompanion();

        if (!readFull(fd, &dexSize, sizeof(long)) || !readFull(fd, &configSize, sizeof(long))) {
            close(fd);
            LOGD("Couldn't read sizes from companion");
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        if (dexSize < 1) {
            close(fd);
            LOGD("Couldn't read dex file");
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        if (configSize < 1) {
            close(fd);
            LOGD("Couldn't read config file");
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        LOGD("Read from file descriptor for 'dex' -> %ld bytes", dexSize);
        LOGD("Read from file descriptor for 'config' -> %ld bytes", configSize);

        dexVector.resize(dexSize);
        configVector.resize(configSize);

        if (!readFull(fd, dexVector.data(), dexSize) || !readFull(fd, configVector.data(), configSize)) {
            close(fd);
            LOGD("Couldn't read dex/config payload from companion");
            dexVector.clear();  // postAppSpecialize() keys off this to skip a truncated dex
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        close(fd);

        std::string configString(configVector.cbegin(), configVector.cend());

        if (!nlohmann::json::accept(configString, true)) {
            LOGD("Converting config from prop format to JSON format");

            configString.erase(std::remove(configString.begin(), configString.end(), '\r'), configString.end());

            std::string jsonString = "{";
            char propDelimiter = '=';
            char commentDelimiter = '#';
            size_t beginPos = 0, endPos = 0;
            // A final line with no trailing newline still counts, otherwise the last entry of
            // every config would be silently dropped.
            while (beginPos < configString.size()) {
                endPos = configString.find('\n', beginPos);
                std::string line;
                if (endPos == std::string::npos) {
                    line = configString.substr(beginPos);
                    beginPos = configString.size();
                } else {
                    line = configString.substr(beginPos, endPos - beginPos);
                    beginPos = endPos + 1;
                }
                trim(line);
                if (line.empty() || line[0] == '#') continue;
                std::string name, value;
                size_t propDelimiterPos = line.find(propDelimiter);
                if (propDelimiterPos != std::string::npos) {
                    name = line.substr(0, propDelimiterPos);
                    value = line.substr(propDelimiterPos + 1);
                } else {
                    LOGD("Invalid prop entry, skipping");
                    continue;
                }
                size_t commentDelimiterPos = value.find(commentDelimiter);
                if (commentDelimiterPos != std::string::npos) {
                    value = value.substr(0, commentDelimiterPos);
                }
                trim(name);
                trim(value);
                jsonString += "\n\"" + name + "\": \"" + value + "\",";
            }
            if (jsonString.back() == ',') jsonString.pop_back();
            jsonString += "\n}\n";

            configString = jsonString;
        }

        json = nlohmann::json::parse(configString, nullptr, false, true);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (dexVector.empty() || json.empty()) return;

        readJson();

        if (spoofProps > 0) doHook();
        if (spoofBuild + spoofProvider + spoofSignature + spoofFeatures > 0) inject();

        dexVector.clear();
        json.clear();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    std::vector<char> dexVector;
    nlohmann::json json;
    std::string pkgName;

    void readJson() {
        LOGD("JSON contains %d keys!", static_cast<int>(json.size()));

        // Verbose logging level
        readSetting(json, "verboseLogs", verboseLogs, nullptr);
        if (verboseLogs > 0) LOGD("Verbose logging (level %d) enabled!", verboseLogs);

        // Advanced spoofing settings
        readSetting(json, "spoofBuild", spoofBuild, "Spoofing Build Fields");
        readSetting(json, "spoofProps", spoofProps, "Spoofing System Properties");
        readSetting(json, "spoofProvider", spoofProvider, "Spoofing Keystore Provider");
        readSetting(json, "spoofSignature", spoofSignature, "Spoofing ROM Signature");
        readSetting(json, "spoofFeatures", spoofFeatures, "Spoofing Pixel System Features");

        std::vector<std::string> eraseKeys;
        for (auto &jsonList: json.items()) {
            if (verboseLogs > 1) LOGD("Parsing %s", jsonList.key().c_str());
            if (jsonList.key().find_first_of("*.") != std::string::npos) {
                // Name contains . or * (wildcard) so assume real property name
                if (!jsonList.value().is_null() && jsonList.value().is_string()) {
                    if (jsonList.value() == "") {
                        LOGD("%s is empty, skipping", jsonList.key().c_str());
                    } else {
                        if (verboseLogs > 0) LOGD("Adding '%s' to properties list", jsonList.key().c_str());
                        jsonProps[jsonList.key()] = jsonList.value();
                    }
                } else {
                    LOGD("Error parsing %s!", jsonList.key().c_str());
                }
                eraseKeys.push_back(jsonList.key());
            }
        }
        // Remove properties from parsed JSON
        for (auto key: eraseKeys) {
            if (json.contains(key)) json.erase(key);
        }
    }

    // Leaving a pending JNI exception behind (or calling a null method ID, which aborts) would
    // crash Google Photos on the next JNI transition, so bail out cleanly instead. This is what
    // a stale classes.dex from a partially applied module update looks like from here.
    bool jniFailed(const char *what) {
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            LOGD("JNI: %s", what);
            return true;
        }
        return false;
    }

    void inject() {
		LOGD("JNI: Getting system classloader");
		auto clClass = env->FindClass("java/lang/ClassLoader");
		auto getSystemClassLoader = env->GetStaticMethodID(clClass, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
		auto systemClassLoader = env->CallStaticObjectMethod(clClass, getSystemClassLoader);
		if (jniFailed("Couldn't get system classloader")) return;

		LOGD("JNI: Creating module classloader");
		auto dexClClass = env->FindClass("dalvik/system/InMemoryDexClassLoader");
		auto dexClInit = env->GetMethodID(dexClClass, "<init>", "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
		auto buffer = env->NewDirectByteBuffer(dexVector.data(), static_cast<jlong>(dexVector.size()));
		auto dexCl = env->NewObject(dexClClass, dexClInit, buffer, systemClassLoader);
		if (jniFailed("Couldn't create module classloader")) return;

		LOGD("JNI: Loading module class");
		auto loadClass = env->GetMethodID(clClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
		auto entryClassName = env->NewStringUTF("com.rev4n.unlimitedphotos.EntryPoint");
		auto entryClassObj = env->CallObjectMethod(dexCl, loadClass, entryClassName);
		if (jniFailed("Couldn't load module class")) return;

        auto entryClass = (jclass) entryClassObj;

        JNINativeMethod methods[] = {
            {"setFieldNative", "(Ljava/lang/Class;Ljava/lang/reflect/Field;Ljava/lang/String;Ljava/lang/Object;)V", (void*) setFieldNative}
        };
        if (env->RegisterNatives(entryClass, methods, 1) != JNI_OK) {
            jniFailed("Couldn't register setFieldNative");
            return;
        }

		LOGD("JNI: Sending JSON");
		auto receiveJson = env->GetStaticMethodID(entryClass, "receiveJson", "(Ljava/lang/String;)V");
		if (jniFailed("Couldn't find EntryPoint.receiveJson")) return;
		auto javaStr = env->NewStringUTF(json.dump().c_str());
		env->CallStaticVoidMethod(entryClass, receiveJson, javaStr);
		if (jniFailed("EntryPoint.receiveJson threw")) return;

		LOGD("JNI: Calling EntryPoint.init");
		auto entryInit = env->GetStaticMethodID(entryClass, "init", "(IIIII)V");
		if (jniFailed("Couldn't find EntryPoint.init")) return;
		env->CallStaticVoidMethod(entryClass, entryInit, verboseLogs, spoofBuild, spoofProvider, spoofSignature, spoofFeatures);
		jniFailed("EntryPoint.init threw");
		env->DeleteLocalRef(javaStr);

        env->DeleteLocalRef(clClass);
        env->DeleteLocalRef(systemClassLoader);
        env->DeleteLocalRef(dexClClass);
        env->DeleteLocalRef(buffer);
        env->DeleteLocalRef(dexCl);
        env->DeleteLocalRef(entryClassName);
        env->DeleteLocalRef(entryClassObj);
    }
};



// Slurps a whole file, reporting the number of bytes actually read rather than the file size, so
// a short read can't leave uninitialised tail bytes to be sent as if they were real content.
static long readFile(FILE *file, std::vector<char> &out) {
    if (!file) return 0;
    long size = 0;
    if (fseek(file, 0, SEEK_END) == 0) {
        size = ftell(file);
        if (fseek(file, 0, SEEK_SET) != 0) size = 0;
    }
    if (size > 0) {
        out.resize(size);
        out.resize(fread(out.data(), 1, size, file));
    }
    fclose(file);
    return static_cast<long>(out.size());
}

static void companion(int fd) {
    std::vector<char> dexVector, configVector;

    long dexSize = readFile(fopen(DEX_FILE_PATH, "rb"), dexVector);

    FILE *config = fopen(CUSTOM_PROP_FILE_PATH, "r");
    if (!config)
        config = fopen(CUSTOM_JSON_FILE_PATH, "r");
    if (!config)
        config = fopen(PROP_FILE_PATH, "r");

    long configSize = readFile(config, configVector);

    if (!writeFull(fd, &dexSize, sizeof(long)) || !writeFull(fd, &configSize, sizeof(long))
            || !writeFull(fd, dexVector.data(), dexSize)
            || !writeFull(fd, configVector.data(), configSize)) {
        LOGD("Couldn't write dex/config to the module");
    }
}

REGISTER_ZYGISK_MODULE(GPhotosUnlimited)

REGISTER_ZYGISK_COMPANION(companion)
