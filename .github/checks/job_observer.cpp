// Exercises the implemented C ABI, including a driver with a descendant.
#include <openkal/process.h>
#include <openkal/env.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#ifdef __linux__
#include <sys/prctl.h>
#include <sys/wait.h>
#endif
using namespace std::chrono_literals;
namespace fs = std::filesystem;
void require(bool value,const char* name) {
    std::printf("{\"check\":\"%s\",\"ok\":%s}\n",name,value?"true":"false");
    std::fflush(stdout); if(!value) throw std::runtime_error(name);
}
std::string image, relative_image;
kal_spawn place() {
    kal_spawn how{};
    for(kal_uintptr i=0;i<kal_fs_preopen_count();++i) {
        char name[4096]{}; kal_uintptr n=0; kal_dir d{};
        if(kal_fs_preopen(i,&d,name,sizeof name,&n)||n>image.size())continue;
        bool same=true;
        for(kal_uintptr j=0;j<n;++j) {
            char a=name[j],b=image[j];
#ifdef _WIN32
            if(a>='A'&&a<='Z')a+=32; if(b>='A'&&b<='Z')b+=32;
#endif
            if(a!=b)same=false;
        }
        if(same && (!how.base.h || n==1 || image[n-1]=='/' || image[n-1]=='\\')) {
            how.base=how.work=d; relative_image=image.substr(n);
            while(!relative_image.empty()&&(relative_image.front()=='/'||relative_image.front()=='\\'))relative_image.erase(0,1);
            return how;
        }
    }
    throw std::runtime_error("program_preopen");
}
kal_process start(std::vector<std::string> arguments,kal_job* job=nullptr,
                  const kal_spawn_streams* streams=nullptr) {
    arguments.insert(arguments.begin(),image);
    std::vector<const char*> argv;std::vector<kal_uintptr> lengths;
    for(auto& s:arguments){argv.push_back(s.data());lengths.push_back(s.size());}
    auto how=place();how.job=job;
    kal_process child{};
    // An absolute name is resolved by the backend against the directory held.
    const int rc=kal_process_spawn(&how,relative_image.data(),relative_image.size(),argv.data(),lengths.data(),argv.size(),
        nullptr,nullptr,0,streams,&child);
    if(rc)throw std::runtime_error("actual_spawn_"+std::to_string(rc));return child;
}
std::string read_line(kal_stream stream) {
    std::string out;char c=0;
    while(out.size()<512) {
        const auto n=kal_stream_read(stream,&c,1);
        if(n!=1)throw std::runtime_error("control_stream_eof");
        if(c=='\n')return out;out+=c;
    }
    throw std::runtime_error("control_stream_bound");
}
void send_line(kal_stream stream,const std::string& value) {
    const auto bytes=value+"\n";
    if(kal_stream_write(stream,bytes.data(),bytes.size())!=static_cast<kal_intptr>(bytes.size()))
        throw std::runtime_error("control_write");
}
int wait(kal_process child,bool success=true) {
    int status=0,environment=0;
    if(kal_process_wait(child,&status,&environment))throw std::runtime_error("actual_wait");
    kal_process_close(child);
    if(success && (status||environment))throw std::runtime_error("actual_child_exit_"+std::to_string(status));
    return environment ? 128+status : status;
}
kal_u32 state(kal_job job,const std::string& ref) {
    kal_u32 value=KAL_JOB_UNKNOWN;
    if(kal_process_job_observe(job,ref.data(),ref.size(),&value))return KAL_JOB_UNKNOWN;
    return value;
}
void await_empty(kal_job job,const std::string& ref) {
    const auto limit=std::chrono::steady_clock::now()+5s;
    for(;;) {
#ifdef __linux__
        while(waitpid(-1,nullptr,WNOHANG)>0){}
#endif
        if(state(job,ref)==KAL_JOB_EMPTY)return;
        if(std::chrono::steady_clock::now()>=limit)throw std::runtime_error("whole_unit_not_quiet");
        std::this_thread::sleep_for(1ms);
    }
}
int helper(const std::string& mode,const fs::path& output) {
    kal_job job{};char bytes[512]{};kal_uintptr used=0;
    if(kal_process_job_create(&job,bytes,sizeof bytes,&used))return 41;
    const std::string ref(bytes,used);
    send_line(kal_stdout(),ref);
    if(read_line(kal_stdin())!="ACK")return 42;
    kal_stream read{},write{};
    if(kal_process_channel(&read,&write))return 43;
    kal_spawn_streams streams{read,{}, {}};
    auto driver=start({"driver",output.string()},&job,&streams);
    kal_process_channel_close(read);
    kal_u32 retired=KAL_JOB_UNKNOWN;
    if(kal_process_job_retire(job,ref.data(),ref.size(),&retired)||retired!=KAL_JOB_OCCUPIED)return 44;
    if(mode=="kill") {
        send_line(kal_stdout(),"BUSY");
        // Only the parent owns the acknowledgement's write end. Hard death
        // also closes this helper's writer gate, allowing the orphan to finish.
        (void)read_line(kal_stdin());return 45;
    }
    if(kal_stream_write(write,"R",1)!=1)return 46;
    kal_process_channel_close(write);wait(driver);
    if(kal_process_job_retire(job,ref.data(),ref.size(),&retired)||retired!=KAL_JOB_EMPTY)return 47;
    if(kal_process_job_retire(job,ref.data(),ref.size(),&retired)||retired!=KAL_JOB_EMPTY)return 48;
    send_line(kal_stdout(),"DONE");kal_process_job_close(job);return 0;
}
void exercise(const fs::path& root,const std::string& mode) {
    kal_stream input{},gate{},results{},output{};
    if(kal_process_channel(&input,&gate)||kal_process_channel(&results,&output))throw std::runtime_error("actual_channels");
    kal_spawn_streams streams{input,output,{}};
    auto child=start({"helper",mode,(root/(mode+".object")).string()},nullptr,&streams);
    kal_process_channel_close(input);kal_process_channel_close(output);
    const auto ref=read_line(results);kal_job observed{};
    require(kal_process_job_open(ref.data(),ref.size(),&observed)==kal_ok,"original_unit_retained_before_admission");
    require(state(observed,ref)==KAL_JOB_OCCUPIED,"unstarted_helper_is_occupied");
    auto bad=ref;bad[0]='?';require(state(observed,bad)!=KAL_JOB_EMPTY,"malformed_reference_never_grants_quiet");
    if(mode=="before") {
        require(kal_process_terminate(child)==kal_ok,"controller_ended_before_ack");
        kal_process_channel_close(gate);(void)wait(child,false);
        require(!fs::exists(root/(mode+".object")),"no_writer_before_ack");
    } else {
        send_line(gate,"ACK");
        if(mode=="kill") {
            require(read_line(results)=="BUSY","retirement_detects_external_driver");
            require(kal_process_terminate(child)==kal_ok,"controller_ended_with_descendant");
            kal_process_channel_close(gate);(void)wait(child,false);
        } else {
            require(read_line(results)=="DONE","external_members_quiet_before_completion");
            kal_process_channel_close(gate);wait(child);
        }
    }
    await_empty(observed,ref);
    require(mode=="before"||fs::exists(root/(mode+".object")),"all_admitted_writer_outputs_observed");
    require(state(observed,ref)==KAL_JOB_EMPTY,"held_original_unit_positively_empty");
    kal_process_job_close(observed);kal_process_channel_close(results);
    kal_job recovered{};const int opened=kal_process_job_open(ref.data(),ref.size(),&recovered);
#ifdef _WIN32
    require(opened!=kal_ok,"lost_windows_native_reference_is_not_recreated");
#else
    require(opened==kal_ok&&state(recovered,ref)==KAL_JOB_EMPTY,"expired_native_group_is_positively_empty");
#endif
    if(opened==kal_ok)kal_process_job_close(recovered);
}
int main(int argc,char** argv) {
    try {
        image=fs::absolute(argv[0]).string();
        if(argc>2&&std::string(argv[1])=="writer") {
            char gate=0;const auto n=kal_stream_read(kal_stdin(),&gate,1);
            if(n<0)return 50;
            std::ofstream out(argv[2]);out<<"actual declared unit descendant\n";return out?0:51;
        }
        if(argc>2&&std::string(argv[1])=="driver") {return wait(start({"writer",argv[2]}));}
        if(argc>3&&std::string(argv[1])=="helper")return helper(argv[2],argv[3]);
#ifdef __linux__
        require(prctl(PR_SET_CHILD_SUBREAPER,1)==0,"native_harness_reaps_fault_orphans");
#endif
        const auto root=fs::temp_directory_path()/("job-api-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(root);
        try {exercise(root,"normal");exercise(root,"before");exercise(root,"kill");}
        catch(...){fs::remove_all(root);throw;}
        fs::remove_all(root);return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"job observer API: %s\n",e.what());return 1;}
}
