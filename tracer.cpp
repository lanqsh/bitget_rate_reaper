#include "tracer.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>

#include <string>
#include <type_traits>
#include <vector>

#include "Poco/Any.h"
#include "Poco/DateTime.h"
#include "Poco/DateTimeFormat.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/Format.h"
#include "Poco/Foundation.h"
#include "Poco/LocalDateTime.h"
#include "Poco/NumberFormatter.h"
#include "Poco/SplitterChannel.h"
#include "Poco/Thread.h"
#include "Poco/Timestamp.h"
#include "Poco/Timezone.h"

using Poco::AutoPtr;
using Poco::ConsoleChannel;
using Poco::File;
using Poco::FileChannel;
using Poco::FormattingChannel;
using Poco::Logger;
using Poco::Message;
using Poco::PatternFormatter;
using Poco::SimpleFileChannel;
using Poco::SplitterChannel;

namespace logger {
std::vector<std::string> Tracer::logger_;

void Tracer::Init() { Init(DEFAULT_LOGGER_NAME, "default.log"); }

void Tracer::Init(const std::string &name, const std::string &path) {
  Init(name, path, "100M");
}

void Tracer::Init(const std::string &name, const std::string &path,
                  const std::string &file_size) {
  AutoPtr<SplitterChannel> pSplitter(new SplitterChannel());
  AutoPtr<PatternFormatter> pPF(new PatternFormatter);
  pPF->setProperty("pattern", "%Y-%m-%d %H:%M:%S.%i [%T] [%U %u] %p - %t");
  pPF->setProperty("times", "local");

#ifdef ENABLE_CONSOLE_LOG
  AutoPtr<ConsoleChannel> pConsoleChannel(new ConsoleChannel());
  AutoPtr<FormattingChannel> pFCConsole(
      new FormattingChannel(pPF, pConsoleChannel));
  pSplitter->addChannel(pFCConsole);
#endif

  AutoPtr<FileChannel> pFileCh(new FileChannel(path));
  pFileCh->setProperty("rotation", file_size);
  pFileCh->setProperty("purgeCount", "10");
  pFileCh->open();

  AutoPtr<FormattingChannel> pFCFile(new FormattingChannel(pPF));
  pFCFile->setChannel(pFileCh);
  pSplitter->addChannel(pFCFile);

  try {
    Logger::create(name, pSplitter, Message::PRIO_NOTICE);
  } catch (Poco::Exception &e) {
    printf("logger %s error: %s\n", name.c_str(), e.what());
  }
  logger_.emplace_back(name);
}

void Tracer::SetLevel(const std::string &level) {
  for (auto logger : logger_) {
    SetLevel(level, logger);
  }
}

void Tracer::SetLevel(const std::string &level, const std::string &name) {
  Poco::Message::Priority p(Message::PRIO_NOTICE);
  if (level == "trace") {
    p = Message::PRIO_TRACE;
  } else if (level == "debug") {
    p = Message::PRIO_DEBUG;
  } else if (level == "information") {
    p = Message::PRIO_INFORMATION;
  } else if (level == "notice") {
    p = Message::PRIO_NOTICE;
  } else if (level == "warning") {
    p = Message::PRIO_WARNING;
  } else if (level == "error") {
    p = Message::PRIO_ERROR;
  } else if (level == "fatal") {
    p = Message::PRIO_FATAL;
  } else {
    return;
  }

  Poco::Logger::get(name).setLevel(p);
}

int Tracer::GetLevel() { return GetLevel(DEFAULT_LOGGER_NAME); }

int Tracer::GetLevel(const std::string &name) {
  return Poco::Logger::get(name).getLevel();
}

}  // namespace logger
