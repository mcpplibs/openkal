#include <openkal/process.h>
#include <cstdio>
#include <cstdlib>
#include <string>
int main(int argc,char** argv) {
  if(argc==3&&std::string(argv[1])=="identity") {
    kal_process p{};kal_program_info i{};i.self_size=sizeof i;
    if(kal_process_open_id(std::stoull(argv[2]),&p)||kal_process_info(p,KAL_PINFO_ID|KAL_PINFO_STARTED,&i)||!i.started_ns)return 2;
    std::printf("%llu %llu\n",(unsigned long long)i.id,(unsigned long long)i.started_ns);kal_process_close(p);return 0;
  }
  if(argc!=3)return 3;
  kal_program_identity anchor{std::stoull(argv[1]),std::stoull(argv[2])};
  kal_job job{};char ref[512];kal_uintptr used=0;
  if(kal_process_job_create(&job,ref,sizeof ref,&used))return 4;
  std::puts("READY");std::fflush(stdout);char ack=0;if(kal_stream_read(kal_stdin(),&ack,1)!=1)return 5;
  kal_u32 state=99;const auto rc=kal_process_job_retire(job,ref,used,&anchor,&state);
  std::printf("RESULT %d %u\n",rc,state);std::fflush(stdout);
  if(!rc&&state==KAL_JOB_EMPTY) {
    const auto ended=kal_process_job_terminate(job);
    std::printf("OLD_UNIT_END %d\n",ended);std::fflush(stdout);
  }
  kal_process_job_close(job);return 0;
}
