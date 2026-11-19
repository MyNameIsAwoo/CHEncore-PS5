// Copyright (C) 2026 Mihawk
// SPDX-License-Identifier: MIT
#include "song_package.hpp"
#include "../../../external/miniz/miniz_tinfl.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <vector>
#include <set>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
namespace chorus {
namespace {
constexpr uint64_t MaxChartBytes=256ull*1024*1024,MaxExpandedBytes=384ull*1024*1024;
constexpr uint64_t MaxSingleFileBytes=256ull*1024*1024,MaxSngHeaderBytes=16ull*1024*1024,MaxFileCount=2048;
bool ensureDirectory(const char* path) noexcept {
    if (!path || !*path) return false;
    char buffer[512]{};
    const size_t len = std::strlen(path);
    if (len >= sizeof(buffer)) return false;
    std::memcpy(buffer, path, len + 1);
    for (size_t i = 1; i <= len; ++i) {
        if (buffer[i] != '/' && buffer[i] != '\0') continue;
        const char saved = buffer[i];
        buffer[i] = '\0';
        if (mkdir(buffer, 0775) != 0 && errno != EEXIST) return false;
        buffer[i] = saved;
    }
    DIR* directory = opendir(path);
    if (!directory) return false;
    closedir(directory);
    return true;
}

void removeTree(const char* path) noexcept {
    DIR* dir = opendir(path);
    if (!dir) { unlink(path); return; }
    while (dirent* entry = readdir(dir)) {
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) continue;
        char child[768];
        const int n = std::snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        if (n <= 0 || static_cast<size_t>(n) >= sizeof(child)) continue;
        DIR* childDirectory = opendir(child);
        if (childDirectory) {
            closedir(childDirectory);
            removeTree(child);
        } else {
            (void)unlink(child);
        }
    }
    closedir(dir);
    rmdir(path);
}

bool safeRelativePath(const std::string& value) noexcept {
    if (value.empty() || value.size() > 240 || value.front() == '/' || value.front() == '\\') return false;
    size_t segment = 0;
    for (size_t i = 0; i <= value.size(); ++i) {
        if (i < value.size()) {
            const unsigned char c = static_cast<unsigned char>(value[i]);
            if (c < 0x20 || c == 0x7f || c == ':' || c == '\\') return false;
        }
        if (i == value.size() || value[i] == '/') {
            const size_t count = i - segment;
            if (!count || (count == 1 && value[segment] == '.') ||
                (count == 2 && value[segment] == '.' && value[segment + 1] == '.')) return false;
            segment = i + 1;
        }
    }
    return true;
}

uint64_t readU64(const std::vector<uint8_t>& bytes, size_t at) noexcept {
    uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) value |= static_cast<uint64_t>(bytes[at + i]) << (8u * i);
    return value;
}

uint32_t readU32(const std::vector<uint8_t>& bytes, size_t at) noexcept {
    uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= static_cast<uint32_t>(bytes[at + i]) << (8u * i);
    return value;
}

bool addWithin(uint64_t start, uint64_t size, uint64_t limit) noexcept {
    return start <= limit && size <= limit - start;
}

struct SngFile {
    std::string name;
    uint64_t length{0};
    uint64_t index{0};
};

bool writeIni(const char* path, const std::vector<std::pair<std::string, std::string>>& metadata) noexcept {
    FILE* out = std::fopen(path, "wb");
    if (!out) return false;
    bool ok = std::fputs("[song]\n", out) >= 0;
    for (const auto& field : metadata) {
        if (field.first.empty() || field.first.size() > 128 || field.second.size() > 2048) continue;
        bool clean = true;
        for (unsigned char c : field.first) clean &= c >= 0x20 && c != ';' && c != '=' && c != '[' && c != ']';
        for (unsigned char c : field.second) clean &= c != '\r' && c != '\n' && c != ';';
        if (!clean) continue;
        if (std::fprintf(out, "%s = %s\n", field.first.c_str(), field.second.c_str()) < 0) ok = false;
    }
    if (std::fflush(out) != 0 || fsync(fileno(out)) != 0) ok = false;
    if (std::fclose(out) != 0) ok = false;
    return ok;
}

struct ExtractedFile {std::string name;uint64_t bytes;};
struct PackageInstaller {
    std::string temporarySng_,stageDirectory_;
    std::string& error;
    const std::atomic<bool>* cancel{};
    std::vector<ExtractedFile> extractedFiles{};
    bool cancelled() const {return cancel&&cancel->load(std::memory_order_relaxed);}
    bool fail(const char* text){error=text;return false;}
    bool extractSng() noexcept;
    bool extractZip();
};
bool PackageInstaller::extractSng() noexcept {
    FILE* in = std::fopen(temporarySng_.c_str(), "rb");
    if (!in) { fail("Cannot read the downloaded chart package."); return false; }
    if (fseeko(in, 0, SEEK_END) != 0) { std::fclose(in); fail("Cannot seek in chart package."); return false; }
    const off_t end = ftello(in);
    if (end < 0 || static_cast<uint64_t>(end) > MaxChartBytes || static_cast<uint64_t>(end) < 50 ||
        fseeko(in, 0, SEEK_SET) != 0) {
        std::fclose(in); fail("Chart package size is invalid."); return false;
    }
    const uint64_t fileSize = static_cast<uint64_t>(end);
    std::vector<uint8_t> header;
    const size_t headerLimit = static_cast<size_t>(std::min<uint64_t>(fileSize, MaxSngHeaderBytes));
    header.resize(headerLimit);
    if (std::fread(header.data(), 1, header.size(), in) != header.size()) {
        std::fclose(in); fail("Chart package header is truncated."); return false;
    }
    if (std::memcmp(header.data(), "SNGPKG", 6) != 0 || readU32(header, 6) != 1) {
        std::fclose(in); fail("Unsupported chart package format."); return false;
    }
    std::array<uint8_t, 16> xorMask{};
    std::copy_n(header.data() + 10, xorMask.size(), xorMask.data());
    constexpr uint64_t metadataLenOffset = 26;
    if (!addWithin(metadataLenOffset, 8, header.size())) { std::fclose(in); fail("Invalid chart metadata."); return false; }
    const uint64_t metadataLen = readU64(header, metadataLenOffset);
    const uint64_t metadataStart = metadataLenOffset + 8;
    if (metadataLen < 8 || !addWithin(metadataStart, metadataLen, header.size())) {
        std::fclose(in); fail("Invalid chart metadata length."); return false;
    }
    const uint64_t metadataCount = readU64(header, static_cast<size_t>(metadataStart));
    uint64_t pos = metadataStart + 8;
    const uint64_t metadataEnd = metadataStart + metadataLen;
    if (metadataCount > 4096) { std::fclose(in); fail("Chart metadata count is invalid."); return false; }
    std::vector<std::pair<std::string, std::string>> metadata;
    for (uint64_t i = 0; i < metadataCount; ++i) {
        if (!addWithin(pos, 4, metadataEnd)) { std::fclose(in); fail("Truncated chart metadata."); return false; }
        const int32_t keyLen = static_cast<int32_t>(readU32(header, static_cast<size_t>(pos))); pos += 4;
        if (keyLen < 0 || keyLen > 4096 || !addWithin(pos, static_cast<uint64_t>(keyLen) + 4, metadataEnd)) {
            std::fclose(in); fail("Invalid chart metadata key."); return false;
        }
        std::string key(reinterpret_cast<const char*>(header.data() + pos), static_cast<size_t>(keyLen)); pos += static_cast<uint64_t>(keyLen);
        const int32_t valueLen = static_cast<int32_t>(readU32(header, static_cast<size_t>(pos))); pos += 4;
        if (valueLen < 0 || valueLen > 16384 || !addWithin(pos, static_cast<uint64_t>(valueLen), metadataEnd)) {
            std::fclose(in); fail("Invalid chart metadata value."); return false;
        }
        std::string value(reinterpret_cast<const char*>(header.data() + pos), static_cast<size_t>(valueLen)); pos += static_cast<uint64_t>(valueLen);
        metadata.emplace_back(std::move(key), std::move(value));
    }
    const uint64_t fileMetaLenOffset = metadataEnd;
    if (!addWithin(fileMetaLenOffset, 8, header.size())) { std::fclose(in); fail("Missing chart file index."); return false; }
    const uint64_t fileMetaLen = readU64(header, static_cast<size_t>(fileMetaLenOffset));
    const uint64_t fileMetaStart = fileMetaLenOffset + 8;
    if (fileMetaLen < 8 || !addWithin(fileMetaStart, fileMetaLen, header.size())) {
        std::fclose(in); fail("Invalid chart file index length."); return false;
    }
    const uint64_t fileCount = readU64(header, static_cast<size_t>(fileMetaStart));
    if (fileCount == 0 || fileCount > MaxFileCount) { std::fclose(in); fail("Chart package file count is invalid."); return false; }
    pos = fileMetaStart + 8;
    const uint64_t fileMetaEnd = fileMetaStart + fileMetaLen;
    std::vector<SngFile> files;
    files.reserve(static_cast<size_t>(fileCount));
    uint64_t expandedBytes = 0;
    std::set<std::string> paths;
    for (uint64_t i = 0; i < fileCount; ++i) {
        if (!addWithin(pos, 1, fileMetaEnd)) { std::fclose(in); fail("Truncated chart file index."); return false; }
        const uint8_t nameLength = header[static_cast<size_t>(pos++)];
        if (!nameLength || !addWithin(pos, static_cast<uint64_t>(nameLength) + 16, fileMetaEnd)) {
            std::fclose(in); fail("Invalid chart filename in package."); return false;
        }
        SngFile file;
        file.name.assign(reinterpret_cast<const char*>(header.data() + pos), nameLength);
        pos += nameLength;
        file.length = readU64(header, static_cast<size_t>(pos)); pos += 8;
        file.index = readU64(header, static_cast<size_t>(pos)); pos += 8;
        if (!safeRelativePath(file.name) || file.length > MaxSingleFileBytes ||
            !addWithin(expandedBytes, file.length, MaxExpandedBytes)) {
            std::fclose(in); fail("Chart package contains an unsafe or oversized file."); return false;
        }
        std::string folded=file.name;for(char& c:folded)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if(!paths.insert(folded).second){std::fclose(in);return fail("Duplicate SNG output path.");}
        expandedBytes += file.length;
        files.push_back(std::move(file));
    }
    const uint64_t fileDataLenOffset = fileMetaEnd;
    if (!addWithin(fileDataLenOffset, 8, header.size())) { std::fclose(in); fail("Missing chart file data."); return false; }
    const uint64_t fileDataLength = readU64(header, static_cast<size_t>(fileDataLenOffset));
    const uint64_t fileDataStart = fileDataLenOffset + 8;
    if (fileDataStart > fileSize || fileDataLength != fileSize - fileDataStart) {
        std::fclose(in); fail("Chart file data is truncated."); return false;
    }

    bool chartFound = false;
    std::array<uint8_t, 65536> data{};
    for (const SngFile& file : files) {
        if(cancelled()) {std::fclose(in);return fail("Install cancelled.");}
        if (file.index < fileDataStart ||
            !addWithin(file.index - fileDataStart, file.length, fileDataLength)) {
            std::fclose(in); fail("Chart file index is outside package data."); return false;
        }
        char outputPath[768];
        const int n = std::snprintf(outputPath, sizeof(outputPath), "%s/%s", stageDirectory_.c_str(), file.name.c_str());
        if (n <= 0 || static_cast<size_t>(n) >= sizeof(outputPath)) { std::fclose(in); fail("Chart file path is too long."); return false; }
        char parent[768];
        std::memcpy(parent, outputPath, static_cast<size_t>(n) + 1);
        char* slash = std::strrchr(parent, '/');
        if (!slash) { std::fclose(in); fail("Invalid chart output path."); return false; }
        *slash = '\0';
        if (!ensureDirectory(parent)) { std::fclose(in); fail("Cannot create chart subfolder."); return false; }
        FILE* out = std::fopen(outputPath, "wb");
        if (!out) { std::fclose(in); fail("Cannot create extracted chart file."); return false; }
        uint64_t offset = 0;
        bool ok = true;
        while (offset < file.length) {
            if(cancelled()){ok=false;break;}
            const size_t chunk = static_cast<size_t>(std::min<uint64_t>(data.size(), file.length - offset));
            const uint64_t absoluteOffset = file.index + offset;
            if (absoluteOffset > static_cast<uint64_t>(INT64_MAX) || fseeko(in, static_cast<off_t>(absoluteOffset), SEEK_SET) != 0 ||
                std::fread(data.data(), 1, chunk, in) != chunk) { ok = false; break; }
            for (size_t j = 0; j < chunk; ++j) {
                const uint8_t cyclic = static_cast<uint8_t>((offset + j) & 0xffu);
                data[j] ^= static_cast<uint8_t>(xorMask[cyclic & 15u] ^ cyclic);
            }
            if (std::fwrite(data.data(), 1, chunk, out) != chunk) { ok = false; break; }
            offset += chunk;
        }
        if (std::fflush(out) != 0 || fsync(fileno(out)) != 0) ok = false;
        if (std::fclose(out) != 0) ok = false;
        if (!ok) { std::fclose(in); fail("Could not extract chart file."); return false; }
        extractedFiles.push_back({file.name,file.length});
        const size_t dot = file.name.find_last_of('.');
        if (dot != std::string::npos && file.name.substr(dot) == ".chart") chartFound = true;
    }
    std::fclose(in);
    const std::string iniPath = stageDirectory_ + "/song.ini";
    struct stat iniStat{};
    if (stat(iniPath.c_str(), &iniStat) != 0 && !writeIni(iniPath.c_str(), metadata)) {
        fail("Could not write chart metadata."); return false;
    }
    (void)chartFound;
    return true;
}

uint16_t u16(const uint8_t* p){return static_cast<uint16_t>(p[0]|(p[1]<<8));}
uint32_t u32(const uint8_t* p){return p[0]|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
bool readAt(FILE* file,uint64_t at,void* data,size_t size){return at<=INT64_MAX&&fseeko(file,static_cast<off_t>(at),SEEK_SET)==0&&std::fread(data,1,size,file)==size;}
constexpr auto crcTable(){std::array<uint32_t,256> table{};for(uint32_t i=0;i<256;++i){uint32_t c=i;for(int bit=0;bit<8;++bit)c=(c>>1)^((c&1)?0xedb88320u:0);table[i]=c;}return table;}
uint32_t crcBytes(uint32_t c,const uint8_t* bytes,size_t count){static constexpr auto table=crcTable();for(size_t i=0;i<count;++i)c=table[(c^bytes[i])&255]^(c>>8);return c;}
std::string lower(std::string s){for(char& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s;}
struct FileClose {FILE* f;~FileClose(){if(f)std::fclose(f);}};
struct ZipEntry {std::string name;uint32_t compressed{},expanded{},crc{},offset{};uint16_t flags{},method{};bool directory{};};
bool PackageInstaller::extractZip(){
    FILE* file=std::fopen(temporarySng_.c_str(),"rb");FileClose close{file};
    if(!file||fseeko(file,0,SEEK_END)!=0)return fail("Cannot read ZIP package.");
    const off_t end=ftello(file);
    if(end<22||uint64_t(end)>MaxChartBytes)return fail("Invalid ZIP package size.");
    const uint64_t size=static_cast<uint64_t>(end);
    std::vector<uint8_t> tail(static_cast<size_t>(std::min<uint64_t>(size,65557)));
    if(!readAt(file,size-tail.size(),tail.data(),tail.size()))return fail("Truncated ZIP footer.");
    size_t eocd=tail.size();
    for(size_t at=tail.size()-22;;--at){if(u32(tail.data()+at)==0x06054b50&&at+22+u16(tail.data()+at+20)==tail.size()){eocd=at;break;}if(at==0)break;}
    if(eocd==tail.size())return fail("ZIP directory footer not found.");
    const auto* footer=tail.data()+eocd;
    const uint16_t count=u16(footer+10);
    const uint64_t cdSize=u32(footer+12),cdAt=u32(footer+16),footerAt=size-tail.size()+eocd;
    if(u16(footer+4)||u16(footer+6)||u16(footer+8)!=count||!count||count>MaxFileCount||cdAt==0xffffffffu||cdSize==0xffffffffu||!addWithin(cdAt,cdSize,footerAt)||cdAt+cdSize!=footerAt)
        return fail("Unsupported ZIP64, split or invalid ZIP directory.");
    uint64_t at=cdAt,expanded=0;
    std::vector<ZipEntry> entries;std::set<std::string> paths;
    for(unsigned i=0;i<count;++i){
        if(cancelled())return fail("Install cancelled.");
        std::array<uint8_t,46> header{};
        if(!addWithin(at,header.size(),cdAt+cdSize)||!readAt(file,at,header.data(),header.size())||u32(header.data())!=0x02014b50)return fail("Invalid ZIP directory entry.");
        const uint16_t nameSize=u16(header.data()+28),extra=u16(header.data()+30),comment=u16(header.data()+32);
        const uint64_t record=46ull+nameSize+extra+comment;
        if(!nameSize||nameSize>240||!addWithin(at,record,cdAt+cdSize))return fail("Invalid ZIP entry length.");
        ZipEntry entry;entry.name.resize(nameSize);
        if(!readAt(file,at+46,entry.name.data(),nameSize))return fail("Truncated ZIP filename.");
        std::replace(entry.name.begin(),entry.name.end(),'\\','/');
        entry.directory=entry.name.back()=='/';if(entry.directory)entry.name.pop_back();
        const uint32_t mode=u32(header.data()+38)>>16;
        if(!safeRelativePath(entry.name)||(mode&0170000)==0120000||u16(header.data()+34))return fail("Unsafe ZIP path, symlink or split entry.");
        if(!paths.insert(lower(entry.name)).second)return fail("Duplicate ZIP output path.");
        entry.flags=u16(header.data()+8);entry.method=u16(header.data()+10);entry.crc=u32(header.data()+16);
        entry.compressed=u32(header.data()+20);entry.expanded=u32(header.data()+24);entry.offset=u32(header.data()+42);
        if((entry.flags&~uint16_t(0x080e))||(entry.method!=0&&entry.method!=8)||entry.expanded>MaxSingleFileBytes||entry.compressed>MaxChartBytes||!addWithin(expanded,entry.expanded,MaxExpandedBytes)||entry.offset>=cdAt)
            return fail("Unsupported or oversized ZIP entry.");
        if(entry.directory&&(entry.expanded||entry.compressed))return fail("Invalid ZIP directory data.");
        expanded+=entry.expanded;entries.push_back(std::move(entry));at+=record;
    }
    if(at!=cdAt+cdSize)return fail("ZIP directory size differs.");
    std::array<uint8_t,65536> input{};std::array<uint8_t,TINFL_LZ_DICT_SIZE> dict{};
    for(const auto& entry:entries){
        if(cancelled())return fail("Install cancelled.");
        const std::string path=stageDirectory_+"/"+entry.name;
        if(entry.directory){if(!ensureDirectory(path.c_str()))return fail("Cannot create ZIP folder.");continue;}
        std::array<uint8_t,30> local{};
        if(!readAt(file,entry.offset,local.data(),local.size())||u32(local.data())!=0x04034b50||u16(local.data()+6)!=entry.flags||u16(local.data()+8)!=entry.method)return fail("ZIP local header differs.");
        const size_t nameSize=u16(local.data()+26);std::string localName(nameSize,'\0');
        if(nameSize>240||!readAt(file,entry.offset+30,localName.data(),nameSize))return fail("Invalid ZIP local filename.");
        std::replace(localName.begin(),localName.end(),'\\','/');
        const uint64_t dataAt=entry.offset+30ull+nameSize+u16(local.data()+28);
        if(localName!=entry.name||!addWithin(dataAt,entry.compressed,cdAt))return fail("ZIP data lies outside the package.");
        if(!(entry.flags&8)&&(u32(local.data()+14)!=entry.crc||u32(local.data()+18)!=entry.compressed||u32(local.data()+22)!=entry.expanded))return fail("ZIP local sizes differ.");
        if(!ensureDirectory(path.substr(0,path.find_last_of('/')).c_str()))return fail("Cannot create ZIP subfolder.");
        FILE* out=std::fopen(path.c_str(),"wb");FileClose output{out};if(!out)return fail("Cannot create extracted ZIP file.");
        uint64_t consumed=0,written=0;uint32_t crc=0xffffffffu;
        const auto emit=[&](const uint8_t* bytes,size_t n){if(!addWithin(written,n,entry.expanded))return false;crc=crcBytes(crc,bytes,n);written+=n;return std::fwrite(bytes,1,n,out)==n;};
        if(entry.method==0){
            if(entry.compressed!=entry.expanded)return fail("Stored ZIP sizes differ.");
            while(consumed<entry.compressed){
                if(cancelled())return fail("Install cancelled.");
                const size_t n=static_cast<size_t>(std::min<uint64_t>(input.size(),entry.compressed-consumed));
                if(!readAt(file,dataAt+consumed,input.data(),n)||!emit(input.data(),n))return fail("Cannot extract stored ZIP data.");consumed+=n;
            }
        }else{
            tinfl_decompressor inflater;tinfl_init(&inflater);
            size_t available=0,inAt=0,dictAt=0;tinfl_status status=TINFL_STATUS_NEEDS_MORE_INPUT;
            while(status!=TINFL_STATUS_DONE){
                if(cancelled())return fail("Install cancelled.");
                if(inAt==available&&consumed<entry.compressed){available=static_cast<size_t>(std::min<uint64_t>(input.size(),entry.compressed-consumed));inAt=0;if(!readAt(file,dataAt+consumed,input.data(),available))return fail("Truncated ZIP deflate stream.");consumed+=available;}
                size_t inSize=available-inAt,outSize=dict.size()-dictAt;
                status=tinfl_decompress(&inflater,input.data()+inAt,&inSize,dict.data(),dict.data()+dictAt,&outSize,consumed<entry.compressed?TINFL_FLAG_HAS_MORE_INPUT:0);
                inAt+=inSize;if(!emit(dict.data()+dictAt,outSize))return fail("ZIP output exceeds declared size or write failed.");
                dictAt=(dictAt+outSize)&(dict.size()-1);
                if(status<0||(!inSize&&!outSize&&status!=TINFL_STATUS_DONE)||(status==TINFL_STATUS_NEEDS_MORE_INPUT&&consumed==entry.compressed&&inAt==available))return fail("Invalid ZIP deflate stream.");
            }
            if(consumed-(available-inAt)!=entry.compressed)return fail("ZIP compressed length differs.");
        }
        if(written!=entry.expanded||(crc^0xffffffffu)!=entry.crc)return fail("ZIP file size or CRC differs.");
        if(std::fflush(out)!=0||fsync(fileno(out))!=0)return fail("Could not flush ZIP output.");
        extractedFiles.push_back({entry.name,entry.expanded});
    }
    return true;
}
} // namespace
void discardSongPackageStage(const std::string& stage) noexcept {if(!stage.empty())removeTree(stage.c_str());}
bool installSongPackage(const std::string& package,const std::string& stage,const std::string& destination,std::string& error,const std::atomic<bool>* cancel,const PackageMetadata* metadata) noexcept {
    error.clear();PackageInstaller installer{package,stage,error,cancel};
    std::array<uint8_t,6> magic{};FILE* file=std::fopen(package.c_str(),"rb");if(!file){error="Cannot read song package.";return false;}
    const bool read=std::fread(magic.data(),1,magic.size(),file)==magic.size();std::fclose(file);
    const bool extracted=read&&(std::memcmp(magic.data(),"SNGPKG",6)==0?installer.extractSng():u32(magic.data())==0x04034b50?installer.extractZip():installer.fail("The download is not an SNG or ZIP package."));
    if(!extracted){if(error.empty())error="Truncated song package.";return false;}
    // The verified archive index already identifies all regular outputs. Use
    // it instead of reopening/stat'ing the entire extracted directory tree.
    std::set<std::string> roots;
    for(const auto& file:installer.extractedFiles){
        if(file.name.rfind("__MACOSX/",0)==0)continue;
        const auto dot=file.name.find_last_of('.');if(dot==std::string::npos)continue;
        const auto ext=lower(file.name.substr(dot));
        if(ext==".chart"||ext==".mid"||ext==".midi"){
            const auto slash=file.name.find_last_of('/');roots.insert(slash==std::string::npos?std::string{}:file.name.substr(0,slash));
        }
    }
    if(roots.size()!=1){error="Package must contain exactly one song folder with a .chart or MIDI file.";return false;}
    const std::string relative=*roots.begin(),prefix=relative.empty()?std::string{}:relative+"/";
    const std::string root=stage+(relative.empty()?std::string{}:"/"+relative);
    std::string midi;bool chart=false;
    for(const auto& file:installer.extractedFiles){
        if(file.name.rfind(prefix,0)!=0)continue;
        const std::string name=file.name.substr(prefix.size());if(name.find('/')!=std::string::npos)continue;
        const std::string normalized=lower(name);const auto dot=normalized.find_last_of('.');if(dot==std::string::npos)continue;
        const std::string ext=normalized.substr(dot);
        if(ext==".chart"){
            if(!file.bytes||file.bytes>32*1024*1024){error="Chart exceeds the engine's supported file size.";return false;}
            chart=true;
        }
        if(ext==".mid"||ext==".midi")if(midi.empty()||normalized=="notes.mid")midi=root+"/"+name;
        if(normalized=="song.ini"&&name!=normalized&&rename((root+"/"+name).c_str(),(root+"/song.ini").c_str())!=0){error="Cannot normalize song metadata filename.";return false;}
    }
    FILE* ini=std::fopen((root+"/song.ini").c_str(),"rb");
    if(ini)std::fclose(ini);
    else if(metadata&&!writeIni((root+"/song.ini").c_str(),{{"name",metadata->title},{"artist",metadata->artist},{"charter",metadata->charter}})){error="Cannot create song metadata.";return false;}
    if(!chart&&(midi.empty()||!midiToChart(midi,root+"/notes.chart",error,cancel)))return false;
    if(installer.cancelled()){error="Install cancelled.";return false;}
    struct stat st{};if(stat(destination.c_str(),&st)==0){error="Song destination already exists.";return false;}
    if(rename(root.c_str(),destination.c_str())!=0){error="Could not commit the installed song folder.";return false;}
    if(root!=stage)removeTree(stage.c_str());
    return true;
}
} // namespace chorus
