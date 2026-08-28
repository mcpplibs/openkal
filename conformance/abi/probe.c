/* One binary, built once, run against two implementations.
 *
 * It is a C program written directly against openkal --- not against a C library
 * above it --- because the property under examination is about this interface
 * and not about anything built upon it.
 *
 * It is linked against `libopenkal.so' by SONAME and against no implementation
 * by name, so which implementation it meets is decided when it is RUN. That is
 * the whole of the arrangement: everything else in this repository decides it
 * when the artifact is built, and therefore cannot observe what happens when the
 * two are not the same choice.
 *
 * The programme prints what it was told. The script that runs it decides whether
 * that is what the specification says, because the two runs expect DIFFERENT
 * answers and a programme that knew which one it was in would be answering from
 * its own knowledge rather than from the implementation's. */
#include <openkal.h>
#include <stdio.h>
#include <string.h>

static const char* yn(int b) { return b ? "yes" : "no"; }

int main(void) {
    printf("version         %llu.%llu.%llu\n",
           (unsigned long long)(kal_version() >> 32),
           (unsigned long long)((kal_version() >> 16) & 0xffffu),
           (unsigned long long)(kal_version() & 0xffffu));

    /* ⭐ THE FLOOR. A consumer holds declarations of one version and may meet an
     * implementation of another; an older one reports conditions this consumer
     * distinguishes as conditions it does not, which is a wrong answer rather
     * than a refusal. */
    printf("satisfies-floor %s\n", yn(kal_version() >= KAL_VERSION));

    printf("granularity     %llu\n", (unsigned long long)kal_memory_granularity());

    const kal_u64 have = kal_interfaces();
    printf("has-space       %s\n", yn((have & KAL_IFACE_SPACE) != 0));
    printf("has-exec        %s\n", yn((have & KAL_IFACE_EXEC) != 0));
    printf("has-fs          %s\n", yn((have & KAL_IFACE_FS) != 0));

    /* ⚠️ AND THE WORD IS CHECKED AGAINST WHAT IS ACTUALLY THERE. A word that
     * claimed an interface the object does not export would mislead exactly the
     * consumer that has no linker to ask --- which is this one. */
    printf("exec-available  %s\n",
           yn((have & KAL_IFACE_EXEC) != 0
              && (kal_exec_props() & KAL_EXEC_PROP_AVAILABLE) != 0));

    /* An enquiry, and whether the implementation knows this node from another. */
    struct kal_dir d = { 0 };
    kal_uintptr nlen = 0;
    if (kal_fs_preopen(0, &d, NULL, 0, &nlen) == kal_ok) {
        struct kal_node_info info;
        memset(&info, 0, sizeof info);
        info.self_size = sizeof info;
        const int e = kal_fs_info(d, ".", 1, 0, KAL_INFO_ALL, &info);
        printf("enquiry         %s\n", e == kal_ok ? "answered" : "refused");
        printf("knows-identity  %s\n", yn((info.present & KAL_INFO_IDENTITY) != 0));
        printf("kind-is-dir     %s\n", yn(info.kind == kal_node_directory));
    } else {
        printf("enquiry         no-preopen\n");
    }
    return 0;
}
