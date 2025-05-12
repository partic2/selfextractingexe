
#include <iostream>
#include <fstream>
#include <queue>
#include <vector>
#include <string>
#include <filesystem>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;


static const char *_magicStringConst="6s7zJa74UCTAwj59gAxIked5htAEDkVf";
static char _magicString[32];
static const char _magicXOR=0x5A;

static void processMagic(){
    for(int i1=0;i1<32;i1++){
        _magicString[i1]=_magicStringConst[i1]^_magicXOR;
    }
}


struct FileInfo {
    std::string name;
    uint64_t size;
    uint64_t offset;
};

fs::path *thisExe;
const int BUFFER_SIZE=4096;

void transferData(std::ifstream& input, std::ofstream& output) {

    std::vector<char> buffer(BUFFER_SIZE);
    uint64_t count=0;
    while (1) {
        input.read(buffer.data(), BUFFER_SIZE);
        std::streamsize bytesRead = input.gcount();
        count+=bytesRead;
        output.write(buffer.data(), bytesRead);
        if(input.eof() || !input.good()){
            break;
        };
    }
}

#ifdef _WIN32
std::string ConvertToUTF8(const std::string& str) {
    int wlen = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, nullptr, 0);
    std::wstring wstr(wlen, 0);
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, wstr.data(), wlen);
    
    int ulen = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8str(ulen, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, utf8str.data(), ulen, nullptr, nullptr);

    utf8str.resize(ulen - 1);
    return utf8str;
}
std::wstring U8ToWchar(const std::string& str){
    int wlen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring wstr(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wstr.data(), wlen);
    return wstr;
}
#endif

std::vector<size_t> findPatternInStream(std::ifstream& file, const std::vector<char>& pattern,int count) {
    std::vector<size_t> positions;
    std::vector<char> buffer(BUFFER_SIZE + pattern.size() - 1);
    size_t global_pos = 0;

    if (pattern.empty()) return positions;

    file.read(buffer.data(), BUFFER_SIZE);
    size_t bytes_read = file.gcount();

    while (bytes_read >= pattern.size()) {
        auto it = buffer.begin();
        while ((it = std::search(it, buffer.begin() + bytes_read, pattern.begin(), pattern.end())) != buffer.begin() + bytes_read) {
            size_t pos = global_pos + (it - buffer.begin());
            positions.push_back(pos);
            it += pattern.size();
            count--;
            if(count==0){
                break;
            }
        }
        if(count==0){
            break;
        }
        size_t overlap = pattern.size() - 1;
        std::copy(buffer.end() - overlap, buffer.end(), buffer.begin());

        global_pos += bytes_read - overlap;
        file.read(buffer.data() + overlap, BUFFER_SIZE);
        bytes_read = file.gcount() + overlap;
    }

    return positions;
}

void packFiles() {
    fs::path outputExe(*thisExe);
    outputExe.replace_filename(outputExe.stem().string()+"_packed"+outputExe.extension().string());

    std::vector<FileInfo> files;
    
    uint64_t currentOffset = 0;
    auto thisDir=thisExe->parent_path();
    if(thisDir==""){
        thisDir=".";
    }
    std::queue<fs::path> dirs;
    dirs.push(thisDir);
    
    while(!dirs.empty()){
        auto dir=dirs.front();
        dirs.pop();
        for (const auto& entry : fs::directory_iterator(dir)) {
            if(dir==thisDir && (entry.path().filename() == outputExe.filename() || entry.path().filename()==thisExe->filename())){
                continue;
            }
            if(entry.is_symlink()){
                continue;
            }
            if (entry.is_regular_file()) {
                std::ifstream file(entry.path(), std::ios::binary);
                if (file) {
                    file.seekg(0, std::ios::end);
                    uint64_t size = file.tellg();
                    file.seekg(0, std::ios::beg);
                    
                    std::vector<char> buffer(size);
                    file.read(buffer.data(), size);
                    
                    
                    files.push_back({
                        entry.path().string().substr(thisDir.string().length()+1),
                        size,
                        currentOffset
                    });
                    
                    currentOffset += size;
                }
            }else if(entry.is_directory()){
                dirs.push(entry.path());
            }
        }
    }
    
    
    std::ofstream exe(outputExe, std::ios::binary);
    if (!exe) {
        std::cerr << "Failed to create output file." << std::endl;
        return;
    }

    std::ifstream thisExeInput(*thisExe,std::ios::binary);
    std::cout<<"Writing copy of self..."<<std::endl;
    transferData(thisExeInput,exe);
    
    exe.write(_magicString, 32);
    uint32_t fileCount = files.size();
    exe.write(reinterpret_cast<char*>(&fileCount), sizeof(fileCount));
    
    for (const auto& file : files) {
        uint16_t nameLength = file.name.size();
        exe.write(reinterpret_cast<char*>(&nameLength), sizeof(nameLength));
        exe.write(file.name.c_str(), nameLength);
        
        exe.write(reinterpret_cast<const char*>(&file.size), sizeof(file.size));
        exe.write(reinterpret_cast<const char*>(&file.offset), sizeof(file.offset));
    }
    
    const char* dataMarker = "DATASECTION";
    exe.write(dataMarker, strlen(dataMarker));
    
    for(const auto& file : files){
        std::ifstream filedata(thisDir/file.name,std::ios::binary);
        std::cout<<"Packing file "<<file.name<<std::endl;
        transferData(filedata,exe);
    }
    exe.write(_magicString, 32);
    exe.flush();
    std::cout << "Successfully packed " << files.size() << " files into " << outputExe << std::endl;
}

const char *extractFiles() {
    const fs::path& inputExe=*thisExe;
    auto thisDir=thisExe->parent_path();
    if(thisDir==""){
        thisDir=".";
    }
    std::ifstream exe(inputExe, std::ios::binary);
    auto foundMagic=findPatternInStream(exe,std::vector<char>(_magicString,_magicString+32),1);
    if (foundMagic.size()==0) {
        packFiles();
        return "No magic found, pack mode";
    }
    exe.seekg(foundMagic.at(0)+32,std::ios::beg);
    
    uint32_t fileCount = 0;
    exe.read(reinterpret_cast<char*>(&fileCount), sizeof(fileCount));
    
    std::vector<FileInfo> files;
    
    for (uint32_t i = 0; i < fileCount; ++i) {
        FileInfo file;
        
        uint16_t nameLength = 0;
        exe.read(reinterpret_cast<char*>(&nameLength), sizeof(nameLength));
        
        char* nameBuffer = new char[nameLength + 1];
        exe.read(nameBuffer, nameLength);
        nameBuffer[nameLength] = '\0';
        file.name = nameBuffer;
        delete[] nameBuffer;
        
        exe.read(reinterpret_cast<char*>(&file.size), sizeof(file.size));
        exe.read(reinterpret_cast<char*>(&file.offset), sizeof(file.offset));
        
        files.push_back(file);
    }
    
    char dataMarker[12] = {0};
    exe.read(dataMarker, 11);
    if (strcmp(dataMarker, "DATASECTION") != 0) {
        std::cerr << "Invalid data section marker." << std::endl;
        return "Invalid data section marker.";
    }
    
    uint64_t dataStart = exe.tellg();
    
    for (const auto& file : files) {
        std::cout << "Extracting: " << file.name << " (" << file.size << " bytes)" << std::endl;
        
        exe.seekg(dataStart + file.offset, std::ios::beg);
        
        std::vector<char> buffer(file.size);
        exe.read(buffer.data(), file.size);
        
        size_t lastSlash = file.name.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            std::string dirPath = file.name.substr(0, lastSlash);
            fs::create_directories(dirPath);
        }
        
        std::ofstream outFile(thisDir/std::string(file.name), std::ios::binary);
        if (outFile) {
            outFile.write(buffer.data(), buffer.size());
        } else {
            std::cerr << "Failed to create file: " << file.name << std::endl;
        }
    }
    
    std::cout << "Successfully extracted " << files.size() << " files." << std::endl;

    fs::path autorun(thisDir/"autorun");
    if(fs::is_regular_file(autorun)){
        std::ifstream autorunIn(autorun,std::ios::binary);
        autorunIn.seekg(0,std::ios::end);
        std::string buffer(uint32_t(autorunIn.tellg())+1,0);
        autorunIn.seekg(0,std::ios::beg);
        autorunIn.read(buffer.data(),buffer.size());
        #ifdef _WIN32
        std::wstring wpath=U8ToWchar(thisDir.string());
        _wchdir(wpath.c_str());
        #else
        chdir(thisDir.string().c_str());
        #endif
        system(buffer.c_str());
    }
    return nullptr;
}



int main(int argc,char *argv[]){
    processMagic();
    #ifdef _WIN32
    thisExe=new fs::path(ConvertToUTF8(argv[0]));
    #else
    thisExe=new fs::path(argv[0]);
    #endif
    if(!fs::is_regular_file(*thisExe)){
        thisExe->replace_filename(thisExe->filename().string()+".exe");
    }
    if(!fs::is_regular_file(*thisExe)){
        std::cerr<<"Failed to open executing file."<<std::endl;
        return 1;
    }
    auto thisDir=thisExe->parent_path();
    if(thisDir==""){
        thisDir=".";
    }
    if(argc==1){
        extractFiles();
    }else if(strcmp(argv[1],"pack")==0){
        packFiles();
    }
}

