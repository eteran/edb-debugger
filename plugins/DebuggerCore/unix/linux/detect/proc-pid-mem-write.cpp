#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char** argv);

// Custom class to work with files, since various wrappers
// appear to be unreliable to check whether writes succeeded
class File
{
    int fd;
    bool success;
public:
    File(const std::string& filename)
    {
        fd=open(filename.c_str(),O_RDWR);
        success=fd!=-1;
    }
    ssize_t write(const void* buf, size_t count)
    {
        const auto result=::write(fd,buf,count);
        success=result!=-1;
        return result;
    }
    ssize_t read(void* buf, size_t count)
    {
        const auto result=::read(fd,buf,count);
        success=result!=-1;
        return result;
    }
    size_t seekp(size_t offset)
    {
        const auto result=::lseek(fd,offset,SEEK_SET);
        success=result!=-1;
        return result;
    }
    ~File()
    { close(fd); }
    explicit operator bool()
    { return success; }
};

static const char* headerName=nullptr;
void writeHeader(bool broken)
{
    std::ofstream file(headerName);
    file << "#define PROC_PID_MEM_WRITE_BROKEN " << std::boolalpha << broken << "\n";
}

bool detectAndWriteHeader(std::string progName)
{
    switch(pid_t pid = fork()) {
    case 0:
        if(ptrace(PTRACE_TRACEME)<0)
        {
            perror((progName+": child: PTRACE_TRACEME failed").c_str());
            abort();
        }

        execl(progName.c_str(),progName.c_str(),"/dev/null","child",nullptr);
        perror((progName+"child: execl() returned").c_str());
        abort();
		
    case -1:
        perror("fork");
        return false;
		
    default:    
        {
            int status;
            if(waitpid(pid,&status,__WALL)==-1)
            {
                perror((progName+": parent: waitpid failed").c_str());
                return false;
            }

            if(!WIFSTOPPED(status)||WSTOPSIG(status)!=SIGTRAP)
            {
                std::cerr << progName << ": unexpected status returned by waitpid: 0x"
                          << std::hex << status << "\n";
                return false;
            }

            File file("/proc/"+std::to_string(pid)+"/mem");
            if(!file)
            {
                perror((progName+": failed to open memory file").c_str());
                return false;
            }

            const auto pageAlignMask=~(sysconf(_SC_PAGESIZE)-1);
            const auto addr=reinterpret_cast<std::size_t>(&main) & pageAlignMask;
            file.seekp(addr);
            if(!file)
            {
                perror((progName+": failed to seek to address to read").c_str());
                return false;
            }

            int buf=0x12345678;
            {
                file.read(&buf,sizeof(buf));
                if(!file)
                {
                    perror((progName+": failed to read data from child's memory, won't even try to write").c_str());
                    return false;
                }
            }

            file.seekp(addr);
            if(!file)
            {
                perror((progName+": failed to seek to address to write").c_str());
                return false;
            }

            {
                file.write(&buf,sizeof(buf));
                if(!file) {
                    writeHeader(true);
                } else {
                    writeHeader(false);
                }
            }

            if(kill(pid,SIGKILL)==-1)
            {
                perror((progName+": warning: failed to kill child").c_str());
                return false;
            }

            return true;
        }
        break;
    }
}

int main(int argc, char** argv)
{
    if(argc>2)
    {
        for(;;) sleep(10);
        abort();
    }

    if(argc!=2)
    {
        std::cerr << "Usage: " << argv[0] << " headerFileName.h\n";
        return 1;
    }
    headerName=argv[1];

    if(!detectAndWriteHeader(argv[0]))
    {
        std::cerr << "Warning: failed to detect whether write to /proc/PID/mem is broken. Assuming it's not.\n";
        writeHeader(false);
    }
}
