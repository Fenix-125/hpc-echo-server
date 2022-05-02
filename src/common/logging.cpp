#include "common/logging.h"
#include <filesystem>

#include "common/defines.h"


void logging_init(int *argc, char **argv[]) {
    google::InitGoogleLogging((*argv)[0]);
    google::SetStderrLogging(google::GLOG_INFO);
    FLAGS_logbufsecs = 0; // realtime output
    FLAGS_stop_logging_if_full_disk = true;
    auto log_path = std::filesystem::absolute(SERVER_DEFAULT_LOG_DIR);
    std::filesystem::create_directories(log_path);
    FLAGS_log_dir = log_path.string();
    LOG(INFO) << "Logging directory: " << log_path;
    google::ParseCommandLineFlags(argc, argv, false);
}

void logging_deinit() {
    google::ShutdownGoogleLogging();
}

void set_log_severity(severity_t severity) {
    if (severity > google::GLOG_FATAL) {
        severity = google::GLOG_FATAL;
    }
    if (severity < google::GLOG_INFO) {
        severity = google::GLOG_INFO;
    }
    FLAGS_minloglevel = severity;
}
